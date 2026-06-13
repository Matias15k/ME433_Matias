#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

// channel: 0 = DAC A, 1 = DAC B
// value: 0-1023 (10-bit)
void dac_write(uint8_t channel, uint16_t value) {
    uint16_t cmd = 0;
    if (channel) cmd |= (1 << 15);  // channel select: 0=A, 1=B
    // BUF = 0 (unbuffered VREF)
    cmd |= (1 << 13);               // GA = 1 (1x gain, output = VREF * D/1024)
    cmd |= (1 << 12);               // SHDN = 1 (output active)
    cmd |= (value & 0x3FF) << 2;    // D9-D0 in bits 11-2

    uint8_t data[2] = {(cmd >> 8) & 0xFF, cmd & 0xFF};

    cs_select(PIN_CS);
    spi_write_blocking(SPI_PORT, data, 2);
    cs_deselect(PIN_CS);
}

int main() {
    stdio_init_all();
    cyw43_arch_init();

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // Update rate: 1000 Hz (1 ms per step)
    // Sine:     2 Hz → period = 500 samples
    // Triangle: 1 Hz → period = 1000 samples
    // Both are well above the required 50x minimum.

    int sample = 0;
    bool led = false;
    while (true) {
        // Channel A: 2 Hz sine, 0 V to 3.3 V
        float sine_val = sinf(2.0f * (float)M_PI * 2.0f * sample / 1000.0f);
        uint16_t sine_dac = (uint16_t)((sine_val + 1.0f) * 0.5f * 1023.0f);

        // Channel B: 1 Hz triangle, 0 V to 3.3 V
        int tri_pos = sample % 1000;
        uint16_t tri_dac;
        if (tri_pos < 500) {
            tri_dac = (uint16_t)(tri_pos * 1023 / 500);
        } else {
            tri_dac = (uint16_t)((1000 - tri_pos) * 1023 / 500);
        }

        dac_write(0, sine_dac);
        dac_write(1, tri_dac);

        sample++;
        if (sample >= 1000) {
            sample = 0;
            led = !led;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
        }

        sleep_ms(1);
    }
}
