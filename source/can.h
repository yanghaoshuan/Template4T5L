#ifndef CAN_H
#define CAN_H


#include "T5LOSConfig.h"

#if canCAN_ENABLED

/**
 * D3 1 BRP 
 * D2 1 BTR0 
 * D1 1 BTR1 0xFF:0060 
 * D0 1 未定义，写 0。 
 * BRP：波特率分频寄存器。 
 * BTR0：[7:5]为同步跳转宽度 sjw，[3:0] prop 传播时间段 T0。 
 * BTR1：[7:4]为相位缓冲段 T1，[3:0]相位缓冲段 2，T2。 
 * T0+T1+T2 = CPU 主频/（波特率*（BRP*2+1））-4
 */
#define canBAUD_RATE_250K       0x1120F400  /**< 250K波特率配置 */

#define canCAN_ACR              0x00000000        /**< CAN接收滤波器掩码寄存器 */
#define canCAN_AMR              0x00000000        /**< CAN接收滤波器标识符寄存器 */

/* CAN消息结构体定义 */
/**
 * @brief CAN消息结构体
 * @details 定义CAN消息的完整格式，包括标识符、数据长度和数据内容
 */
typedef struct
{
    uint32_t id;                        /**< CAN标识符（11位标准或29位扩展） */
    uint8_t frame_type;                 /**< 帧类型：标准帧/扩展帧 */
    uint8_t data_type;                  /**< 数据类型：数据帧/远程帧 */
    uint8_t dlc;                        /**< 数据长度代码（0-8字节） */
    uint8_t data[8];                    /**< 数据内容，最大8字节 */
} CanMessage;

#define CAN_MESSAGE_QUEUE_SIZE      30  /**< CAN消息队列大小 */
CanMessage can_message[CAN_MESSAGE_QUEUE_SIZE];  /**< 全局CAN消息变量 */
extern uint8_t can_message_rxhead;  /**< CAN消息队列头指针 */
extern uint8_t can_message_rxtail;  /**< CAN消息队列尾指针 */
void CanInit(void);
void CanTask(void);

#endif /* canCAN_ENABLED */
#endif /* CAN_H */