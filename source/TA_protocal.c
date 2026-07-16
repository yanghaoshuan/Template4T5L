#include "TA_protocal.h"
#include "rtc.h"
#include "sys.h"
#include <string.h>

/**
 * @file    TA_protocal.c
 * @brief   C51 TA兼容串口协议处理
 * @details 本文件移植自C51工程的Uart2WaterReadFrame协议，
 *          帧格式为AA + 2字节长度 + 命令 + 数据 + CC 33 C3 3C。
 */

#if uartTA_PROTOCOL_ENABLED

/* C51 TA协议固定帧头帧尾 */
#define TA_FRAME_HEAD              0xAAU
#define TA_FRAME_TAIL0             0xCCU
#define TA_FRAME_TAIL1             0x33U
#define TA_FRAME_TAIL2             0xC3U
#define TA_FRAME_TAIL3             0x3CU

#define TA_SEND_BUF_SIZE           256U
#define TA_STRING_CLEAR_WORDS      0x80U
#define TA_STRING_CLEAR_BYTES      0x100U
#define TA_AUTO_UPLOAD_ADDR        sysDGUS_AUTO_UPLOAD_VP_ADDR
#define TA_DATA_WRITE_SETTLE_MS    5U
#define TA_TOGGLE_KEY_START_ADDR   0x1037U
#define TA_TOGGLE_KEY_END_ADDR     0x103DU
#define TA_TOGGLE_KEY_COUNT        (TA_TOGGLE_KEY_END_ADDR - TA_TOGGLE_KEY_START_ADDR + 1U)

/* 0x1037-0x103D按键的独立上传状态，按键每上传一次0值就翻转一次。 */
static uint8_t ta_toggle_key_state[TA_TOGGLE_KEY_COUNT] = {0U};

/**
 * @brief 获取指定UART的发送缓冲区大小
 * @param uart UART通信接口指针
 * @return 发送缓冲区容量，未知UART返回0
 * @note 用于限制TA应答帧长度，避免写满发送环形缓冲区。
 */
static uint16_t TAGetUartTxLimit(UART_TYPE *uart)
{
    #if uartUART2_ENABLED
    if(uart == &Uart2)
    {
        return uartUART2_TXBUF_SIZE;
    }
    #endif /* uartUART2_ENABLED */

    #if uartUART3_ENABLED
    if(uart == &Uart3)
    {
        return uartUART3_TXBUF_SIZE;
    }
    #endif /* uartUART3_ENABLED */

    #if uartUART4_ENABLED
    if(uart == &Uart4)
    {
        return uartUART4_TXBUF_SIZE;
    }
    #endif /* uartUART4_ENABLED */

    #if uartUART5_ENABLED
    if(uart == &Uart5)
    {
        return uartUART5_TXBUF_SIZE;
    }
    #endif /* uartUART5_ENABLED */

    return 0U;
}


/**
 * @brief 带长度保护的TA串口发送
 * @param uart UART通信接口指针
 * @param buf 待发送缓冲区
 * @param len 待发送字节数
 */
static void TASendData(UART_TYPE *uart, uint8_t *buf, uint16_t len)
{
    uint16_t tx_limit;

    if((uart == NULL) || (buf == NULL) || (len == 0U))
    {
        return;
    }

    tx_limit = TAGetUartTxLimit(uart);
    if((tx_limit == 0U) || (len >= tx_limit))
    {
        return;
    }

    UartSendData(uart, buf, len);
}


/**
 * @brief 将TA派生命令转发到UART5
 * @param buf 待转发数据
 * @param len 数据长度
 * @note C51协议中的联网、升级相关命令通过UART5转发给外部模块。
 */
static void TAForwardToUart5(uint8_t *buf, uint16_t len)
{
    #if uartUART5_ENABLED
    TASendData(&Uart5, buf, len);
    #else
    (void)buf;
    (void)len;
    #endif /* uartUART5_ENABLED */
}


/**
 * @brief 写入C51 TA协议帧尾
 * @param buf 目标帧缓冲区
 * @param tail_index 帧尾起始下标
 */
static void TASetTail(uint8_t *buf, uint16_t tail_index)
{
    buf[tail_index] = TA_FRAME_TAIL0;
    buf[tail_index + 1U] = TA_FRAME_TAIL1;
    buf[tail_index + 2U] = TA_FRAME_TAIL2;
    buf[tail_index + 3U] = TA_FRAME_TAIL3;
}


/**
 * @brief 将指定VP的0值按键上传转换为交替的0/1状态
 * @param vp_addr 本次自动上传的起始VP地址
 * @param data 本次上传的数据区
 * @param data_bytes 数据区字节数
 * @note 仅处理0x1037-0x103D且数据为0x0000的单字按键，不修改DGUS中的原值。
 */
static void TAApplyToggleKeyUpload(uint16_t vp_addr, uint8_t *data, uint16_t data_bytes)
{
    uint16_t key_index;

    if((data == NULL) || (data_bytes < 2U) ||
       (vp_addr < TA_TOGGLE_KEY_START_ADDR) || (vp_addr > TA_TOGGLE_KEY_END_ADDR))
    {
        return;
    }

    if((data[0] != 0U) || (data[1] != 0U))
    {
        return;
    }

    key_index = vp_addr - TA_TOGGLE_KEY_START_ADDR;
    ta_toggle_key_state[key_index] = (ta_toggle_key_state[key_index] == 0U) ? 1U : 0U;
    data[0] = 0U;
    data[1] = ta_toggle_key_state[key_index];
    write_dgus_vp(vp_addr, data, 1U);
}


/**
 * @brief 解析C51 TA协议长度字段
 * @param frame 协议帧
 * @param data_len 输出C51逻辑数据长度
 * @return 1=解析成功，0=解析失败
 * @note 普通命令总帧长为len+3；0x42在原C51代码中按len+5处理，
 *       因此这里将其逻辑长度补偿为len+2，以统一后续校验。
 */
static uint8_t TAGetLegacyDataLen(uint8_t *frame, uint16_t *data_len)
{
    uint16_t raw_len;

    if((frame == NULL) || (data_len == NULL))
    {
        return 0U;
    }

    raw_len = ((uint16_t)frame[1] << 8) | frame[2];
    if(frame[3] == 0x42U)
    {
        if(raw_len > 0xFFFDU)
        {
            return 0U;
        }
        *data_len = raw_len + 2U;
    }
    else
    {
        *data_len = raw_len;
    }

    return 1U;
}


/**
 * @brief 校验TA协议完整帧
 * @param frame 协议帧
 * @param len 完整帧长度
 * @param data_len 输出C51逻辑数据长度
 * @return 1=帧合法，0=帧非法
 */
static uint8_t TAFrameIsValid(uint8_t *frame, uint16_t len, uint16_t *data_len)
{
    if((frame == NULL) || (len < 7U) || (frame[0] != TA_FRAME_HEAD))
    {
        return 0U;
    }

    if(TAGetLegacyDataLen(frame, data_len) == 0U)
    {
        return 0U;
    }

    if((*data_len < 4U) || (len != (*data_len + 3U)))
    {
        return 0U;
    }

    if((frame[len - 4U] != TA_FRAME_TAIL0) ||
       (frame[len - 3U] != TA_FRAME_TAIL1) ||
       (frame[len - 2U] != TA_FRAME_TAIL2) ||
       (frame[len - 1U] != TA_FRAME_TAIL3))
    {
        return 0U;
    }

    return 1U;
}


/**
 * @brief 将C51数值区类型和字节偏移映射到DGUS VP地址
 * @param addr_type C51地址类型
 * @param offset C51协议字节偏移，换算VP前先除以2
 * @param addr 输出DGUS VP地址
 * @return 1=映射成功，0=未知类型
 */
static uint8_t TAGetNumberAddr(uint8_t addr_type, uint16_t offset, uint32_t *addr)
{
    if(addr == NULL)
    {
        return 0U;
    }

    offset >>= 1;
    if(addr_type == 0x08U)
    {
        *addr = 0x1000UL + offset;
        return 1U;
    }

    if(addr_type == 0x02U)
    {
        *addr = 0xB000UL + offset;
        return 1U;
    }

    return 0U;
}


/**
 * @brief 处理0x82数值区写入命令
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleWriteNumber(uint8_t *frame, uint16_t data_len)
{
    uint16_t offset;
    uint16_t write_words;
    uint32_t data_addr;

    if(data_len < 10U)
    {
        return;
    }

    offset = ((uint16_t)frame[6] << 8) | frame[7];
    if(TAGetNumberAddr(frame[5], offset, &data_addr) == 0U)
    {
        return;
    }

    write_words = (data_len - 10U) >> 1;
    if(write_words == 0U)
    {
        return;
    }

    write_dgus_vp(data_addr, &frame[9], write_words);
    // delay_ms(TA_DATA_WRITE_SETTLE_MS);
}


/**
 * @brief 处理0x42字符串区写入命令
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 * @note 仅写入协议负载中已有的完整VP字，不再额外补0xFF。
 */
static void TAHandleWriteString(uint8_t *frame, uint16_t data_len)
{
    uint16_t offset;
    uint16_t payload_len;
    uint16_t write_words;
    uint32_t data_addr;

    if((data_len < 9U) || (frame[5] != 0x00U))
    {
        return;
    }

    offset = ((uint16_t)frame[6] << 8) | frame[7];
    offset >>= 1;
    data_addr = 0x5000UL + offset;
    payload_len = data_len - 9U;
    write_words = payload_len >> 1;
    if(write_words > 0U)
    {
        write_dgus_vp(data_addr, &frame[8], write_words);
        // delay_ms(TA_DATA_WRITE_SETTLE_MS);
    }
}


/**
 * @brief 处理0x83数值区读取命令
 * @param uart 应答UART
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleReadNumber(UART_TYPE *uart, uint8_t *frame, uint16_t data_len)
{
    uint8_t send_buf[TA_SEND_BUF_SIZE];
    uint16_t offset;
    uint16_t read_bytes;
    uint16_t read_words;
    uint16_t response_len;
    uint16_t total_len;
    uint16_t i;
    uint32_t data_addr;

    if(data_len < 9U)
    {
        return;
    }

    offset = ((uint16_t)frame[6] << 8) | frame[7];
    if(TAGetNumberAddr(frame[5], offset, &data_addr) == 0U)
    {
        return;
    }

    read_bytes = frame[8];
    read_words = read_bytes >> 1;
    response_len = 9U + read_bytes;
    total_len = response_len + 4U;
    if(total_len > TA_SEND_BUF_SIZE)
    {
        return;
    }

    memset(send_buf, 0, total_len);
    send_buf[0] = TA_FRAME_HEAD;
    send_buf[1] = (uint8_t)(response_len >> 8);
    send_buf[2] = (uint8_t)response_len;
    send_buf[3] = 0x83U;
    for(i = 0U; i < 5U; i++)
    {
        send_buf[4U + i] = frame[4U + i];
    }

    if(read_words > 0U)
    {
        read_dgus_vp(data_addr, &send_buf[9], (uint8_t)read_words);
    }
    TASetTail(send_buf, response_len);
    TASendData(uart, send_buf, total_len);
}


/**
 * @brief 处理0x43中的联网信息查询派生命令
 * @param query_addr 查询地址
 * @note FF00/FE80/FE00/FD80映射为UART5的66 04/05/06/07请求。
 */
static void TAForwardInfoQuery(uint16_t query_addr)
{
    uint8_t send_66[5] = {0xAAU, 0xBBU, 0x02U, 0x66U, 0x00U};

    if(query_addr == 0xFF00U)
    {
        send_66[4] = 0x04U;
    }
    else if(query_addr == 0xFE80U)
    {
        send_66[4] = 0x05U;
    }
    else if(query_addr == 0xFE00U)
    {
        send_66[4] = 0x06U;
    }
    else if(query_addr == 0xFD80U)
    {
        send_66[4] = 0x07U;
    }
    else
    {
        return;
    }

    TAForwardToUart5(send_66, sizeof(send_66));
}


/**
 * @brief 处理0x43字符串区读取命令
 * @param uart 应答UART
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleReadString(UART_TYPE *uart, uint8_t *frame, uint16_t data_len)
{
    uint8_t send_buf[TA_SEND_BUF_SIZE];
    uint16_t offset;
    uint16_t read_bytes;
    uint16_t read_words;
    uint16_t response_len;
    uint16_t total_len;
    uint16_t i;
    uint32_t data_addr;

    if(data_len < 9U)
    {
        return;
    }

    offset = ((uint16_t)frame[6] << 8) | frame[7];
    if(frame[5] == 0x01U)
    {
        TAForwardInfoQuery(offset);
        return;
    }

    if(frame[5] != 0x00U)
    {
        return;
    }

    offset >>= 1;
    data_addr = 0x5000UL + offset;
    read_bytes = frame[8];
    read_words = read_bytes >> 1;
    response_len = 9U + read_bytes;
    total_len = response_len + 4U;
    if(total_len > TA_SEND_BUF_SIZE)
    {
        return;
    }

    memset(send_buf, 0, total_len);
    send_buf[0] = TA_FRAME_HEAD;
    send_buf[1] = (uint8_t)(response_len >> 8);
    send_buf[2] = (uint8_t)response_len;
    send_buf[3] = 0x43U;
    for(i = 0U; i < 5U; i++)
    {
        send_buf[4U + i] = frame[4U + i];
    }

    if(read_words > 0U)
    {
        read_dgus_vp(data_addr, &send_buf[9], (uint8_t)read_words);
    }
    TASetTail(send_buf, response_len);
    TASendData(uart, send_buf, total_len);
}


/**
 * @brief 处理0x32当前页面读取命令
 * @param uart 应答UART
 */
static void TAHandleReadPage(UART_TYPE *uart)
{
    uint8_t send_buf[10];
    uint16_t page_id;

    page_id = ReadPageId();
    send_buf[0] = TA_FRAME_HEAD;
    send_buf[1] = 0x00U;
    send_buf[2] = 0x07U;
    send_buf[3] = 0x32U;
    send_buf[4] = (uint8_t)(page_id >> 8);
    send_buf[5] = (uint8_t)page_id;
    TASetTail(send_buf, 6U);
    TASendData(uart, send_buf, sizeof(send_buf));
}


/**
 * @brief 处理0x9B RTC读取命令
 * @param uart 应答UART
 */
static void TAHandleRtcRead(UART_TYPE *uart)
{
    uint8_t rtc_arr[8];
    uint8_t send_buf[14];

    memset(rtc_arr, 0, sizeof(rtc_arr));
    read_dgus_vp(0x0010UL, rtc_arr, 4);

    send_buf[0] = TA_FRAME_HEAD;
    send_buf[1] = 0x00U;
    send_buf[2] = 0x0BU;
    send_buf[3] = 0x9BU;
    send_buf[4] = rtc_arr[0];
    send_buf[5] = rtc_arr[1];
    send_buf[6] = rtc_arr[2];
    send_buf[7] = rtc_arr[4];
    send_buf[8] = rtc_arr[5];
    send_buf[9] = rtc_arr[6];
    TASetTail(send_buf, 10U);
    TASendData(uart, send_buf, sizeof(send_buf));
}


/**
 * @brief 处理0x9C RTC设置命令
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleRtcWrite(uint8_t *frame, uint16_t data_len)
{
    uint8_t rtc_arr[7];

    if(data_len < 10U)
    {
        return;
    }

    rtc_arr[0] = frame[4];
    rtc_arr[1] = frame[5];
    rtc_arr[2] = frame[6];
    rtc_arr[3] = 0U;
    rtc_arr[4] = frame[7];
    rtc_arr[5] = frame[8];
    rtc_arr[6] = frame[9];
    RtcSetTime(rtc_arr);
}


/**
 * @brief 处理0xE3升级启动命令
 * @note 发送67 01到UART5后写入复位触发字。
 */
static void TAHandleUpgradeStart(void)
{
    uint8_t upd_start_arr[5] = {0xAAU, 0xBBU, 0x02U, 0x67U, 0x01U};
    uint8_t rst_arr[4] = {0x55U, 0xAAU, 0x5AU, 0xA5U};

    TAForwardToUart5(upd_start_arr, sizeof(upd_start_arr));
    write_dgus_vp(0x00FCUL, rst_arr, 2);
}


/**
 * @brief 处理0x93版本信息请求命令
 * @note 按C51逻辑向UART5发送66 09。
 */
static void TAHandleVersionRequest(void)
{
    uint8_t send_66[5] = {0xAAU, 0xBBU, 0x02U, 0x66U, 0x09U};

    TAForwardToUart5(send_66, sizeof(send_66));
}


/**
 * @brief 处理0xEA IP信息设置转发命令
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleForwardIpInfo(uint8_t *frame, uint16_t data_len)
{
    uint8_t send_buf[TA_SEND_BUF_SIZE];
    uint16_t i;

    if((data_len < 5U) || (data_len > TA_SEND_BUF_SIZE) || ((data_len - 3U) > 0xFFU))
    {
        return;
    }

    memset(send_buf, 0, data_len);
    send_buf[0] = 0xAAU;
    send_buf[1] = 0xBBU;
    send_buf[2] = (uint8_t)(data_len - 3U);
    send_buf[3] = 0x66U;
    send_buf[4] = 0x02U;
    for(i = 4U; i < (data_len - 1U); i++)
    {
        send_buf[i + 1U] = frame[i];
    }
    TAForwardToUart5(send_buf, data_len);
}


/**
 * @brief 处理0xE9 IP模式设置转发命令
 * @param frame 完整TA帧
 * @param data_len C51逻辑数据长度
 */
static void TAHandleForwardIpMode(uint8_t *frame, uint16_t data_len)
{
    uint8_t send_buf[6];

    if(data_len < 9U)
    {
        return;
    }

    send_buf[0] = 0xAAU;
    send_buf[1] = 0xBBU;
    send_buf[2] = 0x03U;
    send_buf[3] = 0x66U;
    send_buf[4] = 0x03U;
    send_buf[5] = frame[8];
    TAForwardToUart5(send_buf, sizeof(send_buf));
}


/**
 * @brief 处理C51 TA兼容协议帧
 * @param uart 接收该帧的UART接口
 * @param frame 完整协议帧
 * @param len 完整帧长度
 */
void UartStandardTAProtocol(UART_TYPE *uart, uint8_t *frame, uint16_t len)
{
    uint16_t data_len;
    uint16_t page_id;

    if(TAFrameIsValid(frame, len, &data_len) == 0U)
    {
        return;
    }

    switch(frame[3])
    {
        case 0x82U:
            TAHandleWriteNumber(frame, data_len);
            break;

        case 0x42U:
            TAHandleWriteString(frame, data_len);
            break;

        case 0x31U:
            break;

        case 0x32U:
            TAHandleReadPage(uart);
            break;

        case 0xE3U:
            TAHandleUpgradeStart();
            break;

        case 0x93U:
            TAHandleVersionRequest();
            break;

        case 0xEAU:
            TAHandleForwardIpInfo(frame, data_len);
            break;

        case 0xE9U:
            TAHandleForwardIpMode(frame, data_len);
            break;

        case 0x9BU:
            TAHandleRtcRead(uart);
            break;

        case 0x9CU:
            TAHandleRtcWrite(frame, data_len);
            break;

        case 0x70U:
            if(data_len >= 7U)
            {
                page_id = ((uint16_t)frame[4] << 8) | frame[5];
                SwitchPageById(page_id);
            }
            break;

        case 0x83U:
            TAHandleReadNumber(uart, frame, data_len);
            break;

        case 0x43U:
            TAHandleReadString(uart, frame, data_len);
            break;

        default:
            break;
    }
}


/**
 * @brief C51 TA兼容自动上传任务
 * @param uart 上传目标UART接口
 * @note 读取0x0F00触发区，根据VP地址范围组装0x77上传帧。
 */
void TAProtocolUpload(UART_TYPE *uart)
{
    uint8_t auto_load_arr[4];
    uint8_t zero_arr[4] = {0};
    uint8_t send_auto[TA_SEND_BUF_SIZE];
    uint16_t auto_vp;
    uint16_t nlen;
    uint16_t data_bytes;
    uint16_t total_len;
    uint16_t protocol_offset;
    uint16_t i;

    memset(auto_load_arr, 0, sizeof(auto_load_arr));
    read_dgus_vp(TA_AUTO_UPLOAD_ADDR, auto_load_arr, 2);
    if(auto_load_arr[0] != 0x5AU)
    {
        return;
    }

    write_dgus_vp(TA_AUTO_UPLOAD_ADDR, zero_arr, 2);

    auto_vp = ((uint16_t)auto_load_arr[1] << 8) | auto_load_arr[2];
    nlen = auto_load_arr[3];
    if(nlen == 0U)
    {
        return;
    }

    memset(send_auto, 0, sizeof(send_auto));
    send_auto[0] = TA_FRAME_HEAD;
    send_auto[3] = 0x77U;

    if((auto_vp >= 0x5000U) && (auto_vp < 0xB000U))
    {
        /* 字符串区上传到遇到0x00或0xFF为止，与C51 AutoUrat2保持一致。 */
        data_bytes = nlen << 1;
        if((data_bytes + 14U) > TA_SEND_BUF_SIZE)
        {
            return;
        }

        read_dgus_vp(auto_vp, &send_auto[8], (uint8_t)nlen);
        for(i = 0U; i < data_bytes; i++)
        {
            if((send_auto[8U + i] == 0xFFU) || (send_auto[8U + i] == 0x00U))
            {
                send_auto[1] = (uint8_t)((i + 11U) >> 8);
                send_auto[2] = (uint8_t)(i + 11U);
                send_auto[5] = 0x00U;
                protocol_offset = (uint16_t)((auto_vp - 0x5000U) << 1);
                send_auto[6] = (uint8_t)(protocol_offset >> 8);
                send_auto[7] = (uint8_t)protocol_offset;
                send_auto[8U + i] = 0x00U;
                send_auto[9U + i] = 0x00U;
                TASetTail(send_auto, 10U + i);
                TASendData(uart, send_auto, 14U + i);
                break;
            }
        }
    }
    else if((auto_vp >= 0x1000U) && (auto_vp < 0x5000U))
    {
        data_bytes = nlen << 1;
        total_len = 12U + data_bytes;
        if(total_len > TA_SEND_BUF_SIZE)
        {
            return;
        }

        send_auto[1] = (uint8_t)((data_bytes + 9U) >> 8);
        send_auto[2] = (uint8_t)(data_bytes + 9U);
        send_auto[5] = 0x08U;
        protocol_offset = (uint16_t)((auto_vp - 0x1000U) << 1);
        send_auto[6] = (uint8_t)(protocol_offset >> 8);
        send_auto[7] = (uint8_t)protocol_offset;
        read_dgus_vp(auto_vp, &send_auto[8], (uint8_t)((nlen + 1U) >> 1));
        TAApplyToggleKeyUpload(auto_vp, &send_auto[8], data_bytes);
        TASetTail(send_auto, 8U + data_bytes);
        TASendData(uart, send_auto, total_len);
    }
    else if(auto_vp >= 0xB000U)
    {
        data_bytes = nlen << 1;
        total_len = 12U + data_bytes;
        if(total_len > TA_SEND_BUF_SIZE)
        {
            return;
        }

        send_auto[1] = (uint8_t)((data_bytes + 9U) >> 8);
        send_auto[2] = (uint8_t)(data_bytes + 9U);
        send_auto[5] = 0x02U;
        protocol_offset = (uint16_t)((auto_vp - 0xB000U) << 1);
        send_auto[6] = (uint8_t)(protocol_offset >> 8);
        send_auto[7] = (uint8_t)protocol_offset;
        read_dgus_vp(auto_vp, &send_auto[8], (uint8_t)((nlen + 1U) >> 1));
        TASetTail(send_auto, 8U + data_bytes);
        TASendData(uart, send_auto, total_len);
    }
}

#endif /* uartTA_PROTOCOL_ENABLED */
