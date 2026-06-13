#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"

#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

// MPU6050 config registers
#define CONFIG       0x1A
#define GYRO_CONFIG  0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1   0x6B
#define PWR_MGMT_2   0x6C
// MPU6050 sensor data registers (14 sequential bytes starting at ACCEL_XOUT_H)
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75

static uint8_t imu_addr = 0x68;

static void imu_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c_default, imu_addr, buf, 2, false);
}

static uint8_t imu_read_reg(uint8_t reg) {
    uint8_t val;
    i2c_write_blocking(i2c_default, imu_addr, &reg, 1, true);
    i2c_read_blocking(i2c_default, imu_addr, &val, 1, false);
    return val;
}

// Try 0x68 then 0x69 (MPU-6050M may use different address).
// Hang blinking LED if WHO_AM_I doesn't return 0x68 or 0x98.
static void imu_init(void) {
    imu_addr = 0x68;
    uint8_t who = imu_read_reg(WHO_AM_I);
    if (who != 0x68 && who != 0x98) {
        imu_addr = 0x69;
        who = imu_read_reg(WHO_AM_I);
    }
    printf("IMU WHO_AM_I=0x%02X at addr=0x%02X\n", who, imu_addr);
    if (who != 0x68 && who != 0x98) {
        printf("IMU not found, continuing anyway\n");
        return;
    }
    imu_write(PWR_MGMT_1,   0x00); // wake up
    imu_write(ACCEL_CONFIG, 0x00); // ±2g  (16384 LSB/g)
    imu_write(GYRO_CONFIG,  0x18); // ±2000 dps
}

typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} imu_data_t;

// Burst-read all 14 sensor bytes in one I2C transaction.
static void imu_read(imu_data_t *d) {
    uint8_t reg = ACCEL_XOUT_H;
    uint8_t buf[14];
    i2c_write_blocking(i2c_default, imu_addr, &reg, 1, true);
    i2c_read_blocking(i2c_default, imu_addr, buf, 14, false);
    d->ax   = (int16_t)((buf[0]  << 8) | buf[1]);
    d->ay   = (int16_t)((buf[2]  << 8) | buf[3]);
    d->az   = (int16_t)((buf[4]  << 8) | buf[5]);
    d->temp = (int16_t)((buf[6]  << 8) | buf[7]);
    d->gx   = (int16_t)((buf[8]  << 8) | buf[9]);
    d->gy   = (int16_t)((buf[10] << 8) | buf[11]);
    d->gz   = (int16_t)((buf[12] << 8) | buf[13]);
}

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

// Bresenham line algorithm.
void drawLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        ssd1306_drawPixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

int main() {
    stdio_init_all();
    //while (!stdio_usb_connected()) { sleep_ms(100); }
    printf("USB connected\n");

    if (cyw43_arch_init()) return -1;
    printf("CYW43 ok\n");

    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    printf("I2C ok\n");

    ssd1306_setup();
    printf("OLED ok\n");


    // Scan all I2C addresses to find connected devices
    printf("I2C scan:\n");
    for (int addr = 1; addr < 128; addr++) {
        uint8_t rxdata;
        int ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);
        if (ret >= 0) printf("  device at 0x%02X\n", addr);
    }
    printf("scan done\n");

    imu_init();
    printf("IMU ok\n");

    unsigned int t_blink = to_us_since_boot(get_absolute_time());
    int led = 0;

    // Fixed 100 Hz loop: track the next deadline in absolute time.
    absolute_time_t deadline = get_absolute_time();

    while (true) {
        deadline = delayed_by_us(deadline, 10000); // 10 ms = 100 Hz

        unsigned int t_now = to_us_since_boot(get_absolute_time());
        if (t_now - t_blink >= 500000) {
            t_blink += 500000;
            led = !led;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
        }

        imu_data_t d;
        imu_read(&d);

        // Convert to physical units for serial output.
        float ax_g   = d.ax * 0.000061f;
        float ay_g   = d.ay * 0.000061f;
        float az_g   = d.az * 0.000061f;
        float gx_dps = d.gx * 0.007630f;
        float gy_dps = d.gy * 0.007630f;
        float gz_dps = d.gz * 0.007630f;
        float temp_c = d.temp / 340.00f + 36.53f;

        // printf("ax=%.3f ay=%.3f az=%.3f  gx=%.2f gy=%.2f gz=%.2f  T=%.1fC\n",
        //        ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, temp_c);

        // Draw X/Y gravity vector as a line from the display center.
        // 1g raw ≈ 16384; map to ±60 px in X, ±14 px in Y.
        const int cx = 64, cy = 16;
        int ex = clamp(cx - d.ax * 60 / 16384, 0, 127);
        int ey = clamp(cy + d.ay * 14 / 16384, 0, 31);

        ssd1306_clear();
        drawLine(cx, cy, ex, ey);
        ssd1306_drawPixel(cx,     cy,     1); // center dot
        ssd1306_drawPixel(cx + 1, cy,     1);
        ssd1306_drawPixel(cx,     cy + 1, 1);
        ssd1306_drawPixel(cx + 1, cy + 1, 1);
        ssd1306_update();

        sleep_until(deadline);
    }
}
