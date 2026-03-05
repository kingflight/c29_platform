#include "device.h"
#include "driverlib.h"

int main(void)
{
    Device_init();
    Device_initGPIO();

    GPIO_setPinConfig(DEVICE_GPIO_CFG_LED1);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);

    while(1)
    {
        GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
        DEVICE_DELAY_US(500000);
    }
}
