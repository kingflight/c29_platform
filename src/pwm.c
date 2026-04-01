#include "pwm.h"

#include "driverlib.h"

#define PWM_PERIOD_TICKS       1000U
#define PWM_DUTY_TICKS         500U
#define PWM_DEADBAND_TICKS     50U

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

void Pwm_init(void)
{
    configurePwmPins();

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    configureComplementaryPwm(EPWM1_BASE);
    configureComplementaryPwm(EPWM2_BASE);
    configureComplementaryPwm(EPWM3_BASE);
    configurePwmSync();
    configureMasterPwmSoc();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}
