#include "uart.h"

#include "device.h"
#include "driverlib.h"

#include <stdarg.h>
#include <stdio.h>

#define UART_BAUD_RATE       115200U
#define UART_PRINTF_BUF_SIZE 192U

void Uart_init(void)
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

void Uart_writeString(const char *text)
{
    while(*text != '\0')
    {
        UART_writeChar(UARTA_BASE, (uint8_t)*text);
        text++;
    }
}

int Uart_printf(const char *format, ...)
{
    va_list args;
    char buffer[UART_PRINTF_BUF_SIZE];
    int length;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if(length > 0)
    {
        Uart_writeString(buffer);
    }

    return(length);
}
