/**
 * @file    modbus.h
 * @brief   Modbus RTU通信协议驱动程序头文件
 * @details 本文件提供标准Modbus RTU协议的实现，包括主从机通信、
 *          寄存器读写操作和错误处理机制
 * @author  [yangming]
 * @date    2025-07-31
 * @version 1.0.0
 */

#ifndef MODBUS_H
#define MODBUS_H

#include "sys.h"
#include "uart.h"

/* Modbus从机地址定义 */
/**
 * @brief 默认Modbus从机地址
 * @details 当前设备作为Modbus从机时使用的地址标识
 */
#define modbusSLAVE_ADDRESS 0xc9

/* Modbus功能码定义 */
#define modbusCMD_READ_COILS                0x01
#define modbusCMD_READ_DISCRETE_INPUTS      0x02
#define modbusCMD_READ_HOLDING_REGISTERS    0x03
#define modbusCMD_READ_INPUT_REGISTERS      0x04
#define modbusCMD_WRITE_SINGLE_COIL         0x05
#define modbusCMD_WRITE_SINGLE_REGISTER     0x06
#define modbusCMD_WRITE_MULTIPLE_COILS      0x0F
#define modbusCMD_WRITE_MULTIPLE_REGISTERS  0x10
#define modbusCMD_READ_EXCEPTION_STATUS     0x07
#define modbusCMD_DIAGNOSTICS               0x08


/* Modbus帧格式参数定义 */
/**
 * @brief Modbus帧长度
 * @details 包含从机地址、功能码和CRC校验的帧长度
 */
#define modbusMIN_FRAME_SIZE                5
#define modbusMAX_FRAME_SIZE                255

/* Modbus寄存器地址映射定义 */
#define modbusHOLDING_REGISTERS_START       0x5000
#define modbusHOLDING_REGISTERS_END         (modbusHOLDING_REGISTERS_START + 0xFF)
#define modbusINPUT_REGISTERS_START         modbusHOLDING_REGISTERS_START
#define modbusINPUT_REGISTERS_END           modbusHOLDING_REGISTERS_END
#define modbusCOILS_START                   0x5100
#define modbusCOILS_END                     (modbusCOILS_START + 0xFF)


/* Modbus操作宏定义 */

/**
 * @brief 发送读取线圈状态帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 起始线圈地址
 * @param modbus_register_quantity 要读取的线圈数量 (1-2000)
 */
#define SendModbusReadCoilsFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity)     \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_COILS, modbus_start_address, NULL, modbus_register_quantity)

/**
 * @brief 发送读取保持寄存器帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 起始寄存器地址
 * @param modbus_register_quantity 要读取的寄存器数量 (1-125)
 */
#define SendModbusReadHoldingRegistersFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_HOLDING_REGISTERS, modbus_start_address, NULL, modbus_register_quantity)

/**
 * @brief 发送读取输入寄存器帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 起始寄存器地址
 * @param modbus_register_quantity 要读取的寄存器数量 (1-125)
 */
#define SendModbusReadInputRegistersFrame(uart, modbus_slave_address,modbus_start_address, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_READ_INPUT_REGISTERS, modbus_start_address, NULL, modbus_register_quantity)

/**
 * @brief 发送写入单个线圈帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 目标线圈地址
 * @param modbus_value 线圈值 (0x0000=OFF, 0xFF00=ON)
 */
#define SendModbusWriteSingleCoilFrame(uart, modbus_slave_address, modbus_start_address, modbus_value) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_SINGLE_COIL, modbus_start_address, NULL, modbus_value)

/**
 * @brief 发送写入单个寄存器帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 目标寄存器地址
 * @param modbus_value 寄存器值 (0x0000-0xFFFF)
 */
#define SendModbusWriteSingleRegisterFrame(uart, modbus_slave_address, modbus_start_address, modbus_value) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_SINGLE_REGISTER, modbus_start_address, NULL, modbus_value)

/**
 * @brief 发送写入多个线圈帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 起始线圈地址
 * @param frame 包含线圈数据的缓冲区指针
 * @param modbus_register_quantity 要写入的线圈数量 (1-1968)
 */
#define SendModbusWriteMultipleCoilsFrame(uart, modbus_slave_address, modbus_start_address, frame, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_MULTIPLE_COILS, modbus_start_address, frame, modbus_register_quantity)

/**
 * @brief 发送写入多个寄存器帧的宏定义
 * @param uart UART通信接口指针
 * @param modbus_slave_address 目标从机地址 (1-247)
 * @param modbus_start_address 起始寄存器地址
 * @param frame 包含寄存器数据的缓冲区指针
 * @param modbus_register_quantity 要写入的寄存器数量 (1-123)
 */
#define SendModbusWriteMultipleRegistersFrame(uart, modbus_slave_address, modbus_start_address, frame, modbus_register_quantity) \
    SendModbusGenericFrame(uart, modbus_slave_address, modbusCMD_WRITE_MULTIPLE_REGISTERS, modbus_start_address, frame, modbus_register_quantity)


/**
 * @brief Modbus错误代码枚举类型
 */
typedef enum {
    modbusERR_NONE = 0,                                         /**< 无错误 */
    modbusERR_ILLEGAL_FUNCTION,                                 /**< 非法功能码错误 */
    modbusERR_ILLEGAL_DATA_ADDRESS,                             /**< 非法数据地址错误 */
    modbusERR_ILLEGAL_DATA_VALUE,                               /**< 非法数据值错误 */
    modbusERR_SLAVE_DEVICE_FAILURE,                             /**< 从机设备故障错误 */
    modbusERR_ACKNOWLEDGE,                                      /**< 确认错误 */
    modbusERR_SLAVE_DEVICE_BUSY,                                /**< 从机设备忙错误 */
    modbusERR_MEMORY_PARITY_ERROR,                              /**< 内存奇偶校验错误 */
    modbusERR_GATEWAY_PATH_UNAVAILABLE,                         /**< 网关路径不可用错误 */
    modbusERR_GATEWAY_TARGET_DEVICE_FAILED_TO_RESPOND,          /**< 网关目标设备无响应错误 */
    modbusERR_CRC_RESULT,                                       /**< CRC校验错误 */
    modbusERR_SLAVE_ADDRESS,                                    /**< 从机地址错误 */
} ModbusErrorCode;

/**
 * @brief 标准Modbus RTU协议处理函数
 * @details 处理接收到的Modbus RTU帧，解析命令并执行相应操作
 * @param[in] uart UART通信接口指针，不能为NULL
 * @param[in] frame 接收到的Modbus帧数据缓冲区指针，不能为NULL
 * @param[in] len 帧数据长度，必须大于等于modbusMIN_FRAME_SIZE
 * @return 无
 * @pre len必须在有效范围内 (modbusMIN_FRAME_SIZE <= len <= modbusMAX_FRAME_SIZE)
 * @note 函数会自动进行CRC校验和从机地址验证
 */
void UartStandardModbusRTUProtocal(UART_TYPE *uart,uint8_t *frame, uint16_t len);

/**
 * @brief 发送通用Modbus帧
 * @details 构造并发送标准格式的Modbus RTU帧，支持各种功能码操作
 * @param[in] uart UART通信接口指针
 * @param[in] modbus_slave_address 目标从机地址 (1-247)
 * @param[in] modbus_command Modbus功能码 
 * @param[in] modbus_start_address 起始地址 (0x0000-0xFFFF)
 * @param[in] frame 数据缓冲区指针，可以为NULL (取决于功能码)
 * @param[in] modbus_register_quantity 寄存器/线圈数量或者单个寄存器值
 * @return 无
 * @pre 对于写操作，frame不能为NULL且必须包含有效数据
 * @note 函数会自动计算并添加CRC校验码
 * @note 支持单个和多个寄存器/线圈的读写操作
 */
void SendModbusGenericFrame(UART_TYPE *uart, 
                            uint16_t modbus_slave_address,
                            uint8_t modbus_command, 
                            uint16_t modbus_start_address, 
                            uint8_t *frame, 
                            uint16_t modbus_register_quantity);





















#endif 