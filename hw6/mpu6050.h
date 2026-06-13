#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#define MPU6050_SDA_PIN 20
#define MPU6050_SCL_PIN 21

typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} mpu6050_data_t;

void mpu6050_init(void);
void mpu6050_read(mpu6050_data_t *d);

#endif
