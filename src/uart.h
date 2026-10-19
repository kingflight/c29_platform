#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_writeString(const char *text);
int uart_printf(const char *format, ...);
int32_t uart_readCharNonBlocking(void);
void uart_clearRxErrors(void);

#endif

