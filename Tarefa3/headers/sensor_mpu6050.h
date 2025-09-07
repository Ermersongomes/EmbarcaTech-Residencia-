#ifndef SENSOR_MPU6050_H
#define SENSOR_MPU6050_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <math.h>

// Configurações do sensor MPU6050 e do barramento I2C.
#define MPU6050_ADDR 0x68
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Protótipos das funções do driver do MPU6050.
void init_mpu6050();
void read_raw_data(int16_t accel[3], int16_t gyro[3], int16_t *temp);
void mpu6050_convert_to_g(int16_t accel_raw[3], float accel_g[3]);
void mpu6050_convert_to_dps(int16_t gyro_raw[3], float gyro_dps[3]);
float mpu6050_convert_to_celsius(int16_t temp_raw);

#endif