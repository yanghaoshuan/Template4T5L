#ifndef I2C_H
#define I2C_H

#include "T5LOSConfig.h"

#define i2cDelay(t)    delay_us(t)

void I2cWriteSingleByte(uint8_t register_address, uint8_t register_data);
void I2cWriteMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length);
uint8_t I2cReadSingleByte(uint8_t register_address);
void I2cReadMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length);

#endif
