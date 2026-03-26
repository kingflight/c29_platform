#include "device.h"
#include "driverlib.h"

#include <stdio.h>

#define PWM_PERIOD_TICKS        1000U
#define PWM_DUTY_TICKS          500U
#define PWM_DEADBAND_TICKS      50U
#define ADC_ACQPS_12BIT         14U
#define UART_BAUD_RATE          115200U
#define UART_TX_BUFFER_SIZE     128U
#define PRINT_INTERVAL_US       500000U

volatile uint16_t phaseCurrentA = 0U;
volatile uint16_t phaseCurrentC = 0U;
volatile uint16_t vrefCount = 0U;
volatile uint16_t AS5600 = 0U;
uint32_t isr_count = 0;

__attribute__((interrupt("INT"))) void adcA1ISR(void);

static void enableBoosterPackPower(void)
{
    GPIO_writePin(19U, 0U);
    GPIO_setPadConfig(19U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(19U, GPIO_DIR_MODE_OUT);
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

static void configureUart(void)
{
    GPIO_setPinConfig(DEVICE_GPIO_CFG_UARTA_TX);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_UARTA_TX, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_UARTA_TX, GPIO_QUAL_ASYNC);

    GPIO_setPinConfig(DEVICE_GPIO_CFG_UARTA_RX);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_UARTA_RX,
                      GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_UARTA_RX, GPIO_QUAL_ASYNC);

    UART_setConfig(UARTA_BASE, DEVICE_SYSCLK_FREQ, UART_BAUD_RATE,
                   UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE);
    UART_enableModuleNonFIFO(UARTA_BASE);
}

static void writeUartString(const char *text)
{
    while(*text != '\0')
    {
        UART_writeChar(UARTA_BASE, (uint8_t)*text);
        text++;
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

static void configureMasterPwmSoc(void)
{
    EPWM_disableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_PERIOD);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1U);
    EPWM_clearADCTriggerFlag(EPWM1_BASE, EPWM_SOC_A);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
}

static void configureAdc(uint32_t base)
{
    ADC_setPrescaler(base, ADC_CLK_DIV_4_0);
    ADC_setMode(base, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(base, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(base);
    DEVICE_DELAY_US(1000U);
}

static void configureAdcSoc(uint32_t base, ADC_SOCNumber socNumber,
                            ADC_Channel channel)
{
    ADC_setupSOC(base, socNumber, ADC_TRIGGER_EPWM1_SOCA, channel,
                 ADC_ACQPS_12BIT);
}

static void configureAdcInterrupt(void)
{
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_INT_TRIGGER_EOC0);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_register(INT_ADCA1, &adcA1ISR);
    Interrupt_enable(INT_ADCA1);
}

__attribute__((interrupt("INT"))) void adcA1ISR(void)
{
    isr_count++;
    phaseCurrentA = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
    //phaseCurrentB = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    phaseCurrentC = ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
    vrefCount = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    AS5600 = ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER0);
    

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    if(ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    }
}

int main(void)
{
    char uartMessage[UART_TX_BUFFER_SIZE];
    int messageLength;

    Device_init();
    Device_initGPIO();
    enableBoosterPackPower();
    configureUart();

    ASysCtl_setVREF(ASYSCTL_VREFHIAB, ASYSCTL_VREF_EXTERNAL);
    ASysCtl_setVREF(ASYSCTL_VREFHICDE, ASYSCTL_VREF_EXTERNAL);

    configurePwmPins();
    configureAdc(ADCA_BASE);
    configureAdc(ADCB_BASE);
    configureAdc(ADCC_BASE);
    configureAdc(ADCE_BASE);

    //configureAdcSoc(ADCA_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN7); // not working
    configureAdcSoc(ADCA_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN0);  // VDC
    configureAdcSoc(ADCB_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN4);  // phase A
    configureAdcSoc(ADCC_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN1);  // phase C
    configureAdcSoc(ADCE_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN1);  // AS5600 analog out
    configureAdcInterrupt();

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    configureComplementaryPwm(EPWM1_BASE);
    configureComplementaryPwm(EPWM2_BASE);
    configureComplementaryPwm(EPWM3_BASE);
    configurePwmSync();
    configureMasterPwmSoc();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    ENINT;
    Interrupt_enableGlobal();

    writeUartString("Phase current monitor started\r\n");

    while(1)
    {
        messageLength = snprintf(uartMessage, sizeof(uartMessage),
                                 "IA=%u IC=%u VREF=%u AS5600=%u isr_cnt: %d\r\n",
                                 phaseCurrentA, phaseCurrentC,
                                 vrefCount, AS5600, isr_count);

        if(messageLength > 0)
        {
            writeUartString(uartMessage);
        }

        DEVICE_DELAY_US(PRINT_INTERVAL_US);
    }
}
