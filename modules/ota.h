/**
 * @file    ota.h
 * @brief   OTA升级模块头文件
 * @details 本文件提供V851/R11 OTA升级协议接口，支持升级文件信息接收、
 *          分包下载、NAND写入、升级头文件生成和升级完成后重启触发。
 *          OTA协议帧通过UART4或旧Uart_R11接收，帧头为AB CD。
 * @author  yangming
 * @version 1.0.0
 */

#ifndef OTA_H
#define OTA_H

#include "sys.h"

#if otaOTA_ENABLED

/**
 * @brief OTA单个文件信息结构体
 * @details 保存一次OTA升级中某个文件的类型、编号、大小、校验和NAND起始块信息。
 */
typedef struct
{
    uint8_t itype;             /**< 文件类型: 1=LIB, 2=BIN, 3=DZK, 4=HZK, 5=ICL, 6=WAE, 7=UIC, 8=GTF */
    uint8_t apply;             /**< 业务编号，由服务器协议下发 */
    uint16_t unid;             /**< 文件编号，例如16.ICL对应编号16 */
    uint32_t size;             /**< 文件字节大小 */
    uint32_t crc32;            /**< 文件CRC32校验值 */
    uint16_t flash_start;      /**< 文件在NAND中的起始位置，单位为4KB块 */
} OtaFileInfo;

/**
 * @brief OTA运行上下文结构体
 * @details 保存一次OTA会话中的文件总数、当前文件、下载偏移和所有文件描述信息。
 */
typedef struct
{
    uint8_t download_end_flag;                     /**< 下载完成状态标志: 0=未完成, 0x01=等待确认, 0x11=执行升级 */
    uint32_t all_size;                             /**< 本次OTA所有文件总字节数，用于计算进度 */
    uint8_t total_num;                             /**< 本次OTA文件总数 */
    uint8_t now_num;                               /**< 当前正在处理的文件序号 */
    uint16_t flash_start_num;                      /**< 下一个文件可用NAND起始块，单位为4KB块 */
    uint32_t off_position;                         /**< 当前文件下载偏移，单位为字节 */
    uint32_t off_len;                              /**< 单次请求长度，默认4096字节 */
    OtaFileInfo file[otaDOWNLOAD_MAX];             /**< OTA文件信息缓存 */
} OtaContext;

extern OtaContext OtaStatus;       /**< OTA全局运行上下文 */
extern uint16_t OtaTimeout;        /**< OTA超时计数，10ms递减一次 */
extern uint8_t OtaStep;            /**< OTA当前等待步骤 */
extern uint8_t OtaCompleteFlag;    /**< OTA升级完成标志，用于写入NOR Flash */

/**
 * @brief OTA上下文初始化函数
 * @details 清空OTA运行上下文，并将NAND数据起始块初始化为otaDATA_START_BLOCK。
 * @param 无
 * @return 无
 * @post OTA运行状态回到空闲状态
 */
void OtaInit(void);

/**
 * @brief OTA协议帧接收处理函数
 * @details 处理AB CD协议帧，根据命令号执行文件信息接收、数据包处理或完成确认。
 * @param[in] frame OTA协议帧缓冲区指针，不能为NULL
 * @param[in] len OTA协议帧总长度，单位为字节
 * @return 无
 * @note 本函数由UartReadFrame在识别到AB CD帧后调用
 */
void OtaReceive(uint8_t *frame, uint16_t len);

/**
 * @brief OTA周期任务函数
 * @details 处理OTA测试触发、超时重发和下载完成后的升级头文件生成。
 * @param 无
 * @return 无
 * @note 建议按otaTASK_INTERVAL注册到系统任务调度器
 */
void OtaTask(void);

/**
 * @brief OTA业务默认数据初始化函数
 * @details 按参考工程行为写入版本号、默认时间段、RTC设置触发数据和升级完成标志显示。
 * @param 无
 * @return 无
 * @note 函数会访问DGUS变量地址和NOR Flash，建议在系统初始化完成后调用
 */
void OtaDataInit(void);

/**
 * @brief OTA定时计数函数
 * @details 由1ms定时器调用，内部每10ms递减一次OTA超时计数。
 * @param 无
 * @return 无
 */
void OtaTimerTick1ms(void);

/**
 * @brief OTA进度显示函数
 * @details 通过otaSPEED_SP_ADDR和otaSPEED_VP_ADDR控制进度显示和进度值。
 * @param[in] speed_num 进度百分比，0表示关闭进度显示
 * @return 无
 */
void OtaSpeedShow(uint8_t speed_num);

/**
 * @brief Clear the persistent post-upgrade completion flag after V851 receives SUCCESS.
 */
void OtaAcknowledgeComplete(void);

#endif /* otaOTA_ENABLED */

#endif /* OTA_H */
