#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"

#define UART_ID uart1
#define BAUD_RATE 115200

// GP4 = UART1 TX (connect to STM32 RX)
// GP5 = UART1 RX (connect to STM32 TX)
#define UART_TX_PIN 4
#define UART_RX_PIN 5

int main()
{
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    while (true) {
        // Forward computer -> STM32: read a char from USB serial, send to UART1
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            uart_putc(UART_ID, (char)c);
        }

        // Forward STM32 -> computer: read a char from UART1, print to USB serial
        if (uart_is_readable(UART_ID)) {
            char r = uart_getc(UART_ID);
            putchar(r);
        }
    }
}
