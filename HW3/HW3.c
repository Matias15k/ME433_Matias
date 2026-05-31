#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"

// I2C defines
#define I2C_PORT i2c0
#define I2C_SDA  20
#define I2C_SCL  21

// MCP23008 7-bit address (A0=A1=A2=GND -> 0b0100000)
#define MCP23008_ADDR 0x20

// MCP23008 registers
#define IODIR_REG 0x00  // direction: 1=input, 0=output
#define GPPU_REG  0x06  // pull-up enable
#define GPIO_REG  0x09  // read pin states
#define OLAT_REG  0x0A  // set output latches

// Write one byte to a register on the MCP23008
void mcp_write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    i2c_write_blocking(I2C_PORT, MCP23008_ADDR, buf, 2, false);
}

// Read one byte from a register on the MCP23008
uint8_t mcp_read(uint8_t reg) {
    uint8_t value;
    i2c_write_blocking(I2C_PORT, MCP23008_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MCP23008_ADDR, &value, 1, false);
    return value;
}

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // 100 kHz: safe rise-time budget with 10k pull-ups (400 kHz requires <300 ns rise)
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // GP0 = input (bit 0 = 1), GP7 = output (bit 7 = 0), rest = inputs
    mcp_write(IODIR_REG, 0x7F);  // 0b01111111
    // Enable internal pull-up on GP0 (belt-and-suspenders with external 10k)
    mcp_write(GPPU_REG, 0x01);
    // Start with GP7 off
    mcp_write(OLAT_REG, 0x00);

    int led_state = 0;

    while (true) {
        // Read all pins; GP0 is bit 0
        uint8_t pins = mcp_read(GPIO_REG);

        if (pins & 0x01) {
            // GP0 high -> button not pressed
            mcp_write(OLAT_REG, 0x00);        // GP7 off
        } else {
            // GP0 low -> button pressed (pulled to GND)
            mcp_write(OLAT_REG, 0x80);        // GP7 on (bit 7)
        }

        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

        sleep_ms(200);
    }
}
