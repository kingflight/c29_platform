#include "pwm.h"

#include "device.h"
#include "driverlib.h"

#define PWM_PERIOD_TICKS       1000U
#define PWM_DUTY_TICKS         500U
#define PWM_DEADBAND_TICKS     50U

#define PWM_ALL_TRIP_ZONE_SIGNALS \
    (EPWM_TZ_SIGNAL_CBC1 | EPWM_TZ_SIGNAL_CBC2 | EPWM_TZ_SIGNAL_CBC3 | \
     EPWM_TZ_SIGNAL_CBC4 | EPWM_TZ_SIGNAL_CBC5 | EPWM_TZ_SIGNAL_CBC6 | \
     EPWM_TZ_SIGNAL_DCAEVT2 | EPWM_TZ_SIGNAL_DCBEVT2 | \
     EPWM_TZ_SIGNAL_OSHT1 | EPWM_TZ_SIGNAL_OSHT2 | EPWM_TZ_SIGNAL_OSHT3 | \
     EPWM_TZ_SIGNAL_OSHT4 | EPWM_TZ_SIGNAL_OSHT5 | EPWM_TZ_SIGNAL_OSHT6 | \
     EPWM_TZ_SIGNAL_DCAEVT1 | EPWM_TZ_SIGNAL_DCBEVT1)

#define PWM_ALL_TRIP_ZONE2_SIGNALS \
    (EPWM_TZ_SIGNAL_CAPEVT_OST | EPWM_TZ_SIGNAL_CAPEVT_CBC)

static void clearAndDisableTripZone(uint32_t base)
{
    EPWM_disableTripZoneSignals(base, PWM_ALL_TRIP_ZONE_SIGNALS);
    EPWM_disableTripZone2Signals(base, PWM_ALL_TRIP_ZONE2_SIGNALS);

    EPWM_clearTripZoneFlag(base, EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_CBC |
                                 EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1 |
                                 EPWM_TZ_FLAG_DCAEVT2 | EPWM_TZ_FLAG_DCBEVT1 |
                                 EPWM_TZ_FLAG_DCBEVT2 | EPWM_TZ_FLAG_CAPEVT);
    EPWM_clearCycleByCycleTripZoneFlag(base, EPWM_TZ_CBC_FLAG_1 |
                                             EPWM_TZ_CBC_FLAG_2 |
                                             EPWM_TZ_CBC_FLAG_3 |
                                             EPWM_TZ_CBC_FLAG_4 |
                                             EPWM_TZ_CBC_FLAG_5 |
                                             EPWM_TZ_CBC_FLAG_6 |
                                             EPWM_TZ_CBC_FLAG_DCAEVT2 |
                                             EPWM_TZ_CBC_FLAG_DCBEVT2 |
                                             EPWM_TZ_CBC_FLAG_CAPEVT);
    EPWM_clearOneShotTripZoneFlag(base, EPWM_TZ_OST_FLAG_OST1 |
                                        EPWM_TZ_OST_FLAG_OST2 |
                                        EPWM_TZ_OST_FLAG_OST3 |
                                        EPWM_TZ_OST_FLAG_OST4 |
                                        EPWM_TZ_OST_FLAG_OST5 |
                                        EPWM_TZ_OST_FLAG_OST6 |
                                        EPWM_TZ_OST_FLAG_DCAEVT1 |
                                        EPWM_TZ_OST_FLAG_DCBEVT1 |
                                        EPWM_TZ_OST_FLAG_CAPEVT);
}

static void configurePwmPins(void)
{
    GPIO_setPinConfig(GPIO_0_EPWM1_A);
    GPIO_setPinConfig(GPIO_1_EPWM1_B);
    GPIO_setPinConfig(GPIO_2_EPWM2_A);
    GPIO_setPinConfig(GPIO_3_EPWM2_B);
    GPIO_setPinConfig(GPIO_4_EPWM3_A);
    GPIO_setPinConfig(GPIO_5_EPWM3_B);

    GPIO_setPadConfig(0U, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(1U, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(2U, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(3U, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(4U, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(5U, GPIO_PIN_TYPE_STD);
}

static void configureComplementaryPwm(uint32_t base)
{
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, PWM_PERIOD_TICKS);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, PWM_DUTY_TICKS);

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setRisingEdgeDelayCount(base, PWM_DEADBAND_TICKS);
    EPWM_setFallingEdgeDelayCount(base, PWM_DEADBAND_TICKS);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED,
                                  EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED,
                                  EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setDeadBandOutputSwapMode(base, EPWM_DB_OUTPUT_A, false);
    EPWM_setDeadBandOutputSwapMode(base, EPWM_DB_OUTPUT_B, false);
}

static void configurePwmSync(void)
{
    EPWM_disablePhaseShiftLoad(EPWM1_BASE);
    EPWM_setPhaseShift(EPWM1_BASE, 0U);
    EPWM_enableSyncOutPulseSource(EPWM1_BASE, EPWM_SYNC_OUT_PULSE_ON_CNTR_ZERO);

    EPWM_setSyncInPulseSource(EPWM2_BASE,
                              EPWM_SYNC_IN_PULSE_SRC_SYNCOUT_EPWM1);
    EPWM_enablePhaseShiftLoad(EPWM2_BASE);
    EPWM_setPhaseShift(EPWM2_BASE, 0U);
    EPWM_setCountModeAfterSync(EPWM2_BASE, EPWM_COUNT_MODE_UP_AFTER_SYNC);

    EPWM_setSyncInPulseSource(EPWM3_BASE,
                              EPWM_SYNC_IN_PULSE_SRC_SYNCOUT_EPWM1);
    EPWM_enablePhaseShiftLoad(EPWM3_BASE);
    EPWM_setPhaseShift(EPWM3_BASE, 0U);
    EPWM_setCountModeAfterSync(EPWM3_BASE, EPWM_COUNT_MODE_UP_AFTER_SYNC);
}

static void configureMasterPwmSoc(void)
{
    EPWM_disableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_PERIOD);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1U);
    EPWM_clearADCTriggerFlag(EPWM1_BASE, EPWM_SOC_A);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
}

static float clampDuty(float duty)
{
    if(duty < 0.0f)
    {
        return 0.0f;
    }

    if(duty > 1.0f)
    {
        return 1.0f;
    }

    return duty;
}

static uint16_t dutyToCompare(float duty)
{
    duty = clampDuty(duty);
    return (uint16_t)((1.0f - duty) * (float)PWM_PERIOD_TICKS);
}

void Pwm_init(void)
{
    configurePwmPins();

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    configureComplementaryPwm(EPWM1_BASE);
    configureComplementaryPwm(EPWM2_BASE);
    configureComplementaryPwm(EPWM3_BASE);
    clearAndDisableTripZone(EPWM1_BASE);
    clearAndDisableTripZone(EPWM2_BASE);
    clearAndDisableTripZone(EPWM3_BASE);
    configurePwmSync();
    configureMasterPwmSoc();
    Pwm_setPhaseDutyCycles(0.5, 0.5, 0.5);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

void Pwm_setPhaseDutyCycles(float dutyA, float dutyB, float dutyC)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
                                dutyToCompare(dutyA));
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A,
                                dutyToCompare(dutyB));
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A,
                                dutyToCompare(dutyC));
}

float Pwm_getSwitchingFrequencyHz(void)
{
    return (float)DEVICE_SYSCLK_FREQ / (2.0f * (float)PWM_PERIOD_TICKS);
}
