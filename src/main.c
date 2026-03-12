#include "device.h"
#include "driverlib.h"

#define PWM_PERIOD_TICKS        1000U
#define PWM_DUTY_TICKS          500U
#define PWM_DEADBAND_TICKS      50U
#define ADC_ACQPS_12BIT         14U

typedef struct
{
    uint32_t epwmBase;
    uint32_t gpioAConfig;
    uint32_t gpioBConfig;
    uint32_t adcBase;
    ADC_Channel adcChannel;
    ADC_Trigger adcTrigger;
} PwmAdcConfig;

static const PwmAdcConfig pwmAdcConfigs[] =
{
    {EPWM1_BASE, GPIO_0_EPWM1_A, GPIO_1_EPWM1_B, ADCC_BASE,
     ADC_CH_ADCIN1, ADC_TRIGGER_EPWM1_SOCA},
    {EPWM2_BASE, GPIO_2_EPWM2_A, GPIO_3_EPWM2_B, ADCB_BASE,
     ADC_CH_ADCIN7, ADC_TRIGGER_EPWM2_SOCA},
    {EPWM3_BASE, GPIO_4_EPWM3_A, GPIO_5_EPWM3_B, ADCA_BASE,
     ADC_CH_ADCIN7, ADC_TRIGGER_EPWM3_SOCA},
};

volatile uint16_t adcResultA7 = 0U;
volatile uint16_t adcResultB4 = 0U;
volatile uint16_t adcResultC1 = 0U;

static void configurePwmPins(void)
{
    uint32_t i;

    for(i = 0U; i < (sizeof(pwmAdcConfigs) / sizeof(pwmAdcConfigs[0])); i++)
    {
        GPIO_setPinConfig(pwmAdcConfigs[i].gpioAConfig);
        GPIO_setPinConfig(pwmAdcConfigs[i].gpioBConfig);
        GPIO_setPadConfig((uint32_t)(i * 2U), GPIO_PIN_TYPE_STD);
        GPIO_setPadConfig((uint32_t)(i * 2U + 1U), GPIO_PIN_TYPE_STD);
    }
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

static void configurePwmSoc(uint32_t base)
{
    EPWM_disableADCTrigger(base, EPWM_SOC_A);
    EPWM_setADCTriggerSource(base, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(base, EPWM_SOC_A, 1U);
    EPWM_clearADCTriggerFlag(base, EPWM_SOC_A);
    EPWM_enableADCTrigger(base, EPWM_SOC_A);
}

static void configureAdc(uint32_t base)
{
    ADC_setPrescaler(base, ADC_CLK_DIV_4_0);
    ADC_setMode(base, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(base, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(base);
    DEVICE_DELAY_US(1000U);
}

static void configureAdcSoc(uint32_t base, ADC_Channel channel,
                            ADC_Trigger trigger)
{
    ADC_setupSOC(base, ADC_SOC_NUMBER0, trigger, channel, ADC_ACQPS_12BIT);
}

int main(void)
{
    Device_init();
    Device_initGPIO();

    ASysCtl_setVREF(ASYSCTL_VREFHIAB, ASYSCTL_VREF_INTERNAL_3_3_V);

    configurePwmPins();
    configureAdc(ADCA_BASE);
    configureAdc(ADCB_BASE);
    configureAdc(ADCC_BASE);

    configureAdcSoc(ADCC_BASE, ADC_CH_ADCIN1, ADC_TRIGGER_EPWM1_SOCA);
    configureAdcSoc(ADCB_BASE, ADC_CH_ADCIN4, ADC_TRIGGER_EPWM2_SOCA);
    configureAdcSoc(ADCA_BASE, ADC_CH_ADCIN7, ADC_TRIGGER_EPWM3_SOCA);

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    configureComplementaryPwm(EPWM1_BASE);
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 800);

    configureComplementaryPwm(EPWM2_BASE);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, 700);

    configureComplementaryPwm(EPWM3_BASE);

    configurePwmSync();
    configurePwmSoc(EPWM1_BASE);
    configurePwmSoc(EPWM2_BASE);
    configurePwmSoc(EPWM3_BASE);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    while(1)
    {
        adcResultC1 = ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
        adcResultB4 = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
        adcResultA7 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    }
}
