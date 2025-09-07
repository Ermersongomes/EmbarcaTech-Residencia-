#include "headers/sensor_mpu6050.h"
#include <stdio.h>

// Registradores do MPU6050.
#define REG_ACCEL_XOUT_H 0x3B // Acelerômetro
#define REG_TEMP_OUT_H   0x41 // Temperatura
#define REG_GYRO_XOUT_H  0x43 // Giroscópio
#define REG_PWR_MGMT_1   0x6B // Gerenciamento de energia

// Fatores de conversão para as unidades padrão.
#define ACCEL_FS_SENSITIVITY 16384.0f // LSB/g
#define GYRO_FS_SENSITIVITY  131.0f   // LSB/(°/s)

// Inicializa o MPU6050, retirando-o do modo de repouso.
void init_mpu6050() {
    uint8_t buf[] = {REG_PWR_MGMT_1, 0x00};
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
}

// Lê os dados brutos de acelerômetro, giroscópio e temperatura.
void read_raw_data(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[14];
    
    // Lê 14 bytes a partir do registrador do acelerômetro.
    uint8_t reg = REG_ACCEL_XOUT_H;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 14, false);

    // Converte os bytes lidos em valores de 16 bits.
    accel[0] = (buffer[0] << 8) | buffer[1];
    accel[1] = (buffer[2] << 8) | buffer[3];
    accel[2] = (buffer[4] << 8) | buffer[5];

    *temp = (buffer[6] << 8) | buffer[7];

    gyro[0] = (buffer[8] << 8) | buffer[9];
    gyro[1] = (buffer[10] << 8) | buffer[11];
    gyro[2] = (buffer[12] << 8) | buffer[13];
}

// Converte os valores brutos do acelerômetro para 'g'.
void mpu6050_convert_to_g(int16_t accel_raw[3], float accel_g[3]) {
    accel_g[0] = accel_raw[0] / ACCEL_FS_SENSITIVITY;
    accel_g[1] = accel_raw[1] / ACCEL_FS_SENSITIVITY;
    accel_g[2] = accel_raw[2] / ACCEL_FS_SENSITIVITY;
}

// Converte os valores brutos do giroscópio para graus por segundo (°/s).
void mpu6050_convert_to_dps(int16_t gyro_raw[3], float gyro_dps[3]) {
    gyro_dps[0] = gyro_raw[0] / GYRO_FS_SENSITIVITY;
    gyro_dps[1] = gyro_raw[1] / GYRO_FS_SENSITIVITY;
    gyro_dps[2] = gyro_raw[2] / GYRO_FS_SENSITIVITY;
}

// Converte o valor bruto da temperatura para graus Celsius (°C).
float mpu6050_convert_to_celsius(int16_t temp_raw) {
    // Fórmula baseada no datasheet do MPU6050.
    return (temp_raw / 340.0f) + 36.53f;
}