#ifndef _TA_PROTICAL_H_
#define _TA_PROTICAL_H_

#include "uart.h"
#include "T5LOSConfig.h"
#if uartTA_PROTOCOL_ENABLED
/**
 * @brief 处理TA指令集协议帧
 * @param uart 串口类型指针
 * @param frame 协议帧数据
 * @param len 协议帧长度
 * @note 包含帧头和帧尾的完整的帧
 */
void UartStandardTAProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t len);


/**
 * @brief TA协议触摸上传任务
 * @param uart 串口类型指针
 * @note 定期调用以检测触摸状态变化并上传
 */
void TAProtocolUpload(UART_TYPE *uart);

#endif /* uartTA_PROTOCOL_ENABLED */

#endif /* _TA_PROTICAL_H_ */