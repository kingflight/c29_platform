#include "device.h"

#include <stdio.h>

#include "board_io.h"
#include "current_sensor.h"
#include "encoder.h"
#include "motor_state.h"
#include "pwm.h"
#include "uart.h"

#define PRINT_INTERVAL_US   500000U

int main(void)
{
    Device_init();
    Device_initGPIO();

    BoardIo_init();
    Uart_init();
    CurrentSensor_init();
    Encoder_init();
    Pwm_init();

    ENINT;
    Interrupt_enableGlobal();

    Uart_printf("Motor control telemetry started\r\n");

    while(1)
    {
        Encoder_updateState();

        Uart_printf(
            "IA=%u IC=%u VDC=%u EQEP_POS=%lu EQEP_LAT=%lu DIR=%d ADC_ISR=%lu\r\n",
            g_motorControlState.phase_current_a_raw,
            g_motorControlState.phase_current_c_raw,
            g_motorControlState.dc_bus_raw,
            (unsigned long)g_motorControlState.encoder_position,
            (unsigned long)g_motorControlState.encoder_position_latched,
            g_motorControlState.encoder_direction,
            (unsigned long)g_motorControlState.adc_isr_count);

        DEVICE_DELAY_US(PRINT_INTERVAL_US);
    }
}
