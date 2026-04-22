/*
 * UART.c
 *
 * Created: 9/10/2021 12:32:14 PM
 *  Author: Emin Eminof
 */ 
#include "dw3000_uart.h"


void UART_init(void)
{
  // No-op under ESPHome component context.
}

void UART_putc(char data)
{
  (void) data;
}

void UART_puts(const char *s)
{
  (void) s;
}

void test_run_info(const unsigned char *s)
{
    UART_puts(reinterpret_cast<const char *>(s));
    UART_puts("\r\n");
}
