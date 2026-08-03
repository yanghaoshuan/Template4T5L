/**
 * @file    ota.c
 * @brief   OTA升级模块实现文件
 * @details 本文件实现AB CD协议OTA状态机，负责向V851/R11请求升级数据、
 *          校验分包CRC16、写入NAND Flash、校验整文件CRC32、
 *          生成4KB升级头文件并触发T5L Boot升级流程。
 * @author  yangming
 * @version 1.0.0
 */

#include "ota.h"

#if otaOTA_ENABLED

#include "uart.h"
#if v851PROTOCOL_ENABLED
#include "v851_protocol.h"
#endif
#include <string.h>

/* OTA协议帧头和命令定义 */
#define OTA_FRAME_HEAD_0             0xAB
#define OTA_FRAME_HEAD_1             0xCD
#define OTA_CMD_FILE_INFO            0x04
#define OTA_CMD_READ_DATA            0x05
#define OTA_CMD_FILE_RESULT          0x06

/* OTA内部状态定义 */
#define OTA_STEP_IDLE                0xFF
#define OTA_STEP_WAIT_DATA           0x50
#define OTA_STEP_WAIT_RESULT_ACK     0x62

/* OTA固定数据块和DGUS系统变量地址定义 */
#define OTA_PACKET_BYTES             4096UL
#define OTA_HEADER_BYTES             4096U
#define OTA_HEADER_WORDS             2048U
#define OTA_TIMEOUT_RELOAD           200U
#define OTA_NAND_CMD_ADDR            0x00AA
#define OTA_NAND_CRC_ADDR            0x00AE
#define OTA_BOOT_CMD_ADDR            0x0004
#define OTA_RTC_SET_ADDR             0x009C
#define OTA_DEVICE_CODE_ADDR         0x3004

#define OTA_FILE_INFO_OFFSET         16U
#define OTA_FILE_INFO_SIZE           4U
#define OTA_FILE_INFO_COUNT          (256U + 128U)

OtaContext OtaStatus;                /**< OTA全局运行上下文 */
uint16_t OtaTimeout;                 /**< OTA超时计数，10ms递减一次 */
uint8_t OtaStep = OTA_STEP_IDLE;     /**< OTA当前等待步骤 */
uint8_t OtaCompleteFlag;             /**< OTA完成标志 */

static uint16_t xdata OtaTimeoutReload;             /**< OTA超时重装值 */
static uint8_t xdata OtaHeaderBuffer[OTA_HEADER_BYTES]; /**< OTA 4KB头文件缓存 */
static uint8_t OtaLastResult = 2U;                  /**< 06命令最近一次回复结果，用于超时重发 */
static uint8_t OtaLastReportedProgress;             /**< UART4 TLV进度限频 */

/**
 * @brief 读取大端16位整数
 * @param[in] buf 数据缓冲区指针
 * @return 大端解析后的16位整数
 */
static uint16_t OtaReadBe16(uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/**
 * @brief 读取小端16位整数
 * @param[in] buf 数据缓冲区指针
 * @return 小端解析后的16位整数
 */
static uint16_t OtaReadLe16(uint8_t *buf)
{
    return ((uint16_t)buf[1] << 8) | (uint16_t)buf[0];
}

/**
 * @brief 读取小端32位整数
 * @param[in] buf 数据缓冲区指针
 * @return 小端解析后的32位整数
 */
static uint32_t OtaReadLe32(uint8_t *buf)
{
    return ((uint32_t)buf[3] << 24) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[1] << 8) |
           (uint32_t)buf[0];
}

/**
 * @brief 写入大端16位整数
 * @param[out] buf 目标数据缓冲区指针
 * @param[in] value 待写入的16位整数
 * @return 无
 */
static void OtaWriteBe16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)value;
}

/**
 * @brief 写入小端16位整数
 * @param[out] buf 目标数据缓冲区指针
 * @param[in] value 待写入的16位整数
 * @return 无
 */
static void OtaWriteLe16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
}

/**
 * @brief 写入大端32位整数
 * @param[out] buf 目标数据缓冲区指针
 * @param[in] value 待写入的32位整数
 * @return 无
 */
static void OtaWriteBe32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

/**
 * @brief 写入小端32位整数
 * @param[out] buf 目标数据缓冲区指针
 * @param[in] value 待写入的32位整数
 * @return 无
 */
static void OtaWriteLe32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
}

/**
 * @brief 32位整数向上取整除法
 * @details 用于将文件大小换算为4KB块数或升级文件占用块数。
 * @param[in] value 被除数
 * @param[in] divisor 除数，不能为0
 * @return 向上取整后的结果，超过uint16_t范围时返回0xFFFF
 */
static uint16_t OtaCeilDiv32(uint32_t value, uint32_t divisor)
{
    uint32_t result;

    if(value == 0UL)
    {
        return 1U;
    }

    result = value / divisor;
    if((value % divisor) != 0UL)
    {
        result++;
    }

    if(result > 0xFFFFUL)
    {
        result = 0xFFFFUL;
    }

    return (uint16_t)result;
}

/**
 * @brief 设置OTA等待状态和超时时间
 * @param[in] step 新的OTA等待状态
 * @return 无
 */
static void OtaSetTimeout(uint8_t step)
{
    OtaStep = step;
    OtaTimeoutReload = OTA_TIMEOUT_RELOAD;
    OtaTimeout = OtaTimeoutReload;
}

/**
 * @brief 发送OTA协议帧
 * @details 自动回填AB CD帧中的大端长度字段，并通过UART4或旧Uart_R11发送。
 * @param[in,out] buf 待发送协议帧缓冲区
 * @param[in] len 协议帧总长度，单位为字节
 * @return 无
 */
static void OtaSendFrame(uint8_t *buf, uint16_t len)
{
    OtaWriteBe16(&buf[2], len - 4U);
    #if v851PROTOCOL_ENABLED
    (void)V851ProtocolSendOtaFrame(buf, len);
    #else
    UartSendData(&Uart_R11, buf, len);
    #endif
}

/**
 * @brief 发送05数据请求命令
 * @details 向R11请求当前文件指定偏移处的升级数据。
 * @param 无
 * @return 无
 */
static void OtaSendData05(void)
{
    uint8_t send_buf[32];
    uint16_t send_len = 0U;
    OtaFileInfo *file;

    file = &OtaStatus.file[OtaStatus.now_num];

    send_buf[send_len++] = OTA_FRAME_HEAD_0;
    send_buf[send_len++] = OTA_FRAME_HEAD_1;
    send_buf[send_len++] = 0U;
    send_buf[send_len++] = 0U;
    send_buf[send_len++] = OTA_CMD_READ_DATA;
    send_buf[send_len++] = file->itype;
    send_buf[send_len++] = file->apply;
    OtaWriteLe16(&send_buf[send_len], file->unid);
    send_len += 2U;
    OtaWriteLe32(&send_buf[send_len], OtaStatus.off_position);
    send_len += 4U;
    OtaWriteLe32(&send_buf[send_len], OtaStatus.off_len);
    send_len += 4U;

    OtaSendFrame(send_buf, send_len);
}

/**
 * @brief 发送06文件处理结果命令
 * @param[in] result 文件处理结果，2=成功，3=失败
 * @return 无
 */
static void OtaSendData06(uint8_t result)
{
    uint8_t send_buf[16];
    uint16_t send_len = 0U;
    OtaFileInfo *file;

    OtaLastResult = result;
    file = &OtaStatus.file[OtaStatus.now_num];

    send_buf[send_len++] = OTA_FRAME_HEAD_0;
    send_buf[send_len++] = OTA_FRAME_HEAD_1;
    send_buf[send_len++] = 0U;
    send_buf[send_len++] = 0U;
    send_buf[send_len++] = OTA_CMD_FILE_RESULT;
    send_buf[send_len++] = file->itype;
    send_buf[send_len++] = file->apply;
    OtaWriteLe16(&send_buf[send_len], file->unid);
    send_len += 2U;
    send_buf[send_len++] = result;

    OtaSendFrame(send_buf, send_len);
}

/**
 * @brief 等待NAND操作空闲
 * @details 轮询DGUS系统变量0x00AA，直到NAND命令执行完成。
 * @param 无
 * @return 无
 * @warning 本函数会阻塞等待NAND命令完成
 */
static void OtaWaitNandIdle(void)
{
    uint8_t cmd_state[4];

    do
    {
        read_dgus_vp(OTA_NAND_CMD_ADDR, cmd_state, 2);
        if(cmd_state[0] != 0U)
        {
            delay_ms(2);
        }
    }while(cmd_state[0] != 0U);
}

/**
 * @brief 启动NAND写入命令
 * @param[in] nand_addr NAND目标地址，单位为字节地址
 * @param[in] vp_addr DGUS数据缓存VP地址
 * @param[in] block_num 写入4KB块数量
 * @return 无
 */
static void OtaStartNandWrite(uint32_t nand_addr, uint16_t vp_addr, uint16_t block_num)
{
    uint8_t cmd[12];

    cmd[0] = 0x5A;
    cmd[1] = 0x04;
    OtaWriteBe32(&cmd[2], nand_addr);
    OtaWriteBe16(&cmd[6], vp_addr);
    OtaWriteBe16(&cmd[8], block_num);
    cmd[10] = 0U;
    cmd[11] = 0U;
    write_dgus_vp(OTA_NAND_CMD_ADDR, cmd, 6);
}

/**
 * @brief 启动NAND CRC32计算命令
 * @param[in] nand_addr NAND起始地址，单位为字节地址
 * @param[in] block_num 参与CRC计算的4KB块数量
 * @return 无
 */
static void OtaStartNandCrc(uint32_t nand_addr, uint16_t block_num)
{
    uint8_t cmd[12];

    cmd[0] = 0x5A;
    cmd[1] = 0x05;
    OtaWriteBe32(&cmd[2], nand_addr);
    OtaWriteBe16(&cmd[6], block_num);
    cmd[8] = 0U;
    cmd[9] = 0U;
    cmd[10] = 0U;
    cmd[11] = 0U;
    write_dgus_vp(OTA_NAND_CMD_ADDR, cmd, 6);
}

/**
 * @brief 读取NAND CRC32计算结果
 * @return NAND控制器计算得到的CRC32值
 */
static uint32_t OtaReadNandCrc32(void)
{
    uint8_t crc_buf[4];

    read_dgus_vp(OTA_NAND_CRC_ADDR, crc_buf, 2);
    return OtaReadLe32(crc_buf);
}

/**
 * @brief 清空DGUS数据缓存区
 * @param[in] vp_addr 待清空的DGUS VP起始地址
 * @return 无
 * @note 清空长度固定为4KB，用于避免末包残留旧数据
 */
static void OtaClearDgusBuffer(uint16_t vp_addr)
{
    uint8_t zero_buf[8];
    uint16_t i;

    memset(zero_buf, 0, sizeof(zero_buf));
    for(i = 0U; i < (OTA_PACKET_BYTES / sizeof(zero_buf)); i++)
    {
        write_dgus_vp(vp_addr + (i * (sizeof(zero_buf) / 2U)), zero_buf, sizeof(zero_buf) / 2U);
    }
}

/**
 * @brief 清空NAND升级头文件区域
 * @details OTA开始时先写入空4KB头文件，下载完成后再写入有效头文件。
 * @param 无
 * @return 无
 */
static void OtaClearNandHeader(void)
{
    OtaWaitNandIdle();
    OtaClearDgusBuffer(otaCACHE_VP_A);
    OtaStartNandWrite(otaNAND_START_ADDR, otaCACHE_VP_A, 1U);
}

/**
 * @brief 校验05数据包CRC16
 * @param[in] frame OTA数据包帧指针
 * @param[in] packet_len 数据区长度，单位为字节
 * @return 1=校验通过，0=校验失败
 */
static uint8_t OtaPacketCrc16Ok(uint8_t *frame, uint16_t packet_len)
{
    uint16_t crc_calc;
    uint16_t crc_recv;

    crc_calc = crc_16(&frame[26], packet_len);
    crc_recv = OtaReadLe16(&frame[26U + packet_len]);

    return (crc_calc == crc_recv) ? 1U : 0U;
}

/**
 * @brief 处理04文件信息命令
 * @details 解析文件数量、文件类型、文件大小、文件CRC和NAND起始块，并回复05请求数据。
 * @param[in] frame OTA文件信息帧指针
 * @param[in] len OTA文件信息帧长度，单位为字节
 * @return 无
 */
static void OtaHandleFileInfo(uint8_t *frame, uint16_t len)
{
    uint8_t name_len;
    uint16_t status_index;
    uint16_t crc_index;
    uint16_t blocks;
    OtaFileInfo *file;

    if(len < 25U)
    {
        return;
    }

    name_len = frame[19];
    status_index = 20U + (uint16_t)name_len;
    crc_index = status_index + 1U;

    if((status_index >= len) || ((crc_index + 4U) > len))
    {
        return;
    }

    if(frame[status_index] != 0U)
    {
        return;
    }

    if(frame[6] >= otaDOWNLOAD_MAX)
    {
        return;
    }

    if(frame[6] == 0U)
    {
        OtaInit();
        OtaClearNandHeader();
        #if v851PROTOCOL_ENABLED
        V851ProtocolNotifyOtaState(V851_OTA_STAGE_INSTALLING, 0U, NULL);
        #endif
    }

    OtaStatus.total_num = frame[5];
    if(OtaStatus.total_num > otaDOWNLOAD_MAX)
    {
        OtaStatus.total_num = otaDOWNLOAD_MAX;
    }
    OtaStatus.now_num = frame[6];
    OtaStatus.off_position = 0UL;
    OtaStatus.off_len = OTA_PACKET_BYTES;
    OtaStatus.all_size = OtaReadLe32(&frame[7]);

    file = &OtaStatus.file[OtaStatus.now_num];
    file->itype = frame[11];
    file->apply = frame[12];
    file->unid = OtaReadLe16(&frame[13]);
    file->size = OtaReadLe32(&frame[15]);
    file->crc32 = OtaReadLe32(&frame[crc_index]);
    file->flash_start = OtaStatus.flash_start_num;

    blocks = OtaCeilDiv32(file->size, OTA_PACKET_BYTES);
    OtaStatus.flash_start_num += blocks;
    OtaStatus.download_end_flag = 0U;

    OtaSendData05();
    OtaSetTimeout(OTA_STEP_WAIT_DATA);
}

/**
 * @brief 将05数据包写入NAND
 * @details 先将数据写入DGUS缓存VP，再通过NAND命令写入目标4KB块。
 * @param[in] frame OTA数据帧指针
 * @param[in] packet_len 数据区长度，单位为字节
 * @return 无
 */
static void OtaWritePacketToNand(uint8_t *frame, uint16_t packet_len)
{
    uint16_t vp_addr;
    uint16_t write_words;
    uint8_t last_word[2];
    uint32_t now_packet;
    uint32_t nand_addr;
    uint32_t progress;

    now_packet = (uint32_t)OtaStatus.file[OtaStatus.now_num].flash_start +
                 (OtaStatus.off_position / OTA_PACKET_BYTES);

    if((now_packet & 0x01UL) != 0UL)
    {
        vp_addr = otaCACHE_VP_A;
    }else
    {
        vp_addr = otaCACHE_VP_B;
    }

    if(packet_len < OTA_PACKET_BYTES)
    {
        OtaClearDgusBuffer(vp_addr);
    }

    write_words = packet_len / 2U;
    if(write_words != 0U)
    {
        write_dgus_vp(vp_addr, &frame[26], write_words);
    }
    if((packet_len & 0x01U) != 0U)
    {
        last_word[0] = frame[26U + packet_len - 1U];
        last_word[1] = 0U;
        write_dgus_vp(vp_addr + write_words, last_word, 1);
    }

    if((OtaStatus.all_size != 0UL) && (now_packet >= otaDATA_START_BLOCK))
    {
        progress = (((now_packet - otaDATA_START_BLOCK) * OTA_PACKET_BYTES) * 100UL) /
                   OtaStatus.all_size;
        if(progress > 100UL)
        {
            progress = 100UL;
        }
        OtaSpeedShow((uint8_t)progress);
        #if v851PROTOCOL_ENABLED
        if(((uint8_t)progress >= (uint8_t)(OtaLastReportedProgress + 5U)) ||
           ((uint8_t)progress == 100U))
        {
            OtaLastReportedProgress = (uint8_t)progress;
            V851ProtocolNotifyOtaState(V851_OTA_STAGE_INSTALLING,
                                       (uint8_t)progress, NULL);
        }
        #endif
    }

    OtaWaitNandIdle();
    nand_addr = otaNAND_START_ADDR + (now_packet * OTA_PACKET_BYTES);
    OtaStartNandWrite(nand_addr, vp_addr, 1U);
}

/**
 * @brief 校验当前文件CRC32
 * @return 1=校验通过，0=校验失败
 * @note otaCRC32_CHECK_ENABLED为0时直接返回通过，便于调试
 */
static uint8_t OtaFileCrcOk(void)
{
    uint16_t blocks;
    uint32_t nand_addr;
    uint32_t crc32;
    OtaFileInfo *file;

    #if !otaCRC32_CHECK_ENABLED
    return 1U;
    #else
    file = &OtaStatus.file[OtaStatus.now_num];
    blocks = OtaCeilDiv32(file->size, OTA_PACKET_BYTES);
    nand_addr = otaNAND_START_ADDR + ((uint32_t)file->flash_start * OTA_PACKET_BYTES);

    #if v851PROTOCOL_ENABLED
    V851ProtocolNotifyOtaState(V851_OTA_STAGE_VERIFYING,
                               OtaLastReportedProgress, NULL);
    #endif
    OtaWaitNandIdle();
    OtaStartNandCrc(nand_addr, blocks);
    OtaWaitNandIdle();
    delay_ms(50);
    crc32 = OtaReadNandCrc32();

    return (crc32 == file->crc32) ? 1U : 0U;
    #endif
}

/**
 * @brief 处理05升级数据命令
 * @details 校验分包CRC16、写入NAND、判断文件是否完成，并根据整文件CRC32回复06结果。
 * @param[in] frame OTA数据帧指针
 * @param[in] len OTA数据帧长度，单位为字节
 * @return 无
 */
static void OtaHandlePacketData(uint8_t *frame, uint16_t len)
{
    uint16_t payload_len;
    OtaFileInfo *file;

    if(len < 30U)
    {
        return;
    }

    payload_len = OtaReadBe16(&frame[2]);
    if(payload_len < 24U)
    {
        return;
    }
    payload_len -= 24U;

    if((payload_len > OTA_PACKET_BYTES) || ((26U + payload_len + 2U) > len))
    {
        return;
    }

    if(OtaPacketCrc16Ok(frame, payload_len) == 0U)
    {
        OtaSendData05();
        OtaSetTimeout(OTA_STEP_WAIT_DATA);
        return;
    }

    OtaStep = OTA_STEP_IDLE;
    OtaWritePacketToNand(frame, payload_len);

    file = &OtaStatus.file[OtaStatus.now_num];
    if((OtaStatus.off_position + OtaStatus.off_len) >= file->size)
    {
        if(OtaFileCrcOk() != 0U)
        {
            OtaSendData06(2U);
            OtaSetTimeout(OTA_STEP_WAIT_RESULT_ACK);
            if((OtaStatus.now_num + 1U) >= OtaStatus.total_num)
            {
                OtaStatus.download_end_flag = 0x01U;
                OtaLastReportedProgress = 100U;
                #if v851PROTOCOL_ENABLED
                V851ProtocolNotifyOtaState(V851_OTA_STAGE_INSTALLING,
                                           100U, NULL);
                #endif
            }
        }else
        {
            OtaSendData06(3U);
            OtaSetTimeout(OTA_STEP_WAIT_RESULT_ACK);
            #if v851PROTOCOL_ENABLED
            V851ProtocolNotifyOtaState(V851_OTA_STAGE_FAILED,
                                       OtaLastReportedProgress,
                                       "HARDWARE_FAULT");
            #endif
        }
    }else
    {
        OtaStatus.off_position += OtaStatus.off_len;
        OtaSendData05();
        OtaSetTimeout(OTA_STEP_WAIT_DATA);
    }
}

/**
 * @brief 处理06结果确认命令
 * @details 在最后一个文件成功后，将下载完成状态推进到升级执行状态。
 * @param 无
 * @return 无
 */
static void OtaHandleFileResultAck(void)
{
    OtaStep = OTA_STEP_IDLE;
    if(OtaStatus.download_end_flag == 0x01U)
    {
        OtaStatus.download_end_flag = 0x11U;
    }
}

/**
 * @brief 写入OTA头文件中的单个文件索引项
 * @param[in] index 文件索引项位置
 * @param[in] file 文件信息指针
 * @param[in] block_count 文件占用块数
 * @return 无
 */
static void OtaSetHeaderFileInfo(uint16_t index, OtaFileInfo *file, uint8_t block_count)
{
    uint16_t offset;

    if(index >= OTA_FILE_INFO_COUNT)
    {
        return;
    }

    offset = OTA_FILE_INFO_OFFSET + (index * OTA_FILE_INFO_SIZE);
    OtaHeaderBuffer[offset] = 0x5A;
    OtaHeaderBuffer[offset + 1U] = block_count;
    OtaHeaderBuffer[offset + 2U] = (uint8_t)file->flash_start;
    OtaHeaderBuffer[offset + 3U] = (uint8_t)(file->flash_start >> 8);
}

/**
 * @brief 生成OTA 4KB升级头文件
 * @details 根据已下载文件信息生成Boot识别用头文件，并在末尾写入CRC16。
 * @param 无
 * @return 无
 */
static void OtaBuildHeader(void)
{
    uint8_t i;
    uint16_t crc16;
    uint16_t index;
    uint32_t block_count;
    OtaFileInfo *file;

    memset(OtaHeaderBuffer, 0, sizeof(OtaHeaderBuffer));
    OtaHeaderBuffer[0] = 0xA5;
    OtaHeaderBuffer[1] = 0x5A;
    OtaHeaderBuffer[2] = 0x33;
    OtaHeaderBuffer[3] = 0x43;

    for(i = 0U; i < OtaStatus.total_num; i++)
    {
        file = &OtaStatus.file[i];
        if(file->itype == 1U)
        {
            if(file->unid <= 127U)
            {
                block_count = OtaCeilDiv32(file->size, OTA_PACKET_BYTES);
                if(block_count > 255UL)
                {
                    block_count = 255UL;
                }
                OtaSetHeaderFileInfo(file->unid + 256U, file, (uint8_t)block_count);
            }
        }else if((file->itype >= 2U) && (file->itype <= 8U))
        {
            if(file->unid <= 255U)
            {
                index = file->unid;
                if(file->unid >= 0xC0U)
                {
                    block_count = OtaCeilDiv32(file->size, 8388608UL);
                }else
                {
                    block_count = OtaCeilDiv32(file->size, 262144UL);
                }
                if(block_count > 255UL)
                {
                    block_count = 255UL;
                }
                OtaSetHeaderFileInfo(index, file, (uint8_t)block_count);
            }
        }else
        {
            __NOP();
        }
    }

    crc16 = crc_16(OtaHeaderBuffer, OTA_HEADER_BYTES - 2U);
    OtaHeaderBuffer[OTA_HEADER_BYTES - 2U] = (uint8_t)crc16;
    OtaHeaderBuffer[OTA_HEADER_BYTES - 1U] = (uint8_t)(crc16 >> 8);
}

/**
 * @brief 完成OTA升级并触发Boot流程
 * @details 写入4KB头文件、保存升级完成标志到NOR Flash，最后写入0x04触发Boot加载升级文件。
 * @param 无
 * @return 无
 * @warning 本函数会触发设备重启升级，应只在所有文件下载成功后调用
 */
static void OtaFinishUpgrade(void)
{
    uint8_t boot_cmd[4];
    uint8_t report_try;
    uint16_t complete_flag_word = 1U;

    #if v851PROTOCOL_ENABLED
    V851ProtocolNotifyOtaState(V851_OTA_STAGE_REBOOTING, 100U, NULL);
    for(report_try = 0U; report_try < 4U; ++report_try)
    {
        V851ProtocolTask();
        delay_ms(30);
    }
    #else
    (void)report_try;
    #endif

    OtaBuildHeader();
    write_dgus_vp(otaCACHE_VP_A, OtaHeaderBuffer, OTA_HEADER_WORDS);
    OtaWaitNandIdle();
    OtaStartNandWrite(otaNAND_START_ADDR, otaCACHE_VP_A, 1U);
    OtaWaitNandIdle();

    OtaCompleteFlag = 1U;
    write_dgus_vp(otaUPGRADE_FLAG_ADDR, (uint8_t *)&complete_flag_word, 1);
    DgusToFlash(flashMAIN_BLOCK_ORDER, otaUPGRADE_FLAG_ADDR, otaUPGRADE_FLAG_ADDR, 2);

    OtaStatus.download_end_flag = 0U;

    boot_cmd[0] = 0x55;
    boot_cmd[1] = 0xAA;
    boot_cmd[2] = 0x5A;
    boot_cmd[3] = 0xA5;
    SysEnterCritical();
    write_dgus_vp(OTA_BOOT_CMD_ADDR, boot_cmd, 2);

    SysExitCritical();
}

/**
 * @brief OTA测试触发处理函数
 * @details 当otaTEST_TRIGGER_ADDR为0x5AA5时，向R11发送F3命令触发测试升级流程。
 * @param 无
 * @return 无
 */
static void OtaTestTrigger(void)
{
    uint16_t trigger;
    #if !v851PROTOCOL_ENABLED
    uint8_t cmd[5];
    #endif
    uint16_t zero = 0U;

    read_dgus_vp(otaTEST_TRIGGER_ADDR, (uint8_t *)&trigger, 1);
    if(trigger == 0x5AA5U)
    {
        #if !v851PROTOCOL_ENABLED
        cmd[0] = 0xAA;
        cmd[1] = 0x55;
        cmd[2] = 0x00;
        cmd[3] = 0x01;
        cmd[4] = 0xF3;
        UartSendData(&Uart_R11, cmd, sizeof(cmd));
        #endif
        write_dgus_vp(otaTEST_TRIGGER_ADDR, (uint8_t *)&zero, 1);
    }
}

/**
 * @brief OTA上下文初始化函数
 * @details 清空OTA运行上下文，并初始化NAND数据起始块和超时状态。
 * @param 无
 * @return 无
 */
void OtaInit(void)
{
    memset((uint8_t *)&OtaStatus, 0, sizeof(OtaStatus));
    OtaStatus.flash_start_num = otaDATA_START_BLOCK;
    OtaStep = OTA_STEP_IDLE;
    OtaTimeout = 0U;
    OtaTimeoutReload = 0U;
    OtaLastResult = 2U;
    OtaLastReportedProgress = 0U;
}

/**
 * @brief OTA协议帧接收处理函数
 * @details 根据AB CD帧命令号分发到04、05、06命令处理函数。
 * @param[in] frame OTA协议帧缓冲区指针，不能为NULL
 * @param[in] len OTA协议帧总长度，单位为字节
 * @return 无
 */
void OtaReceive(uint8_t *frame, uint16_t len)
{
    uint16_t body_length;

    if((frame == NULL) || (len < 5U))
    {
        return;
    }

    if((frame[0] != OTA_FRAME_HEAD_0) || (frame[1] != OTA_FRAME_HEAD_1))
    {
        return;
    }
    body_length = OtaReadBe16(&frame[2]);
    if((body_length < 1U) || ((uint16_t)(body_length + 4U) != len) ||
       (len > 4124U))
    {
        return;
    }

    switch(frame[4])
    {
        case OTA_CMD_FILE_INFO:
            OtaHandleFileInfo(frame, len);
            break;
        case OTA_CMD_READ_DATA:
            OtaHandlePacketData(frame, len);
            break;
        case OTA_CMD_FILE_RESULT:
            OtaHandleFileResultAck();
            break;
        default:
            break;
    }
}

/**
 * @brief OTA周期任务函数
 * @details 处理测试触发、等待超时重发和下载完成后的升级执行。
 * @param 无
 * @return 无
 */
void OtaTask(void)
{
    OtaTestTrigger();

    if(OtaStep != OTA_STEP_IDLE)
    {
        if(OtaTimeout == 0U)
        {
            if((OtaStep & 0xF0U) == 0x50U)
            {
                OtaSendData05();
                OtaSetTimeout(OTA_STEP_WAIT_DATA);
            }else if((OtaStep & 0xF0U) == 0x60U)
            {
                OtaSendData06(OtaLastResult);
                OtaSetTimeout(OTA_STEP_WAIT_RESULT_ACK);
            }else
            {
                OtaStep = OTA_STEP_IDLE;
            }
        }
    }

    if(OtaStatus.download_end_flag == 0x11U)
    {
        OtaFinishUpgrade();
    }
}

/**
 * @brief OTA业务默认数据初始化函数
 * @details 写入参考项目中的版本号，升级完成提示。
 * @param 无
 * @return 无
 */
void OtaDataInit(void)
{
    static uint8_t soft_ver[64] = "v2.0.4";
    static uint8_t hard_ver[64] = "v3.2.4";
    uint16_t complete_flag_word = 0U;
    uint8_t complete_text[2] = {0x20, 0x31};

    FlashToDgus(flashMAIN_BLOCK_ORDER, otaUPGRADE_FLAG_ADDR, otaUPGRADE_FLAG_ADDR, 2);
    read_dgus_vp(otaUPGRADE_FLAG_ADDR, (uint8_t *)&complete_flag_word, 1);
    OtaCompleteFlag = (uint8_t)complete_flag_word;
    if(complete_flag_word == 1U)
    {
        write_dgus_vp(otaUPGRADE_FLAG_ADDR, complete_text, 1);
    }
    write_dgus_vp(otaUPDATE_INFO_ADDR, soft_ver, 32);
    write_dgus_vp(otaUPDATE_INFO_ADDR + 0x20U, hard_ver, 32);
}

/**
 * @brief OTA定时计数函数
 * @details 由1ms定时中断调用，内部累加到10ms后递减OTA超时计数。
 * @param 无
 * @return 无
 */
void OtaTimerTick1ms(void)
{
    static uint8_t tick_count = 0U;

    tick_count++;
    if(tick_count >= 10U)
    {
        tick_count = 0U;
        if(OtaTimeout != 0U)
        {
            OtaTimeout--;
        }
    }
}

/**
 * @brief OTA进度显示函数
 * @details 控制DGUS进度控件显示状态，并写入当前百分比进度值。
 * @param[in] speed_num 当前下载进度百分比，0表示关闭显示
 * @return 无
 */
void OtaSpeedShow(uint8_t speed_num)
{
    uint16_t value;
    uint8_t disable_sp[2] = {0xFF, 0x00};
    static uint8_t speed_enabled = 0xFFU;

    if(speed_num == 0U)
    {
        if(speed_enabled != 0U)
        {
            speed_enabled = 0U;
            write_dgus_vp(otaSPEED_SP_ADDR, disable_sp, 1);
        }
    }else
    {
        speed_enabled = 1U;
        value = otaSPEED_VP_ADDR;
        write_dgus_vp(otaSPEED_SP_ADDR, (uint8_t *)&value, 1);
        value = speed_num;
        write_dgus_vp(otaSPEED_VP_ADDR, (uint8_t *)&value, 1);
    }
}

void OtaAcknowledgeComplete(void)
{
    uint16_t complete_flag_word = 0U;

    OtaCompleteFlag = 0U;
    write_dgus_vp(otaUPGRADE_FLAG_ADDR,
                  (uint8_t *)&complete_flag_word, 1);
    DgusToFlash(flashMAIN_BLOCK_ORDER,
                otaUPGRADE_FLAG_ADDR,
                otaUPGRADE_FLAG_ADDR,
                2);
}

#endif /* otaOTA_ENABLED */
