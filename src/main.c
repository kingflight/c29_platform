#include "device.h"

#include <stdio.h>

#include "board_io.h"
#include "current_sensor.h"
#include "encoder.h"
#include "motor_control.h"
#include "motor_state.h"
#include "pwm.h"
#include "shell.h"
#include "uart.h"

#define PRINT_INTERVAL_US   500000U
#define MAIN_LOOP_PERIOD_US 1000U

int main(void)
{
    uint32_t printCountdownUs;

    Device_init();
    Device_initGPIO();

    BoardIo_init();
    Uart_init();
    Encoder_init();
    MotorControl_init();
    CurrentSensor_init();
    Pwm_init();
    Shell_init();

    ENINT;
    Interrupt_enableGlobal();

    Uart_printf("Motor control telemetry started\r\n");
    printCountdownUs = PRINT_INTERVAL_US;

    while(1)
    {
        Shell_process();

        if((printCountdownUs == 0U) && !Shell_isEditingLine())
        {
            Encoder_updateState();

            Uart_printf(
                "STATE=%s IA=%.3f IB=%.3f IC=%.3f ID=%.3f IQ=%.3f ID_REF=%.3f IQ_REF=%.3f WMECH=%.2f WELEC=%.2f OFFA=%.1f OFFC=%.1f POS=%lu ANG=%.3f ADC_ISR=%lu\r\n",
                MotorControl_getStateName(),
                g_motorControlState.phase_current_a,
                g_motorControlState.phase_current_b,
                g_motorControlState.phase_current_c,
                g_motorControlState.current_d,
                g_motorControlState.current_q,
                g_motorControlState.current_d_ref,
                g_motorControlState.current_q_ref,
                g_motorControlState.mechanical_speed_rad_per_sec,
                g_motorControlState.electrical_speed_rad_per_sec,
                g_motorControlState.current_offset_a_counts,
                g_motorControlState.current_offset_c_counts,
                (unsigned long)g_motorControlState.encoder_position,
                g_motorControlState.electrical_angle_rad,
                (unsigned long)g_motorControlState.adc_isr_count);

            printCountdownUs = PRINT_INTERVAL_US;
        }

        DEVICE_DELAY_US(MAIN_LOOP_PERIOD_US);

        if(printCountdownUs > MAIN_LOOP_PERIOD_US)
        {
            printCountdownUs -= MAIN_LOOP_PERIOD_US;
        }
        else
        {
            printCountdownUs = 0U;
        }
    }
}
