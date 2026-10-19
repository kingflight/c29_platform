#include "device.h"

#include <stdio.h>

//#include "board_io.h"
//#include "shell.h"
#include "uart.h"

#define PRINT_INTERVAL_US   500000U
#define MAIN_LOOP_PERIOD_US 1000U

int main(void)
{

    Device_init();
    Device_initGPIO();

    //BoardIo_init();
    //Uart_init();
    //Encoder_init();
    //MotorControl_init();
    //CurrentSensor_init();
    //Pwm_init();
    //Shell_init();

    //ENINT;
    //Interrupt_enableGlobal();

    uart_printf("Motor control telemetry started\r\n");

    while(1)
    {
        //Shell_process();
    }
}
