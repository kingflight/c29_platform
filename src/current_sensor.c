#include "current_sensor.h"

#include "device.h"
#include "driverlib.h"

#include "motor_state.h"

#define ADC_ACQPS_12BIT 14U

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

void CurrentSensor_init(void)
{
    ASysCtl_setVREF(ASYSCTL_VREFHIAB, ASYSCTL_VREF_EXTERNAL);
    ASysCtl_setVREF(ASYSCTL_VREFHICDE, ASYSCTL_VREF_EXTERNAL);

    configureAdc(ADCA_BASE);
    configureAdc(ADCB_BASE);
    configureAdc(ADCC_BASE);

    configureAdcSoc(ADCA_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN0);
    configureAdcSoc(ADCB_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN4);
    configureAdcSoc(ADCC_BASE, ADC_SOC_NUMBER0, ADC_CH_ADCIN1);

    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_INT_TRIGGER_EOC0);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);

    Interrupt_register(INT_ADCA1, &CurrentSensor_adcIsr);
    Interrupt_enable(INT_ADCA1);
}

__attribute__((interrupt("INT"))) void CurrentSensor_adcIsr(void)
{
    g_motorControlState.adc_isr_count++;
    g_motorControlState.phase_current_a_raw =
        ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
    g_motorControlState.phase_current_c_raw =
        ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
    g_motorControlState.dc_bus_raw =
        ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    if(ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    }
}
