#ifndef UART1_H
#define UART1_H
#include <stdint.h>

void uart1_init(void);
void uart1_writeString(const char *text);
int uart1_printf(const char *format, ...);
int32_t uart1_readCharNonBlocking(void);
void uart1_clearRxErrors(void);

#endif

