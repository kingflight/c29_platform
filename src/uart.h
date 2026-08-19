#ifndef UART_APP_H
#define UART_APP_H

#include <stdint.h>

void Uart_init(void);
void Uart_writeString(const char *text);
int Uart_printf(const char *format, ...);
int32_t Uart_readCharNonBlocking(void);
void Uart_clearRxErrors(void);

#endif
