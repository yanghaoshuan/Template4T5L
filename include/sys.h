/**
 * @file    sys.h
 * @brief   System utilities and hardware abstraction layer for T5L platform
 * @author  yangming
 * @date    2025-07-29
 * @version 1.0.0
 */

#ifndef SYS_H
#define SYS_H

#include "T5LOSConfig.h"


/**
 * @brief Microsecond delay function
 * @details Provides a blocking delay for the specified number of microseconds
 * @param[in] us The delay time in microseconds (0-65535)
 * @return None
 * @pre us parameter must be a valid 16-bit value
 * @post System will be delayed for approximately the specified microseconds
 * @note This function is blocking and should not be used in time-critical sections
 */
void delay_us(const uint16_t us);

/**
 * @brief Millisecond delay function
 * @details Provides a blocking delay for the specified number of milliseconds
 * @param[in] ms The delay time in milliseconds (0-65535)
 * @return None
 * @pre ms parameter must be a valid 16-bit value
 * @post System will be delayed for approximately the specified milliseconds
 * @note This function is blocking and should not be used in time-critical sections
 */
void delay_ms(const uint16_t ms);

/**
 * @brief Calculate 16-bit CRC checksum
 * @details Computes a 16-bit Cyclic Redundancy Check for the given data buffer
 * @param[in] pBuf Pointer to the data buffer for CRC calculation
 * @param[in] buf_len Length of the data buffer in bytes
 * @return 16-bit CRC value
 * @pre pBuf must point to a valid memory location
 * @pre buf_len must be greater than 0 and not exceed buffer size
 * @post Returns computed CRC-16 value
 * @note Uses standard CRC-16 algorithm
 */
uint16_t crc_16(uint8_t *pBuf, uint16_t buf_len);

/**
 * @brief Read data from DGUS display variable pointer
 * @details Reads specified length of data from DGUS display at given address
 * @param[in] addr The variable pointer address in DGUS display memory
 * @param[out] buf Pointer to buffer where read data will be stored
 * @param[in] len Number of bytes to read (must not exceed buffer size)
 * @return None
 * @pre addr must be a valid DGUS memory address
 * @pre buf must point to a valid memory location with sufficient space
 * @pre len must be greater than 0 and not exceed buffer capacity
 * @post Data from DGUS display will be stored in buf
 */
void read_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);

/**
 * @brief Write data to DGUS display variable pointer
 * @details Writes specified length of data to DGUS display at given address
 * @param[in] addr The variable pointer address in DGUS display memory
 * @param[in] buf Pointer to buffer containing data to be written
 * @param[in] len Number of bytes to write (must not exceed buffer size)
 * @return None
 * @pre addr must be a valid DGUS memory address
 * @pre buf must point to a valid memory location containing data
 * @pre len must be greater than 0 and not exceed buffer size
 * @post Data from buf will be written to DGUS display memory
 */
void write_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);

/**
 * @brief Initialize T5L CPU and peripherals
 * @details Performs complete initialization of the T5L CPU including:
 *          - Clock configuration
 *          - GPIO initialization
 *          - Peripheral setup
 *          - System timer configuration
 * @param None
 * @return None
 * @pre System must be in reset state
 * @post CPU and peripherals will be properly configured for operation
 * @note This function should be called once at system startup
 */
void T5LCpuInit(void);

#endif /* SYS_H */