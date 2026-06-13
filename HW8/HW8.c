#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/spi.h"

#define SPI_PORT    spi0
#define PIN_MISO    16
#define PIN_CS_DAC  17
#define PIN_SCK     18
#define PIN_MOSI    19
#define PIN_CS_RAM  20

#define RAM_WRITE   0x02
#define RAM_READ    0x03
#define RAM_WRSR    0x01
#define RAM_SEQ     0x40

#define NUM_SAMPLES 1000

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

void spi_ram_init(void) {
    cs_select(PIN_CS_RAM);
    uint8_t cmd[2] = {RAM_WRSR, RAM_SEQ};
    spi_write_blocking(SPI_PORT, cmd, 2);
    cs_deselect(PIN_CS_RAM);
}

void spi_ram_write_bytes(uint16_t addr, const uint8_t *data, size_t len) {
    cs_select(PIN_CS_RAM);
    uint8_t hdr[3] = {RAM_WRITE, (addr >> 8) & 0xFF, addr & 0xFF};
    spi_write_blocking(SPI_PORT, hdr, 3);
    spi_write_blocking(SPI_PORT, data, len);
    cs_deselect(PIN_CS_RAM);
}

void spi_ram_read_bytes(uint16_t addr, uint8_t *data, size_t len) {
    cs_select(PIN_CS_RAM);
    uint8_t hdr[3] = {RAM_READ, (addr >> 8) & 0xFF, addr & 0xFF};
    spi_write_blocking(SPI_PORT, hdr, 3);
    spi_read_blocking(SPI_PORT, 0, data, len);
    cs_deselect(PIN_CS_RAM);
}

int main() {
    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS_DAC);
    gpio_set_dir(PIN_CS_DAC, GPIO_OUT);
    gpio_put(PIN_CS_DAC, 1);

    gpio_init(PIN_CS_RAM);
    gpio_set_dir(PIN_CS_RAM, GPIO_OUT);
    gpio_put(PIN_CS_RAM, 1);

    spi_ram_init();

    // Build 1000 DAC SPI command words for one cycle of a 1 Hz sine wave (0V to 3.3V)
    // and store all 2000 bytes into SPI RAM in one sequential write.
    uint8_t buf[NUM_SAMPLES * 2];
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float s = sinf(2.0f * (float)M_PI * i / NUM_SAMPLES);
        uint16_t dac_val = (uint16_t)((s + 1.0f) * 0.5f * 1023.0f);

        // Full 16-bit MCP4822 command: channel A, GA=1 (1x), SHDN=active
        uint16_t cmd = (1 << 13) | (1 << 12) | ((dac_val & 0x3FF) << 2);
        buf[i * 2]     = (cmd >> 8) & 0xFF;
        buf[i * 2 + 1] = cmd & 0xFF;
    }
    spi_ram_write_bytes(0, buf, sizeof(buf));

    int sample = 0;
    while (true) {
        uint8_t word[2];
        spi_ram_read_bytes(sample * 2, word, 2);

        cs_select(PIN_CS_DAC);
        spi_write_blocking(SPI_PORT, word, 2);
        cs_deselect(PIN_CS_DAC);

        sample++;
        if (sample >= NUM_SAMPLES) sample = 0;

        sleep_ms(1);
    }
}
