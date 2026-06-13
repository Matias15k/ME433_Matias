#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"

#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

void drawChar(int x, int y, char c) {
    if (c < 0x20 || c > 0x7f) return;
    int idx = c - 0x20;
    for (int col = 0; col < 5; col++) {
        unsigned char colData = ASCII[idx][col];
        for (int row = 0; row < 8; row++) {
            ssd1306_drawPixel(x + col, y + row, (colData >> row) & 1);
        }
    }
}

void drawString(int x, int y, char *str) {
    while (*str != '\0') {
        drawChar(x, y, *str);
        x += 6;
        str++;
    }
}

int main() {
    stdio_init_all();
    if (cyw43_arch_init()) return -1;

    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    ssd1306_setup();

    adc_init();
    adc_gpio_init(26);   // ADC0 = GP26
    adc_select_input(0);

    unsigned int t_prev = to_us_since_boot(get_absolute_time());
    unsigned int t_blink = t_prev;
    unsigned int t_move = t_prev;
    int led = 0;
    int hw_visible = 1;
    // "hello world" is 11 chars * 6px = 66px wide, 8px tall
    int hw_x = 0, hw_y = 12;
    float fps = 0.0f;

    srand(t_prev);

    while (true) {
        unsigned int t_now = to_us_since_boot(get_absolute_time());
        printf("hello");
        if (t_now - t_blink >= 500000) {
            t_blink += 500000;
            led = !led;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
        }
        unsigned int dt = t_now - t_prev;
        if (dt > 0) fps = 1000000.0f / dt;
        t_prev = t_now;

        uint16_t raw = adc_read();
        float voltage = raw * 3.3f / 4096.0f;

        ssd1306_clear();

        char msg[32];
        sprintf(msg, "ADC0 = %.3f V", voltage);
        drawString(0, 0, msg);

       sprintf(msg, "FPS: %.1f", fps);
       drawString(0, 24, msg);

        ssd1306_update();
    }
}
