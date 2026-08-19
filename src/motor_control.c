#include "motor_control.h"

#include <math.h>
#include <stdint.h>

#include "encoder.h"
#include "motor_state.h"
#include "pwm.h"

#define MOTOR_CONTROL_ONE_OVER_SQRT3           0.5773502692f
#define MOTOR_CONTROL_SQRT3_OVER_2             0.8660254038f
#define MOTOR_CONTROL_TWO_PI                   6.2831853072f
#define MOTOR_CONTROL_ADC_MAX_COUNTS           4095.0f
#define MOTOR_CONTROL_ADC_REFERENCE_VOLTAGE    3.33f
#define MOTOR_CONTROL_CURRENT_SENSE_GAIN_V_PER_A 0.1f
#define MOTOR_CONTROL_CURRENT_A_SCALE \
    (MOTOR_CONTROL_ADC_REFERENCE_VOLTAGE / \
     (MOTOR_CONTROL_ADC_MAX_COUNTS * MOTOR_CONTROL_CURRENT_SENSE_GAIN_V_PER_A))
#define MOTOR_CONTROL_CURRENT_C_SCALE MOTOR_CONTROL_CURRENT_A_SCALE
#define MOTOR_CONTROL_CURRENT_A_SIGN           1.0f
#define MOTOR_CONTROL_CURRENT_C_SIGN           1.0f
#define MOTOR_CONTROL_ENCODER_CPR              4096.0f
#define MOTOR_CONTROL_POLE_PAIRS               7.0f
#define MOTOR_CONTROL_PHASE_RESISTANCE_OHM     2.55f
#define MOTOR_CONTROL_PHASE_INDUCTANCE_H       0.00086f
#define MOTOR_CONTROL_FLUX_LINKAGE_WB          0.0035f
#define MOTOR_CONTROL_CURRENT_LOOP_BW_HZ       2000.0f
#define MOTOR_CONTROL_KP_CURRENT \
    (MOTOR_CONTROL_PHASE_INDUCTANCE_H * \
     (MOTOR_CONTROL_TWO_PI * MOTOR_CONTROL_CURRENT_LOOP_BW_HZ))
#define MOTOR_CONTROL_KI_CURRENT \
    (MOTOR_CONTROL_PHASE_RESISTANCE_OHM * \
     (MOTOR_CONTROL_TWO_PI * MOTOR_CONTROL_CURRENT_LOOP_BW_HZ))
#define MOTOR_CONTROL_BUS_VOLTAGE_NOMINAL      12.0f
#define MOTOR_CONTROL_VOLTAGE_LIMIT_RATIO      0.95f
#define MOTOR_CONTROL_OFFSET_SAMPLE_COUNT      4096UL
#define MOTOR_CONTROL_ALIGN_DURATION_S         0.20f
#define MOTOR_CONTROL_ALIGN_VOLTAGE_V          1.0f
#define MOTOR_CONTROL_DEFAULT_ID_REF_A         0.0f
#define MOTOR_CONTROL_DEFAULT_IQ_REF_A         0.0f

typedef struct
{
    MotorControlStartupState startupState;
    bool outputEnabled;
    bool manualDutyModeEnabled;
    uint32_t offsetSampleCounter;
    uint32_t alignSampleCounter;
    uint32_t alignSampleTarget;
    uint32_t previousEncoderPosition;
    float currentOffsetASum;
    float currentOffsetCSum;
    float currentLoopFrequencyHz;
    float currentLoopTs;
    float currentKiDiscrete;
    float electricalZeroOffsetRad;
    float filteredMechanicalSpeedRadPerSec;
    float integralD;
    float integralQ;
    float manualDutyA;
    float manualDutyB;
    float manualDutyC;
} MotorControlLoopState;

static MotorControlLoopState motorControlLoopState = {0};

static float clampf(float value, float minimum, float maximum)
{
    if(value < minimum)
    {
        return minimum;
    }

    if(value > maximum)
    {
        return maximum;
    }

    return value;
}

static float wrapAngle(float angle)
{
    while(angle >= MOTOR_CONTROL_TWO_PI)
    {
        angle -= MOTOR_CONTROL_TWO_PI;
    }

    while(angle < 0.0f)
    {
        angle += MOTOR_CONTROL_TWO_PI;
    }

    return angle;
}

static float mechanicalAngleFromCount(uint32_t encoderCount)
{
    return ((float)(encoderCount % (uint32_t)MOTOR_CONTROL_ENCODER_CPR) /
            MOTOR_CONTROL_ENCODER_CPR) * MOTOR_CONTROL_TWO_PI;
}

static float electricalAngleFromEncoder(void)
{
    return wrapAngle((mechanicalAngleFromCount(g_motorControlState.encoder_position) *
                      MOTOR_CONTROL_POLE_PAIRS) -
                     motorControlLoopState.electricalZeroOffsetRad);
}

static float updateMechanicalSpeedEstimate(void)
{
    int32_t deltaCount;
    float instantaneousMechanicalSpeed;

    deltaCount = (int32_t)g_motorControlState.encoder_position -
                 (int32_t)motorControlLoopState.previousEncoderPosition;
    motorControlLoopState.previousEncoderPosition =
        g_motorControlState.encoder_position;

    instantaneousMechanicalSpeed =
        ((float)deltaCount * MOTOR_CONTROL_TWO_PI *
         motorControlLoopState.currentLoopFrequencyHz) /
        MOTOR_CONTROL_ENCODER_CPR;

    motorControlLoopState.filteredMechanicalSpeedRadPerSec =
        (0.95f * motorControlLoopState.filteredMechanicalSpeedRadPerSec) +
        (0.05f * instantaneousMechanicalSpeed);

    g_motorControlState.mechanical_speed_rad_per_sec =
        motorControlLoopState.filteredMechanicalSpeedRadPerSec;
    g_motorControlState.electrical_speed_rad_per_sec =
        g_motorControlState.mechanical_speed_rad_per_sec *
        MOTOR_CONTROL_POLE_PAIRS;

    return g_motorControlState.electrical_speed_rad_per_sec;
}

static float getBusVoltage(void)
{
    return MOTOR_CONTROL_BUS_VOLTAGE_NOMINAL;
}

static void setVoltageVector(float vAlpha, float vBeta, float busVoltage)
{
    float vPhaseA;
    float vPhaseB;
    float vPhaseC;
    float phaseLimit;
    float maxMagnitude;
    float dutyA;
    float dutyB;
    float dutyC;

    phaseLimit = 0.5f * busVoltage * MOTOR_CONTROL_VOLTAGE_LIMIT_RATIO;
    maxMagnitude = sqrtf((vAlpha * vAlpha) + (vBeta * vBeta));

    if((maxMagnitude > phaseLimit) && (maxMagnitude > 0.0f))
    {
        float scale = phaseLimit / maxMagnitude;
        vAlpha *= scale;
        vBeta *= scale;
    }

    vPhaseA = vAlpha;
    vPhaseB = (-0.5f * vAlpha) + (MOTOR_CONTROL_SQRT3_OVER_2 * vBeta);
    vPhaseC = (-0.5f * vAlpha) - (MOTOR_CONTROL_SQRT3_OVER_2 * vBeta);

    dutyA = clampf(0.5f + (vPhaseA / busVoltage), 0.0f, 1.0f);
    dutyB = clampf(0.5f + (vPhaseB / busVoltage), 0.0f, 1.0f);
    dutyC = clampf(0.5f + (vPhaseC / busVoltage), 0.0f, 1.0f);

    g_motorControlState.pwm_duty_a = dutyA;
    g_motorControlState.pwm_duty_b = dutyB;
    g_motorControlState.pwm_duty_c = dutyC;

    Pwm_setPhaseDutyCycles(dutyA, dutyB, dutyC);
}

static void updateMeasuredCurrents(void)
{
    g_motorControlState.phase_current_a =
        MOTOR_CONTROL_CURRENT_A_SIGN *
        (((float)g_motorControlState.phase_current_a_raw) -
         g_motorControlState.current_offset_a_counts) *
        MOTOR_CONTROL_CURRENT_A_SCALE;
    g_motorControlState.phase_current_c =
        MOTOR_CONTROL_CURRENT_C_SIGN *
        (((float)g_motorControlState.phase_current_c_raw) -
         g_motorControlState.current_offset_c_counts) *
        MOTOR_CONTROL_CURRENT_C_SCALE;
    g_motorControlState.phase_current_b =
        -(g_motorControlState.phase_current_a + g_motorControlState.phase_current_c);
    g_motorControlState.dc_bus_voltage = getBusVoltage();
}

static void runAlignmentState(void)
{
    Encoder_updateState();

    g_motorControlState.electrical_angle_rad = 0.0f;
    g_motorControlState.current_alpha = g_motorControlState.phase_current_a;
    g_motorControlState.current_beta = MOTOR_CONTROL_ONE_OVER_SQRT3 *
        (g_motorControlState.phase_current_a + (2.0f * g_motorControlState.phase_current_b));
    g_motorControlState.current_d = 0.0f;
    g_motorControlState.current_q = 0.0f;
    g_motorControlState.voltage_d = MOTOR_CONTROL_ALIGN_VOLTAGE_V;
    g_motorControlState.voltage_q = 0.0f;
    g_motorControlState.voltage_d_ff = 0.0f;
    g_motorControlState.voltage_q_ff = 0.0f;

    setVoltageVector(MOTOR_CONTROL_ALIGN_VOLTAGE_V, 0.0f,
                     g_motorControlState.dc_bus_voltage);

    motorControlLoopState.alignSampleCounter++;

    if(motorControlLoopState.alignSampleCounter >=
       motorControlLoopState.alignSampleTarget)
    {
        motorControlLoopState.electricalZeroOffsetRad =
            wrapAngle(mechanicalAngleFromCount(g_motorControlState.encoder_position) *
                      MOTOR_CONTROL_POLE_PAIRS);
        g_motorControlState.electrical_zero_offset_rad =
            motorControlLoopState.electricalZeroOffsetRad;
        motorControlLoopState.integralD = 0.0f;
        motorControlLoopState.integralQ = 0.0f;
        motorControlLoopState.filteredMechanicalSpeedRadPerSec = 0.0f;
        motorControlLoopState.previousEncoderPosition =
            g_motorControlState.encoder_position;
        motorControlLoopState.startupState =
            MOTOR_CONTROL_STATE_CURRENT_CONTROL;
        g_motorControlState.control_state =
            (uint32_t)motorControlLoopState.startupState;
    }
}

static void runCurrentControlState(void)
{
    float iAlpha;
    float iBeta;
    float sinTheta;
    float cosTheta;
    float electricalAngle;
    float electricalSpeedRadPerSec;
    float iD;
    float iQ;
    float errorD;
    float errorQ;
    float vDpi;
    float vQpi;
    float vDff;
    float vQff;
    float vD;
    float vQ;
    float vMagnitude;
    float vLimit;
    float vAlpha;
    float vBeta;

    if(!motorControlLoopState.outputEnabled)
    {
        motorControlLoopState.integralD = 0.0f;
        motorControlLoopState.integralQ = 0.0f;
        g_motorControlState.current_d_ref = 0.0f;
        g_motorControlState.current_q_ref = 0.0f;
        g_motorControlState.voltage_d = 0.0f;
        g_motorControlState.voltage_q = 0.0f;
        g_motorControlState.voltage_d_ff = 0.0f;
        g_motorControlState.voltage_q_ff = 0.0f;
        setVoltageVector(0.0f, 0.0f, g_motorControlState.dc_bus_voltage);
        return;
    }

    if(motorControlLoopState.manualDutyModeEnabled)
    {
        motorControlLoopState.integralD = 0.0f;
        motorControlLoopState.integralQ = 0.0f;
        g_motorControlState.current_d_ref = 0.0f;
        g_motorControlState.current_q_ref = 0.0f;
        g_motorControlState.voltage_d = 0.0f;
        g_motorControlState.voltage_q = 0.0f;
        g_motorControlState.voltage_d_ff = 0.0f;
        g_motorControlState.voltage_q_ff = 0.0f;
        g_motorControlState.pwm_duty_a = motorControlLoopState.manualDutyA;
        g_motorControlState.pwm_duty_b = motorControlLoopState.manualDutyB;
        g_motorControlState.pwm_duty_c = motorControlLoopState.manualDutyC;
        Pwm_setPhaseDutyCycles(motorControlLoopState.manualDutyA,
                               motorControlLoopState.manualDutyB,
                               motorControlLoopState.manualDutyC);
        return;
    }

    Encoder_updateState();

    electricalAngle = electricalAngleFromEncoder();
    electricalSpeedRadPerSec = updateMechanicalSpeedEstimate();
    sinTheta = sinf(electricalAngle);
    cosTheta = cosf(electricalAngle);

    iAlpha = g_motorControlState.phase_current_a;
    iBeta = MOTOR_CONTROL_ONE_OVER_SQRT3 *
            (g_motorControlState.phase_current_a +
             (2.0f * g_motorControlState.phase_current_b));

    iD = (iAlpha * cosTheta) + (iBeta * sinTheta);
    iQ = (-iAlpha * sinTheta) + (iBeta * cosTheta);

    errorD = g_motorControlState.current_d_ref - iD;
    errorQ = g_motorControlState.current_q_ref - iQ;

    motorControlLoopState.integralD +=
        motorControlLoopState.currentKiDiscrete * errorD;
    motorControlLoopState.integralQ +=
        motorControlLoopState.currentKiDiscrete * errorQ;

    vDpi = (MOTOR_CONTROL_KP_CURRENT * errorD) +
           motorControlLoopState.integralD;
    vQpi = (MOTOR_CONTROL_KP_CURRENT * errorQ) +
           motorControlLoopState.integralQ;

    vDff = -electricalSpeedRadPerSec * MOTOR_CONTROL_PHASE_INDUCTANCE_H * iQ;
    vQff = electricalSpeedRadPerSec *
           ((MOTOR_CONTROL_PHASE_INDUCTANCE_H * iD) +
            MOTOR_CONTROL_FLUX_LINKAGE_WB);

    vD = vDpi + vDff;
    vQ = vQpi + vQff;

    vLimit = 0.5f * g_motorControlState.dc_bus_voltage *
             MOTOR_CONTROL_VOLTAGE_LIMIT_RATIO;
    vMagnitude = sqrtf((vD * vD) + (vQ * vQ));

    if((vMagnitude > vLimit) && (vMagnitude > 0.0f))
    {
        float scale = vLimit / vMagnitude;

        vD *= scale;
        vQ *= scale;

        if(MOTOR_CONTROL_KP_CURRENT > 0.0f)
        {
            motorControlLoopState.integralD = vD - (MOTOR_CONTROL_KP_CURRENT * errorD);
            motorControlLoopState.integralQ = vQ - (MOTOR_CONTROL_KP_CURRENT * errorQ);
        }
    }

    motorControlLoopState.integralD =
        clampf(motorControlLoopState.integralD, -vLimit, vLimit);
    motorControlLoopState.integralQ =
        clampf(motorControlLoopState.integralQ, -vLimit, vLimit);

    vAlpha = (vD * cosTheta) - (vQ * sinTheta);
    vBeta = (vD * sinTheta) + (vQ * cosTheta);

    g_motorControlState.current_alpha = iAlpha;
    g_motorControlState.current_beta = iBeta;
    g_motorControlState.current_d = iD;
    g_motorControlState.current_q = iQ;
    g_motorControlState.voltage_d = vD;
    g_motorControlState.voltage_q = vQ;
    g_motorControlState.voltage_d_ff = vDff;
    g_motorControlState.voltage_q_ff = vQff;
    g_motorControlState.electrical_angle_rad = electricalAngle;
    g_motorControlState.mechanical_speed_rad_per_sec =
        motorControlLoopState.filteredMechanicalSpeedRadPerSec;
    g_motorControlState.electrical_speed_rad_per_sec =
        electricalSpeedRadPerSec;

    setVoltageVector(vAlpha, vBeta, g_motorControlState.dc_bus_voltage);
}

void MotorControl_init(void)
{
    motorControlLoopState.currentLoopFrequencyHz = Pwm_getSwitchingFrequencyHz();
    motorControlLoopState.currentLoopTs =
        1.0f / motorControlLoopState.currentLoopFrequencyHz;
    motorControlLoopState.currentKiDiscrete =
        MOTOR_CONTROL_KI_CURRENT * motorControlLoopState.currentLoopTs;
    motorControlLoopState.alignSampleTarget = (uint32_t)(
        MOTOR_CONTROL_ALIGN_DURATION_S * motorControlLoopState.currentLoopFrequencyHz);
    motorControlLoopState.startupState =
        MOTOR_CONTROL_STATE_OFFSET_CALIBRATION;
    motorControlLoopState.outputEnabled = true;
    motorControlLoopState.manualDutyModeEnabled = false;
    motorControlLoopState.manualDutyA = 0.5f;
    motorControlLoopState.manualDutyB = 0.5f;
    motorControlLoopState.manualDutyC = 0.5f;

    g_motorControlState.control_state =
        (uint32_t)motorControlLoopState.startupState;
    g_motorControlState.current_offset_a_counts = 0.0f;
    g_motorControlState.current_offset_c_counts = 0.0f;
    g_motorControlState.current_d_ref = 0.0f;
    g_motorControlState.current_q_ref = 0.0f;
    g_motorControlState.dc_bus_voltage = MOTOR_CONTROL_BUS_VOLTAGE_NOMINAL;
    g_motorControlState.voltage_d = 0.0f;
    g_motorControlState.voltage_q = 0.0f;
    g_motorControlState.voltage_d_ff = 0.0f;
    g_motorControlState.voltage_q_ff = 0.0f;
    g_motorControlState.electrical_angle_rad = 0.0f;
    g_motorControlState.electrical_zero_offset_rad = 0.0f;
    g_motorControlState.mechanical_speed_rad_per_sec = 0.0f;
    g_motorControlState.electrical_speed_rad_per_sec = 0.0f;
    g_motorControlState.pwm_duty_a = 0.5f;
    g_motorControlState.pwm_duty_b = 0.5f;
    g_motorControlState.pwm_duty_c = 0.5f;
}

void MotorControl_runCurrentLoop(void)
{
    updateMeasuredCurrents();

    switch(motorControlLoopState.startupState)
    {
        case MOTOR_CONTROL_STATE_OFFSET_CALIBRATION:
            motorControlLoopState.currentOffsetASum +=
                (float)g_motorControlState.phase_current_a_raw;
            motorControlLoopState.currentOffsetCSum +=
                (float)g_motorControlState.phase_current_c_raw;
            motorControlLoopState.offsetSampleCounter++;
            setVoltageVector(0.0f, 0.0f, g_motorControlState.dc_bus_voltage);

            if(motorControlLoopState.offsetSampleCounter >=
               MOTOR_CONTROL_OFFSET_SAMPLE_COUNT)
            {
                g_motorControlState.current_offset_a_counts =
                    motorControlLoopState.currentOffsetASum /
                    (float)MOTOR_CONTROL_OFFSET_SAMPLE_COUNT;
                g_motorControlState.current_offset_c_counts =
                    motorControlLoopState.currentOffsetCSum /
                    (float)MOTOR_CONTROL_OFFSET_SAMPLE_COUNT;
                motorControlLoopState.startupState =
                    MOTOR_CONTROL_STATE_ALIGNING;
                g_motorControlState.control_state =
                    (uint32_t)motorControlLoopState.startupState;
            }
            break;

        case MOTOR_CONTROL_STATE_ALIGNING:
            runAlignmentState();
            break;

        case MOTOR_CONTROL_STATE_CURRENT_CONTROL:
        default:
            runCurrentControlState();
            break;
    }
}

bool MotorControl_isReady(void)
{
    return motorControlLoopState.startupState ==
           MOTOR_CONTROL_STATE_CURRENT_CONTROL;
}

const char *MotorControl_getStateName(void)
{
    switch(motorControlLoopState.startupState)
    {
        case MOTOR_CONTROL_STATE_OFFSET_CALIBRATION:
            return "OFFSET";

        case MOTOR_CONTROL_STATE_ALIGNING:
            return "ALIGN";

        case MOTOR_CONTROL_STATE_CURRENT_CONTROL:
        default:
            return "RUN";
    }
}

void MotorControl_setCurrentReference(float dAxisCurrentA, float qAxisCurrentA)
{
    motorControlLoopState.outputEnabled = true;
    motorControlLoopState.manualDutyModeEnabled = false;
    g_motorControlState.current_d_ref = dAxisCurrentA;
    g_motorControlState.current_q_ref = qAxisCurrentA;
}

void MotorControl_setPhaseDutyCycles(float dutyA, float dutyB, float dutyC)
{
    motorControlLoopState.outputEnabled = true;
    motorControlLoopState.manualDutyModeEnabled = true;
    motorControlLoopState.integralD = 0.0f;
    motorControlLoopState.integralQ = 0.0f;
    motorControlLoopState.manualDutyA = dutyA;
    motorControlLoopState.manualDutyB = dutyB;
    motorControlLoopState.manualDutyC = dutyC;
    g_motorControlState.current_d_ref = 0.0f;
    g_motorControlState.current_q_ref = 0.0f;
    g_motorControlState.voltage_d = 0.0f;
    g_motorControlState.voltage_q = 0.0f;
    g_motorControlState.voltage_d_ff = 0.0f;
    g_motorControlState.voltage_q_ff = 0.0f;
    g_motorControlState.pwm_duty_a = dutyA;
    g_motorControlState.pwm_duty_b = dutyB;
    g_motorControlState.pwm_duty_c = dutyC;
    Pwm_setPhaseDutyCycles(dutyA, dutyB, dutyC);
}

void MotorControl_stop(void)
{
    motorControlLoopState.outputEnabled = false;
    motorControlLoopState.manualDutyModeEnabled = false;
    motorControlLoopState.integralD = 0.0f;
    motorControlLoopState.integralQ = 0.0f;
    g_motorControlState.current_d_ref = 0.0f;
    g_motorControlState.current_q_ref = 0.0f;
    g_motorControlState.voltage_d = 0.0f;
    g_motorControlState.voltage_q = 0.0f;
    g_motorControlState.voltage_d_ff = 0.0f;
    g_motorControlState.voltage_q_ff = 0.0f;
    setVoltageVector(0.0f, 0.0f, g_motorControlState.dc_bus_voltage);
}
