#include "mpu6050.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define PWR_MGMT_1   0x6B
#define ACCEL_CONFIG 0x1C
#define GYRO_CONFIG  0x1B
#define ACCEL_XOUT_H 0x3B
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

void mpu6050_init(void) {
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(MPU6050_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU6050_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPU6050_SDA_PIN);
    gpio_pull_up(MPU6050_SCL_PIN);

    imu_addr = 0x68;
    uint8_t who = imu_read_reg(WHO_AM_I);
    if (who != 0x68 && who != 0x98) {
        imu_addr = 0x69;
    }
    imu_write(PWR_MGMT_1,   0x00); // wake up
    imu_write(ACCEL_CONFIG, 0x00); // ±2g  (16384 LSB/g)
    imu_write(GYRO_CONFIG,  0x18); // ±2000 dps
}

void mpu6050_read(mpu6050_data_t *d) {
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
