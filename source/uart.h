/**
 * @file    uart.h
 * @brief   UART通信接口驱动程序头文件
 * @details 本文件提供多路UART通信接口的管理和控制功能，支持UART2-UART5，
 *          包括数据收发、缓冲区管理和协议处理
 * @author  [yangming]
 * @date    2025-07-31
 * @version 1.0.0
 */

#ifndef __UART_H__
#define __UART_H__
#include "T5LOSConfig.h"

/**
 * @brief UART控制结构体定义
 * @details 定义了UART通信所需的所有控制参数和状态信息
 */
typedef struct UartxDefine
{
    uint16_t TxHead;                                            /**< 发送缓冲区头指针 */
    uint16_t TxTail;                                            /**< 发送缓冲区尾指针 */

    uint16_t RxHead;                                            /**< 接收缓冲区头指针 */
    uint16_t RxTail;                                            /**< 接收缓冲区尾指针 */

    uint8_t RxTimeout;                                          /**< 接收超时计数器 */

    uint8_t RxFlag:2;                                           /**< 接收状态标志位(2位) */
    uint8_t TxBusy:1;                                           /**< 发送忙碌标志位(1位) */
}UART_TYPE;

/* UART接收状态定义 */
/**
 * @brief UART接收状态
 * @details 0:UART当前没有数据接收活动 1:UART当前正在接收数据
 */
#define  UART_NON_REC    0
#define  UART_RECING     1

/* UART接口声明 - 根据配置条件编译 */

#if uartUART2_ENABLED
/**
 * @brief UART2控制结构体外部声明
 */
extern UART_TYPE Uart2;

/**
 * @brief UART2初始化函数
 * @details 初始化UART2硬件接口，配置波特率、数据位、停止位等参数
 * @param[in] bdt 波特率设置值 (9600-921600)
 * @return 无
 * @note 函数会自动配置GPIO引脚和中断设置
 * @note 支持RS485模式配置（如果启用）
 */
void Uart2Init(const uint32_t bdt);
#endif

#if uartUART3_ENABLED
/**
 * @brief UART3控制结构体外部声明
 */
extern UART_TYPE Uart3;

/**
 * @brief UART3初始化函数
 * @details 初始化UART3硬件接口，配置波特率、数据位、停止位等参数
 * @param[in] bdt 波特率设置值 (9600-921600)
 * @return 无
 * @note 函数会自动配置GPIO引脚和中断设置
 * @note 使用固定时钟源6451200Hz计算波特率
 */
void Uart3Init(const uint32_t bdt);
#endif

#if uartUART4_ENABLED
/**
 * @brief UART4控制结构体外部声明
 */
extern UART_TYPE Uart4;

/**
 * @brief UART4初始化函数
 * @details 初始化UART4硬件接口，配置波特率、数据位、停止位等参数
 * @param[in] bdt 波特率设置值 (9600-921600)
 * @return 无
 * @note 函数会自动配置GPIO引脚和中断设置
 * @note 支持RS485模式配置（如果启用）
 * @note 具有独立的收发中断处理
 */
void Uart4Init(const uint32_t bdt);
#endif

#if uartUART5_ENABLED
/**
 * @brief UART5控制结构体外部声明
 */
extern UART_TYPE Uart5;

/**
 * @brief UART5初始化函数
 * @details 初始化UART5硬件接口，配置波特率、数据位、停止位等参数
 * @param[in] bdt 波特率设置值 (9600-921600)
 * @return 无
 * @note 函数会自动配置GPIO引脚和中断设置
 * @note 支持RS485模式配置（如果启用）
 * @note 具有独立的收发中断处理
 */
void Uart5Init(const uint32_t bdt);
#endif

/**
 * @brief UART模块统一初始化函数
 * @details 根据配置初始化所有启用的UART接口，使用预设波特率
 * @param 无
 * @return 无
 * @note 函数内部调用各个UARTx的初始化函数
 * @note 使用配置文件中预设的波特率参数
 */
void UartInit(void);

/**
 * @brief UART数据发送函数
 * @details 将数据添加到指定UART的发送缓冲区并启动发送过程
 * @param[in] uart 目标UART控制结构体指针，不能为NULL
 * @param[in] buf 待发送数据缓冲区指针，不能为NULL
 * @param[in] len 待发送数据长度，单位为字节 (1-65535)
 * @return 无
 * @pre len必须大于0且不超过发送缓冲区剩余空间
 * @note 函数使用中断方式发送数据，非阻塞调用
 * @note 支持RS485自动方向控制（如果启用）
 * @warning 调用者必须确保缓冲区有足够空间
 * @warning 发送过程中不要修改源数据缓冲区
 */
void UartSendData(UART_TYPE *uart, uint8_t *buf,uint16_t len);

/**
 * @brief UART数据帧读取函数
 * @details 从指定UART的接收缓冲区读取完整数据帧并进行协议解析
 * @param[in] uart 目标UART控制结构体指针，不能为NULL
 * @return 无
 * @post 接收到的数据帧已被处理和解析
 * @note 函数支持多种协议自动识别和处理
 * @note 支持Dwin8283协议和Modbus RTU协议
 * @note 使用超时机制判断帧结束
 * @warning 函数会修改UART的接收状态标志
 * @warning 未完成的数据帧将被丢弃
 */
void UartReadFrame(UART_TYPE *uart);

#endif /* __UART_H__ */
