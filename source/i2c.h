/**
 * @file    i2c.h
 * @brief   I2C通信接口驱动程序头文件
 * @details 本文件提供I2C总线通信的标准接口函数，包括单字节和多字节的读写操作
 * @author  yangming
 * @version 1.0.0
 */

#ifndef I2C_H
#define I2C_H

#include "T5LOSConfig.h"

/**
 * @brief I2C延时宏定义
 * @details 用于I2C时序控制的微秒级延时宏，确保I2C通信时序符合协议要求
 * @param t 延时时间，单位为微秒
 * @note 该宏基于delay_us函数实现，用于I2C总线时序控制
 */
#define i2cDelay(t)    delay_us(t)

/**
 * @brief I2C写入单个字节数据
 * @details 向指定的寄存器地址写入一个字节的数据
 * @param[in] register_address 目标寄存器地址 (0x00-0xFF)
 * @param[in] register_data    要写入的数据字节 (0x00-0xFF)
 * @return 无
 */
void I2cWriteSingleByte(uint8_t register_address, uint8_t register_data);

/**
 * @brief I2C写入多个字节数据
 * @details 从指定的起始寄存器地址开始，连续写入多个字节的数据
 * @param[in] register_address 起始寄存器地址 (0x00-0xFF)
 * @param[in] buffer          指向待写入数据缓冲区的指针
 * @param[in] length          要写入的数据长度，单位为字节 (1-255)
 * @return 无
 */
void I2cWriteMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length);

/**
 * @brief I2C读取单个字节数据
 * @details 从指定的寄存器地址读取一个字节的数据
 * @param[in] register_address 目标寄存器地址 (0x00-0xFF)
 * @return 读取到的8位数据值
 */
uint8_t I2cReadSingleByte(uint8_t register_address);

/**
 * @brief I2C读取多个字节数据
 * @details 从指定的起始寄存器地址开始，连续读取多个字节的数据
 * @param[in]  register_address 起始寄存器地址 (0x00-0xFF)
 * @param[out] buffer          指向存储读取数据的缓冲区指针
 * @param[in]  length          要读取的数据长度，单位为字节 (1-255)
 * @return 无
 */
void I2cReadMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length);

#endif /* I2C_H */
