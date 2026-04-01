#include "board_io.h"

#include "driverlib.h"

static void enableBoosterPackPower(void)
{
    GPIO_writePin(19U, 0U);
    GPIO_setPadConfig(19U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(19U, GPIO_DIR_MODE_OUT);
}

void BoardIo_init(void)
{
    enableBoosterPackPower();
}
