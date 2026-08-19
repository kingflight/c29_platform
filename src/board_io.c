#include "board_io.h"

#include "device.h"
#include "driverlib.h"

static void enableBoosterPackPower(void)
{
    GPIO_setPinConfig(GPIO_19_GPIO19);
    GPIO_setPadConfig(19U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(19U, GPIO_DIR_MODE_OUT);
    GPIO_writePin(19U, 0U);
}

void BoardIo_init(void)
{
    enableBoosterPackPower();
}
