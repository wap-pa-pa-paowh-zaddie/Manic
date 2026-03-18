#ifndef COMMON_H
#define COMMON_H

#include <stdint.h> //for including fixed-width integer types

//1. Sensor constants

#define MPU6050_ADDR 0x68 //address of mpu6050
#define MPU6050_REG_PWR_MGMT_1 0x6B //address of the register which tells it to wake up
#define MPU6050_REG_ACCEL_XOUT_H 0x3B //address of the register which contains the x coord of accelerometer data
#define MPU6050_REG_GYRO_ZOUT_H 0x47 //address of the register which contains the z coord of gyroscope data

//2. PWM assumptions
#define DEFAULT_PWM_PERIOD_NS 20000000

//3. Shared sensor packet
typedef struct 
{
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int16_t gyro_z;
} sensor_data_t;

#endif