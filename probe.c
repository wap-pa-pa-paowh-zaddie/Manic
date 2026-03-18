#include <stdio.h> //for printing error messages
#include <stdlib.h> //for exit codes
#include <stdint.h> //for exact integer sizes like int16_t and uint8_t
#include <unistd.h> //standard Unix header for file handling
#include <fcntl.h>  // also for file handling (mainly open() function)
#include <errno.h> //produce readable error messages
#include <string.h> 
#include <sys/types.h> //for ssize_t (which is useful for read() and write() functions)
#include <sys/ioctl.h> //input output control, the ioctl() function
#include <linux/i2c-dev.h> //Linux I2C interface

#include "common.h" //accessing declarations and structs of common.h


static int open_i2c_bus(const char* path) // opens i2c
{
	int fd = open(path, O_RDWR); //opening the device file in read and write mode
	if(fd < 0)
	{
		perror("Couldn't open the I2C bus! ");
		return -1;
	}
	return fd;
}

static int set_i2c_slave(int fd, int addr) //sets the slave, which the receiver of the message passed thru the I2C bus
{
	if(ioctl(fd, I2C_SLAVE, addr) < 0) //I2C_SLAVE is a macro in unistd.h
	{
		perror("The I2C slave couldn't be set! ");
		return -1;
	}
	return 0;
}

static int write_register(int fd, uint8_t reg, uint8_t value) // reg itself is the address of the register
{
	uint8_t buffer[2]; //the contents which get sent to be written on the register
	buffer[0] = reg; //stores address of the register on which content is to be written
	buffer[1] = value; // stores value to be written onto the register

	ssize_t written = write(fd, buffer, 2); //write returns integers, but apparently it's safer to use ssize_t as the data type
	if (written != 2) //write returns the no. of bytes written
	{
		perror("Couldn't write onto the register!! ");
		return -1;
	}
	return 0;
}

static int read_registers(int fd, uint8_t start_reg, uint8_t *buffer, size_t len) 
{
    ssize_t written = write(fd, &start_reg, 1); //writing the address of the register to be read
    if (written != 1) 
    {
        perror("Couldn't select the register! ");
        return -1;
    }

    ssize_t rd = read(fd, buffer, len); //reading from the address written using write() 
    if (rd != (ssize_t)len) 
    {
        perror("Couldn't read from the register! ");
        return -1;
    }

    return 0;
}

static int16_t combine_bytes(uint8_t high, uint8_t low) 
{
    return (int16_t)((high << 8) | low); //its a sort of concatenation of bytes using bitwise and shift operators
}

static int read_sensor_data(int fd, sensor_data_t *data) // using pointer so that we can implement pass by reference for the structure
{
    uint8_t raw[8];

    if (read_registers(fd, MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw)) < 0) 
        return -1;

    (*data).accel_x = combine_bytes(raw[0], raw[1]); //high and low
    (*data).accel_y = combine_bytes(raw[2], raw[3]); //high and low
    (*data).accel_z = combine_bytes(raw[4], raw[5]); //high and low

    if (read_registers(fd, MPU6050_REG_GYRO_ZOUT_H, raw, 2) < 0) 
        return -1;

    (*data).gyro_z = combine_bytes(raw[0], raw[1]); //high and low, this is another iteration 

    return 0;
}

int main() 
{
    int i2c_fd;
    sensor_data_t data;

    i2c_fd = open_i2c_bus("/dev/i2c-1"); //passing device file to open function
    if (i2c_fd < 0) 
    {
        return EXIT_FAILURE; //macro constant from stdlib.h
    }

    if (set_i2c_slave(i2c_fd, MPU6050_ADDR) < 0) 
    {
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    if (write_register(i2c_fd, MPU6050_REG_PWR_MGMT_1, 0x00) < 0) //good morning sensor! wake up 🥰
    {
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    while (1) 
    {
        if (read_sensor_data(i2c_fd, &data) < 0) 
        {
            close(i2c_fd);
            return EXIT_FAILURE; // a macro constant in stdlib.h
        }

        if (write(STDOUT_FILENO, &data, sizeof(data)) != (ssize_t)sizeof(data)) //STDOUT_FILENO is standard output
        {                                                                       //we write to standard output so that the probe can connect this to a pipe
            perror("Probe: write to controller failed! ");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        usleep(100000); //small delay
    }

    close(i2c_fd);
    return EXIT_SUCCESS;
}