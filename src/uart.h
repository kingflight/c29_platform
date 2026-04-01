#ifndef UART_APP_H
#define UART_APP_H

void Uart_init(void);
void Uart_writeString(const char *text);
int Uart_printf(const char *format, ...);

#endif
