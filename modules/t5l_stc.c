/*
智能舱鸟舱产品说明书

1.产品功能参数介绍

排风：
    A.app 小程序 控制屏可控制风扇1-6风速调节
      可控制开关时间（定时0.5-8小时后自动开启通风时长10-15分钟）


进风：
    A.app 小程序 控制屏可控制风扇1-4档风速可调节（预留后期可以用负离子替代）
    C.自动开启特殊情况
        1.	开启加热，温度传感器反馈有温差，自动开启加热强风。
        2.	负离子，等离子开启自动开启小风（一档）
        3.	开启加热时，负离子，等离子以加热的进风，优先考虑
        4.	开启加热自动开启，加热时进风不可控制关闭

uvb：（单独控制或单独连接端子）
    A.app 小程序 控制屏可控制UVB 1-4档亮度调节、(1档：0-18/2档：18-25/3档：25-64/4档：64-87.5按照弗格森四区间控制）时间控制（2小时/4小时/6小时/8小时）
    B. UVB波段（253.7nm）有助于补钙，仿晒太阳


雾化：雾化片+雾化液检测
    A.	app 小程序 控制屏可控制开关雾化片，能够根据舱内湿度传感器需要加湿量调节，水位传感器检测雾化液，加液提醒。
    B.	单次运行0.5/1/2小时，间隔时间为2/4/8/12小时
                     C.直径16mm/频率100-115KHz/工作电压3-12v/谐振阻抗≤180/静态电容量3000±15%/机电藕合系数≥60%/厚度0.05mm/孔目数1-5000/孔径3-20Um/堵孔率≤2%

照明：（单独控制或单独连接端子）
    A.app 小程序 控制屏可控制灯光1-4档亮度调节（1档：弱光/2档正常光/3-4档强光）


加热模块:
    A.app 小程序 控制屏可控制温度20-35度(开启时快速升温达到指定温度，
     传感器反馈温度达到了停止加热，温度没达到差-5°正常加热
    （进风风扇和加热模块同步开起，关闭时进风风扇比加热模块晚5秒关闭）
    B.采用220V的PTC加热元件，提供安全的加热能力，PTC根据环境温度调节加热功率；1路400W PTC加热器是否能兼容，220V/110V通用。

紫外线消杀（uvc）：（单独控制或单独连接端子）
    A.	app 小程序 控制屏可控制开关时间（15分钟/30分钟/1小时）
    B.	紫外线UVC（253.7nm）杀菌功能，对空气进行深度消毒，
    C.	消毒需要显示屏确认舱内是否有宠物,并触发物理开关开始工作


红外线：（单独控制或单独连接端子）
    A.光线较暗自动开启/光线强自动关闭（开启后 摄像头LRcat模式自动开启）
    B.红外线波段（940nm）夜间照明摄像头观察清晰

负离子：
    A.app 小程序 控制屏可控制开关(开启时进风自动开启）
    B.	尺寸42*24*21mm/工作环境-20到80度/功率12v/0.3ma/输出高压-4500v±500v/负离子浓度1000万pcs/cm?

等离子：
    A.app 小程序 控制屏可控制开关(开启时进风自动开启）
    C.	尺寸42*24*21mm/功率0.5w/输出高压±5000v±500v/正离子浓度1000万pcs/cm?/负离子浓度1000万pcs/cm?/工作环境-20到75度/湿度＜75%湿度越高噪音越大/等离子发射头针尖距10-50mm




传感器：（湿度+温度传感器）可控制pc板
摄像头：
    A.app 小程序可控制开关
    B. 搭载160°超广角1080P高清
    C.
        1.可通过app，小程序共享摄像头，
    2.视频可分段上传云端，可分享媒体如 微信 抖音 小红书等
    3.光线暗摄像头LRcat模式红外线自动开启
    4.语音相互接收内置麦克风和扬声器支持语音采集与播放，
      可双向通话或语音控制互动，可消除回音背景等，MIC和SPEAKER集成在摄像头模块

电磁锁：
    A.只可控制屏输入电磁锁开启（后台可一键同步开启多台电磁门）
    B. 12V/0.5A/80克力/10秒

智能连接管理：1. 支持双频Wi-Fi和蓝牙，确保设备可轻松接入网络
2.后台数据可快速了解设备运行状况，客户下达指令后台可查询是否同步
3.后台可帮客户解绑二维码
4.二维码只可绑定一人，功能控制权管理，由绑定人分享它人

    *服务器与本地控制立即同步 UI；主控应答只更新运行、联动和队列状态。
*/
#include "sys.h"
#include "t5l_stc.h"
#include "uart.h"
#include "string.h"
DeviceCtrl G_Device_Ctrl;

#define WAIT_ACK_TIMEOUT 1000 //等待应答超时时间
#define WAIT_ACK_ERROR_CNT 2  //等待应答错误次数
/**
 * @brief  modbus主机状态
 */
typedef enum
{
    STATE_IDLE = 0X00,      // 主机空闲状态
    STATE_WAIT_ACK,         // 等待接收
    STATE_WAIT_ACK_TIMEOUT, // 等待接收超时
    STATE_WAIT_ACK_FAILURE, // 等待接收错误
    STATE_WAIT_ACK_SUCCESS, // 等待接收成功
    STATE_EXEC,             // 回调处理

} Uart_State;

typedef struct _data_node
{
    uint8_t buf[20];
    uint8_t len;
} DATA_NODE;

typedef struct _queue_node
{
    DATA_NODE Data[16];
    uint8_t head;
    uint8_t tail;
    uint8_t last_head;
    uint8_t err_cnt;
    Uart_State sta;
    uint16_t wait_ack_time; //等待应答时间
    uint16_t showsuccse_time_ms;
    uint8_t showsuccse_sta;
} QUEUE_NODE;

QUEUE_NODE G_Queue; //环形队列
void Dev_Init(void)
{
    memset(&G_Device_Ctrl, 0, sizeof(G_Device_Ctrl));
}

/* Keep G_Device_Ctrl and the mapped DGUS words in sync. */
static uint8_t T5lStcSyncMappedControlInternal(
                                T5lStcMappedControl control,
                                uint8_t field_mask,
                                uint16_t enabled,
                                uint16_t secondary,
                                uint16_t tertiary,
                                uint8_t sync_command_vp)
{
    uint16_t report[3];
    uint16_t command[3];
    uint32_t report_vp;
    uint32_t command_vp;
    uint8_t valid_mask;
    uint8_t report_words;
    uint8_t save_changed;

    if((control >= T5L_STC_MAPPED_CONTROL_COUNT) ||
       (((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U) &&
        (enabled > 1U)))
    {
        return 0U;
    }

    valid_mask = T5L_STC_MAPPED_FIELD_ENABLED;
    report_words = 1U;
    save_changed = 0U;
    switch(control)
    {
        case T5L_STC_MAPPED_EXHAUST:
        case T5L_STC_MAPPED_UVB:
        case T5L_STC_MAPPED_HUMIDIFIER:
            valid_mask |= T5L_STC_MAPPED_FIELD_SECONDARY |
                          T5L_STC_MAPPED_FIELD_TERTIARY;
            report_words = 3U;
            break;
        case T5L_STC_MAPPED_LIGHT:
        case T5L_STC_MAPPED_CLIMATE:
        case T5L_STC_MAPPED_INLET_FAN:
            valid_mask |= T5L_STC_MAPPED_FIELD_SECONDARY;
            report_words = 2U;
            break;
        default:
            break;
    }
    field_mask &= valid_mask;
    if(field_mask == 0U)
    {
        return 0U;
    }

    if((((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U) &&
        (((control == T5L_STC_MAPPED_EXHAUST) &&
          ((secondary < 1U) || (secondary > 6U))) ||
         ((control == T5L_STC_MAPPED_LIGHT) &&
          ((secondary < 1U) || (secondary > 4U))) ||
         ((control == T5L_STC_MAPPED_UVB) &&
          ((secondary < 1U) || (secondary > 4U))) ||
         ((control == T5L_STC_MAPPED_CLIMATE) &&
          ((secondary < 20U) || (secondary > 35U))) ||
         ((control == T5L_STC_MAPPED_HUMIDIFIER) &&
          ((secondary < 1U) || (secondary > 4U))) ||
         ((control == T5L_STC_MAPPED_INLET_FAN) &&
          ((secondary < 1U) || (secondary > 4U))))) ||
       (((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U) &&
        (((control == T5L_STC_MAPPED_EXHAUST) &&
          ((tertiary < 1U) || (tertiary > 5U))) ||
         ((control == T5L_STC_MAPPED_UVB) &&
          ((tertiary < 1U) || (tertiary > 4U))) ||
         ((control == T5L_STC_MAPPED_HUMIDIFIER) &&
          ((tertiary < 1U) || (tertiary > 3U))))))
    {
        return 0U;
    }

    switch(control)
    {
        case T5L_STC_MAPPED_EXHAUST:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Exhaust.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.Exhaust.speed != secondary) save_changed = 1U;
                G_Device_Ctrl.Exhaust.speed = secondary;
                G_Device_Ctrl.Exhaust.target_speed = secondary;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
            {
                if(G_Device_Ctrl.Exhaust.interval_time_h != tertiary) save_changed = 1U;
                G_Device_Ctrl.Exhaust.interval_time_h = tertiary;
            }
            report[0] = G_Device_Ctrl.Exhaust.enable;
            report[1] = G_Device_Ctrl.Exhaust.speed;
            report[2] = G_Device_Ctrl.Exhaust.interval_time_h;
            command_vp = OUTWIND_VP;
            report_vp = T5L_STC_REPORT_EXHAUST_VP;
            break;

        case T5L_STC_MAPPED_LIGHT:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Light.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.Light.brightness != secondary) save_changed = 1U;
                G_Device_Ctrl.Light.brightness = secondary;
                G_Device_Ctrl.Light.target_brightness = secondary;
            }
            report[0] = G_Device_Ctrl.Light.enable;
            report[1] = G_Device_Ctrl.Light.brightness;
            command_vp = LIGHT_VP;
            report_vp = T5L_STC_REPORT_LIGHT_VP;
            break;

        case T5L_STC_MAPPED_UVB:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.UVB.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.UVB.brightness != secondary) save_changed = 1U;
                G_Device_Ctrl.UVB.brightness = secondary;
                G_Device_Ctrl.UVB.target_brightness = secondary;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
            {
                if(G_Device_Ctrl.UVB.running_time_h != tertiary) save_changed = 1U;
                G_Device_Ctrl.UVB.running_time_h = tertiary;
            }
            report[0] = G_Device_Ctrl.UVB.enable;
            report[1] = G_Device_Ctrl.UVB.brightness;
            report[2] = G_Device_Ctrl.UVB.running_time_h;
            command_vp = UVB_VP;
            report_vp = T5L_STC_REPORT_UVB_VP;
            break;

        case T5L_STC_MAPPED_ANION:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Anion.enable = enabled;
            }
            report[0] = G_Device_Ctrl.Anion.enable;
            command_vp = ANION_VP;
            report_vp = T5L_STC_REPORT_ANION_VP;
            break;

        case T5L_STC_MAPPED_PLASMA:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Plasma.enable = enabled;
            }
            report[0] = G_Device_Ctrl.Plasma.enable;
            command_vp = PLASMA_VP;
            report_vp = T5L_STC_REPORT_PLASMA_VP;
            break;

        case T5L_STC_MAPPED_CLIMATE:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Heater.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.Heater.set_temp != secondary) save_changed = 1U;
                G_Device_Ctrl.Heater.set_temp = secondary;
                G_Device_Ctrl.Heater.target_temp = secondary;
            }
            report[0] = G_Device_Ctrl.Heater.enable;
            report[1] = G_Device_Ctrl.Heater.set_temp;
            command_vp = HEATER_VP;
            report_vp = T5L_STC_REPORT_CLIMATE_VP;
            break;

        case T5L_STC_MAPPED_HUMIDIFIER:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Humidifier.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.Humidifier.interval_time_h != secondary) save_changed = 1U;
                G_Device_Ctrl.Humidifier.interval_time_h = secondary;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
            {
                if(G_Device_Ctrl.Humidifier.running_time_h != tertiary) save_changed = 1U;
                G_Device_Ctrl.Humidifier.running_time_h = tertiary;
            }
            report[0] = G_Device_Ctrl.Humidifier.enable;
            report[1] = G_Device_Ctrl.Humidifier.interval_time_h;
            report[2] = G_Device_Ctrl.Humidifier.running_time_h;
            command_vp = MIST_VP;
            report_vp = T5L_STC_REPORT_HUMIDIFIER_VP;
            break;

        case T5L_STC_MAPPED_INLET_FAN:
            if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
            {
                G_Device_Ctrl.Inlet_Fan.enable = enabled;
            }
            if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
            {
                if(G_Device_Ctrl.Inlet_Fan.speed != secondary) save_changed = 1U;
                G_Device_Ctrl.Inlet_Fan.speed = secondary;
                G_Device_Ctrl.Inlet_Fan.target_speed = secondary;
            }
            report[0] = G_Device_Ctrl.Inlet_Fan.enable;
            report[1] = G_Device_Ctrl.Inlet_Fan.speed;
            command_vp = INWIND_VP;
            report_vp = T5L_STC_REPORT_INLET_FAN_VP;
            break;

        default:
            return 0U;
    }

    if(sync_command_vp != 0U)
    {
        read_dgus_vp(command_vp, (uint8_t *)command, report_words);
        if((field_mask & T5L_STC_MAPPED_FIELD_ENABLED) != 0U)
        {
            command[0] = report[0];
        }
        if((field_mask & T5L_STC_MAPPED_FIELD_SECONDARY) != 0U)
        {
            command[1] = report[1];
        }
        if((field_mask & T5L_STC_MAPPED_FIELD_TERTIARY) != 0U)
        {
            command[2] = report[2];
        }
        write_dgus_vp(command_vp, (uint8_t *)command, report_words);
    }
    write_dgus_vp(report_vp, (uint8_t *)report, report_words);
    if(save_changed != 0U)
    {
        Start_Once_SaveData();
    }
    return 1U;
}

uint8_t T5lStcSyncMappedControl(T5lStcMappedControl control,
                                uint8_t field_mask,
                                uint16_t enabled,
                                uint16_t secondary,
                                uint16_t tertiary)
{
    return T5lStcSyncMappedControlInternal(control, field_mask, enabled,
                                            secondary, tertiary, 0U);
}

uint8_t T5lStcSyncLocalMappedControl(T5lStcMappedControl control,
                                     uint8_t field_mask,
                                     uint16_t enabled,
                                     uint16_t secondary,
                                     uint16_t tertiary)
{
    return T5lStcSyncMappedControlInternal(control, field_mask, enabled,
                                            secondary, tertiary, 1U);
}

void Queue_Init(void)
{
    G_Queue.head = 0;
    G_Queue.tail = 0;
    G_Queue.err_cnt = 0;
    G_Queue.sta = STATE_IDLE;
    G_Queue.wait_ack_time = 0;
}

void DeRun_Auto(void)
{
    uint16_t addr_VP = 0;
    uint16_t dataChangeLen;
    uint8_t upLoadF00[4];
    uint8_t buf[16];
    read_dgus_vp(0x0f00, upLoadF00, 2);
    if (upLoadF00[0] == 0x5a)
    {
        addr_VP = *(uint16_t *)&upLoadF00[1];
        dataChangeLen = upLoadF00[3];
        *(uint16_t *)&buf[0] = addr_VP;
        read_dgus_vp(addr_VP, (uint8_t *)&buf[2], dataChangeLen);

        UartSendData(&Uart2, buf, (dataChangeLen * 2) + 2);
        upLoadF00[0] = 0;
        write_dgus_vp(0x0f00, (uint8_t *)&upLoadF00, 1);
    }
}
void T5l_Stc_Init(void)
{
    Dev_Init();
    Queue_Init();
    SysCfg_Init();
}

void Queue_Push(uint8_t *buf, uint8_t len)
{
    if (len > 20)
        return;
    memcpy(G_Queue.Data[G_Queue.tail].buf, buf, len);
    G_Queue.Data[G_Queue.tail].len = len;
    G_Queue.tail++;
    if (G_Queue.tail >= 16)
    {
        G_Queue.tail = 0;
    }
}

/**
 * @brief 队列出队函数
 * @details 从队列中取出一个元素，通过移动队列头指针实现
 *          队列采用循环队列实现，当头指针到达队列末尾时会回到起始位置
 */
void Queue_Pop_Poll(void)
{
    if (G_Queue.sta != STATE_IDLE)
    {
        return;
    }
    // 检查队列是否为空（头指针和尾指针不相等表示队列不为空）
    if (G_Queue.head != G_Queue.tail)
    {
        G_Queue.err_cnt = 0;
        G_Queue.sta = STATE_WAIT_ACK;
        G_Queue.wait_ack_time = WAIT_ACK_TIMEOUT;
        G_Queue.last_head = G_Queue.head;

        UartSendData(&Uart2, G_Queue.Data[G_Queue.head].buf, G_Queue.Data[G_Queue.head].len);
        // 头指针后移，实现出队操作
        G_Queue.head++;
        // 循环队列处理：当头指针达到队列容量上限时，回到起始位置
        if (G_Queue.head >= 16)
        {
            G_Queue.head = 0;
        }
    }
}

void Queue_Sta_Poll(void)
{
    switch (G_Queue.sta)
    {
    case STATE_WAIT_ACK_TIMEOUT: //超时重传
        G_Queue.err_cnt++;
        if (G_Queue.err_cnt > WAIT_ACK_ERROR_CNT)
        {
            G_Queue.sta = STATE_WAIT_ACK_FAILURE;
            break;
        }

        G_Queue.sta = STATE_WAIT_ACK;
        G_Queue.wait_ack_time = WAIT_ACK_TIMEOUT;
        UartSendData(&Uart2, G_Queue.Data[G_Queue.last_head].buf, G_Queue.Data[G_Queue.last_head].len);
        break;
    case STATE_WAIT_ACK_FAILURE: //无响应判断失败
        G_Queue.sta = STATE_IDLE;
        G_Queue.err_cnt = 0;
        break;
    case STATE_WAIT_ACK_SUCCESS: //成功
        G_Queue.sta = STATE_IDLE;
        G_Queue.err_cnt = 0;
        break;
    default:
        break;
    }

    if (G_Queue.showsuccse_sta == 1)
    {
        G_Queue.showsuccse_sta = 0;
        write_dgus_vp(0x535A, "\x00\x00", 1);
    }
}

//将时间间隔档位转换为秒数 time_tpye 0=间隔时间  1=单次运行时间
uint32_t Get_Interval_Run_Sec(DEV_TYPE dev_type, uint8_t time_tpye, uint16_t th)
{

    uint32_t ts = 0UL;
    if (dev_type < Type_OutWind || dev_type > Type_LOCK)
        return 0; //设备类型错误
    switch (dev_type)
    {
    case Type_OutWind:
        if (time_tpye == 0)
        {

            if (th == 1)
            {
                ts = 30 * 60;
                // ts = 10;
            }
            else if (th == 2)
            {
                ts = 1 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 5)
            {
                ts = 8 * 3600UL;
            }

        }
        break;
    case Type_InWind:

        break;
    case Type_UVC:
        if (time_tpye == 1)
        {
            if (th == 1)
            {
                ts = 15 * 60;
                // ts = 5;
            }
            else if (th == 2)
            {
                ts = 30 * 60;
            }
            else if (th == 3)
            {
                ts = 60 * 60;
            }
        }
        break;
    case Type_UVB:
        if (time_tpye == 0)
        {
            if (th == 1)
            {
                ts = (24 - 2) * 3600UL;
            }
            else if (th == 2)
            {
                ts = (24 - 4) * 3600UL;
            }
            else if (th == 3)
            {
                ts = (24 - 6) * 3600UL;
            }
            else if (th == 4)
            {
                ts = (24 - 8) * 3600UL;
            }
        }
        else if (time_tpye == 1)
        {
            if (th == 1)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 2)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 6 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 8 * 3600UL;
            }
        }

        break;
    case Type_MIST:
        if (time_tpye == 0)
        {
            if (th == 1)
            {
                ts = 2 * 3600UL;
            }
            else if (th == 2)
            {
                ts = 4 * 3600UL;
            }
            else if (th == 3)
            {
                ts = 8 * 3600UL;
            }
            else if (th == 4)
            {
                ts = 12 * 3600UL;
            }
        }
        break;
    case Type_LIGHT:

        break;
    case Type_HEATER:

        break;
    case Type_IR:

        break;
    case Type_PLASMA1:

        break;
    case Type_ANION:

        break;
    case Type_LOCK:

        break;
    default:
        break;
    }
    return ts;
}

static uint8_t T5lStcAcknowledgedRunState(uint16_t addr)
{
    DATA_NODE *node;
    uint16_t queued_addr;

    node = &G_Queue.Data[G_Queue.last_head];
    if(node->len >= 8U)
    {
        queued_addr = (uint16_t)(((uint16_t)node->buf[4] << 8) |
                                 node->buf[5]);
        if(queued_addr == addr)
        {
            return node->buf[7];
        }
    }

    switch(addr)
    {
        case OUTWIND_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Exhaust.target_speed;
        case INWIND_CMDWORD:
        case INWIND2_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed;
        case LIGHT_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Light.target_brightness;
        case UVB_CMDWORD:
            return (uint8_t)G_Device_Ctrl.UVB.target_brightness;
        case HEATER_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Heater.enable;
        case ANION_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Anion.enable;
        case PLASMA_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Plasma.enable;
        case HUMIDIFIER_CMDWORD:
            return (uint8_t)G_Device_Ctrl.Humidifier.enable;
        default:
            return 0U;
    }
}

//应答成功回调
void T5l_Stc_UartRxProcess(uint16_t addr)
{
    uint16_t tmp;
    uint16_t time_value;
    uint8_t run_state;
    if (addr >= 6100 && addr <= 0x61FF)
    {
        G_Queue.sta = STATE_WAIT_ACK_SUCCESS;
        write_dgus_vp(0x535A, "\x00\x01", 1); //设定成功
        G_Queue.showsuccse_time_ms = 1000;
        run_state = T5lStcAcknowledgedRunState(addr);
        switch (addr)
        {
        case OUTWIND_CMDWORD:
            if (run_state == 0U)
            {
                G_Device_Ctrl.Exhaust.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Exhaust.auto_vent_en = 1;
                G_Device_Ctrl.Exhaust.running = TRUE;
                G_Device_Ctrl.Exhaust.running_time_sec = OUTWIND_AUTO_RUN_TIME_SEC;
                read_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&time_value, 1);
                if (time_value != G_Device_Ctrl.Exhaust.interval_time_h)
                {
                    G_Device_Ctrl.Exhaust.interval_time_h = time_value;
                    Start_Once_SaveData();
                }
            }
            break;
        case INWIND_CMDWORD:
        case INWIND2_CMDWORD:
            if (run_state == 0U)
            {
                G_Device_Ctrl.Inlet_Fan.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Inlet_Fan.running = TRUE;
            }
            break;
        case LIGHT_CMDWORD:
            if (run_state == 0U)
            {
                G_Device_Ctrl.Light.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Light.running = TRUE;
            }
            break;
        case IR_CMDWORD:

            break;
        case UVC_CMDWORD:
            // UVC
            if (G_Device_Ctrl.UVC.enable == 1)
            {
                G_Device_Ctrl.UVC.enable = 0;
                read_dgus_vp(0x14, (uint8_t *)&tmp, 1);
                if (tmp == 13)
                {
                    SwitchPageById(12);
                }
            }
            else
            {
                G_Device_Ctrl.UVC.enable = 1;
                SwitchPageById(13);
                read_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1);
                G_Device_Ctrl.UVC.running_time_s = Get_Interval_Run_Sec(Type_UVC, 1, G_Device_Ctrl.UVC.running_time_h);
            }

            Start_Once_SaveData();
            break;
        case UVB_CMDWORD:
            if (run_state == 0U)
            {
                G_Device_Ctrl.UVB.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.UVB.auto_UVB_en = 1;
                G_Device_Ctrl.UVB.running = TRUE;
            }

            if (run_state != 0U)
            {
                if (G_Device_Ctrl.UVB.brightness != run_state)
                {
                    G_Device_Ctrl.UVB.brightness = run_state;
                    Start_Once_SaveData();
                }
            }
            read_dgus_vp(UVB_VP + 2, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.UVB.running_time_h)
            {
                G_Device_Ctrl.UVB.running_time_h = time_value;
                Start_Once_SaveData();
            }

            G_Device_Ctrl.UVB.running_time_s = Get_Interval_Run_Sec(Type_UVB, 1, G_Device_Ctrl.UVB.running_time_h);
            break;
        case HEATER_CMDWORD:
            if (run_state == 0U)
            {
                G_Device_Ctrl.Inlet_Fan.heater_auto_en = 0;
                G_Device_Ctrl.Inlet_Fan.off_delay_sec = 5;
                G_Device_Ctrl.Heater.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Inlet_Fan.heater_auto_en = 1;
                G_Device_Ctrl.Heater.aotu_heater_en = 1;
                G_Device_Ctrl.Heater.run_status = 2;
                G_Device_Ctrl.Heater.running = TRUE;
            }
            break;
        case 0x6108:

            break;
        case ANION_CMDWORD: // NAI
            if (run_state != 0U)
            {
                G_Device_Ctrl.Inlet_Fan.anion_auto_en = 1;
                G_Device_Ctrl.Anion.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.Inlet_Fan.anion_auto_en = 0;
                G_Device_Ctrl.Inlet_Fan.run_status = 1;
                G_Device_Ctrl.Anion.running = FALSE;
            }
            break;
        case PLASMA_CMDWORD: // PLASMA
            if (run_state != 0U)
            {
                G_Device_Ctrl.Inlet_Fan.plasma_auto_en = 1;
                G_Device_Ctrl.Plasma.running = TRUE;
            }
            else
            {
                G_Device_Ctrl.Inlet_Fan.plasma_auto_en = 0;
                G_Device_Ctrl.Inlet_Fan.run_status = 1;
                G_Device_Ctrl.Plasma.running = FALSE;
            }
            break;
        case HUMIDIFIER_CMDWORD: // Humidifier
            if (run_state == 0U)
            {
                G_Device_Ctrl.Humidifier.running = FALSE;
            }
            else
            {
                G_Device_Ctrl.Humidifier.auto_mist_en = 1;
                G_Device_Ctrl.Humidifier.running = TRUE;
            }

            read_dgus_vp(MIST_VP + 1, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.Humidifier.running_time_h)
            {
                G_Device_Ctrl.Humidifier.running_time_h = time_value;
                Start_Once_SaveData();
            }

            read_dgus_vp(MIST_VP + 2, (uint8_t *)&time_value, 1);
            if (time_value != G_Device_Ctrl.Humidifier.interval_time_h)
            {
                G_Device_Ctrl.Humidifier.interval_time_h = time_value;
                Start_Once_SaveData();
            }
            if (G_Device_Ctrl.Humidifier.running_time_h == 1)
            {
                G_Device_Ctrl.Humidifier.running_time_s = 1800; // sec
            }
            else if (G_Device_Ctrl.Humidifier.running_time_h == 2)
            {
                G_Device_Ctrl.Humidifier.running_time_s = 3600; // sec
            }
            else if (G_Device_Ctrl.Humidifier.running_time_h == 3)
            {
                G_Device_Ctrl.Humidifier.running_time_s = 7200; // sec
            }
            break;
        case LOCK_CMDWORD:

            break;
        default:
            break;
        }
    }
}

//状态flash双备份（预留）
void Stc_FlashBackup(void)
{
    // TODO
}

/*
/*
cmd
0x6100	排风扇_档位设置
0x6101	进风扇 档位设置
0x6103	照明灯_档位设置
0x6104	红外灯_开关设置
0x6105	UVC_开关设置
0x6106	UVB_档位设置
0x6107	加热器_开关设置

0x6109	负离子_开关设置
0x610A	等离子_开关设置

0x610B	雾化器_开关设置
0x610C	锁_开关设置

run_sta:
-对只用开关的设备0=OFF，1=ON
-对有档位的设备0=OFF，1~n档位
*/

void Send_Cmd_Ctrl(uint16_t cmdword, uint8_t run_sta)
{
    uint8_t cmdbuf[16];
    uint8_t t_len = 0;
    uint16_t crc;
    if (cmdword < 0x6100 || cmdword > 0x6110)
        return;
    cmdbuf[t_len++] = 0x5A;
    cmdbuf[t_len++] = 0xA5;

    cmdbuf[t_len++] = 0x00; //长度
    cmdbuf[t_len++] = 0x82;

    *(uint16_t *)&cmdbuf[t_len] = cmdword;
    t_len += 2;

    if (run_sta)
    {
        //打开
        cmdbuf[t_len++] = 0;
        cmdbuf[t_len++] = run_sta;
    }
    else
    {
        //关闭
        cmdbuf[t_len++] = 0;
        cmdbuf[t_len++] = OFF;
    }

    //校验
    crc = crc_16((uint8_t *)&cmdbuf[3], t_len - 3);
    cmdbuf[t_len++] = (uint8_t)(crc & 0x00FF);
    cmdbuf[t_len++] = (uint8_t)(crc >> 8);
    // t_len += 2;
    cmdbuf[2] = t_len - 3; // 长度
    //添加到发送队列
    Queue_Push(cmdbuf, t_len);
}
/**
 * @brief 队列超时检查函数 放入1ms定时器中
 * @details 该函数用于检查队列等待应答的状态，并在等待超时后更新队列状态
 */
void Queue_Time_Check(void)
{
    // 检查队列当前状态是否为等待应答状态
    if (G_Queue.sta == STATE_WAIT_ACK)
    {
        // 如果存在等待应答的计时器
        if (G_Queue.wait_ack_time)
        {
            // 计时器递减
            G_Queue.wait_ack_time--;
            // 如果计时器减到0，表示应答超时
            if (G_Queue.wait_ack_time == 0)
            {
                // 更新队列状态为等待应答超时状态
                G_Queue.sta = STATE_WAIT_ACK_TIMEOUT;
            }
        }
    }
    if (G_Queue.showsuccse_time_ms)
    {
        G_Queue.showsuccse_time_ms--;
        if (G_Queue.showsuccse_time_ms == 0)
        {
            G_Queue.showsuccse_sta = 1;
        }
    }
}

void T5L_Stc_Poll(void)
{
    Queue_Pop_Poll();
    Queue_Sta_Poll();
    DeRun_Auto();
    Sys_Cfg();
    Flash_SaveData();
    Dev_Aotu_Procese();
    key_scanf();
}
//系统配置,此方式配置的参数,是掉电不保存的
// is_beep:是否开启触摸屏提示音
// is_sleep:是否开启触摸屏自动待机休眠的功能
void sys_config(uint8_t is_beep, uint8_t is_sleep)
{
#define CONFIG_ADDR 0x80
    uint8_t config_cmd[4];

    //先把之前的设置先读取出来
    read_dgus_vp(CONFIG_ADDR, config_cmd, 2);

    //是否开启提示音
    if (is_beep)
        config_cmd[3] |= 0x08;
    else
        config_cmd[3] &= 0xf7;
    //自动休眠控制
    if (is_sleep)
        config_cmd[3] |= 0x04;
    else
        config_cmd[3] &= 0xfb;

    //再写回去
    config_cmd[0] = 0x5a; //启动写操作
    write_dgus_vp(CONFIG_ADDR, config_cmd, 2);
}

void Exhaust_On()
{
    uint16_t interval_code;

    read_dgus_vp(OUTWIND_VP + 1, (uint8_t *)&G_Device_Ctrl.Exhaust.target_speed, 1);
    read_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&interval_code, 1);
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_EXHAUST,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY |
        T5L_STC_MAPPED_FIELD_TERTIARY,
        1U, G_Device_Ctrl.Exhaust.target_speed, interval_code);
    Send_Cmd_Ctrl(OUTWIND_CMDWORD, G_Device_Ctrl.Exhaust.target_speed);
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void Exhaust_Off(uint8_t auto_flag)
{
    G_Device_Ctrl.Exhaust.auto_vent_en = auto_flag;
    G_Device_Ctrl.Exhaust.target_speed = 0;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_EXHAUST,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(OUTWIND_CMDWORD, (uint8_t)G_Device_Ctrl.Exhaust.target_speed);
}

void Humidifier_On()
{
    uint16_t interval_code;
    uint16_t running_code;

    read_dgus_vp(MIST_VP + 1, (uint8_t *)&interval_code, 1);
    read_dgus_vp(MIST_VP + 2, (uint8_t *)&running_code, 1);
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_HUMIDIFIER,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY |
        T5L_STC_MAPPED_FIELD_TERTIARY,
        1U, interval_code, running_code);
    Send_Cmd_Ctrl(HUMIDIFIER_CMDWORD, 1);
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void Humidifier_Off(uint8_t auto_flag)
{
    G_Device_Ctrl.Humidifier.auto_mist_en = auto_flag;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_HUMIDIFIER,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(HUMIDIFIER_CMDWORD, 0);
}

void UVB_On(uint16_t target_brightness)
{
    uint16_t daily_code;

    G_Device_Ctrl.UVB.target_brightness = target_brightness;
    read_dgus_vp(UVB_VP + 2, (uint8_t *)&daily_code, 1);
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_UVB,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY |
        T5L_STC_MAPPED_FIELD_TERTIARY,
        1U, target_brightness, daily_code);
    Send_Cmd_Ctrl(UVB_CMDWORD, (uint8_t)target_brightness);
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void UVB_Off(uint8_t auto_flag)
{
    G_Device_Ctrl.UVB.auto_UVB_en = auto_flag;
    G_Device_Ctrl.UVB.target_brightness = 0;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_UVB,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(UVB_CMDWORD, (uint8_t)G_Device_Ctrl.UVB.target_brightness);
}

void UVC_On()
{
    G_Device_Ctrl.UVC.enable = 0;
    Send_Cmd_Ctrl(UVC_CMDWORD, 1);
}
void UVC_Off()
{
    G_Device_Ctrl.UVC.enable = 1;
    Send_Cmd_Ctrl(UVC_CMDWORD, 0);
}

//调用此函数时需要先把目标值负值给G_Device_Ctrl.Heater.target_temp，并传入target_tmp
void Heater_On(int16_t target_tmp)
{
    G_Device_Ctrl.Heater.target_temp = (uint16_t)target_tmp;
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_CLIMATE,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY,
        1U, (uint16_t)target_tmp, 0U);
    //判断温差
    if ((G_Device_Ctrl.environment.temperature + 5) < target_tmp)
    {
        Send_Cmd_Ctrl(HEATER_CMDWORD, 1);
    }
    else
    {
        G_Device_Ctrl.Heater.aotu_heater_en = 1;
        G_Device_Ctrl.Heater.running = 1;
        write_dgus_vp(HEATER_VP, "\x00\x01", 1);
    }
}

//正常关闭auto_flag=0; 定时间隔关闭auto_flag=1;0更新开关状态  1不更新开关状态
void Heater_Off(uint8_t auto_flag)
{
    G_Device_Ctrl.Heater.aotu_heater_en = auto_flag;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_CLIMATE,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(HEATER_CMDWORD, 0);
}

void InWind_On(uint16_t speed)
{
    G_Device_Ctrl.Inlet_Fan.target_speed = speed;
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_INLET_FAN,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY,
        1U, speed, 0U);
    Send_Cmd_Ctrl(INWIND_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
    Send_Cmd_Ctrl(INWIND2_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
}
void InWind_Off()
{
    G_Device_Ctrl.Inlet_Fan.target_speed = 0;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_INLET_FAN,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(INWIND_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
    Send_Cmd_Ctrl(INWIND2_CMDWORD, (uint8_t)G_Device_Ctrl.Inlet_Fan.target_speed);
}

void Light_On(uint16_t target_brightness)
{
    G_Device_Ctrl.Light.target_brightness = target_brightness;
    (void)T5lStcSyncLocalMappedControl(
        T5L_STC_MAPPED_LIGHT,
        T5L_STC_MAPPED_FIELD_ENABLED | T5L_STC_MAPPED_FIELD_SECONDARY,
        1U, target_brightness, 0U);
    Send_Cmd_Ctrl(LIGHT_CMDWORD, (uint8_t)target_brightness);
}

void Light_Off()
{
    G_Device_Ctrl.Light.target_brightness = 0;
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_LIGHT,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(LIGHT_CMDWORD, (uint8_t)G_Device_Ctrl.Light.target_brightness);
}

void Anion_On(void)
{
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_ANION,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  1U, 0U, 0U);
    Send_Cmd_Ctrl(ANION_CMDWORD, 1U);
}

void Anion_Off(void)
{
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_ANION,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(ANION_CMDWORD, 0U);
}

void Plasma_On(void)
{
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_PLASMA,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  1U, 0U, 0U);
    Send_Cmd_Ctrl(PLASMA_CMDWORD, 1U);
}

void Plasma_Off(void)
{
    (void)T5lStcSyncLocalMappedControl(T5L_STC_MAPPED_PLASMA,
                                  T5L_STC_MAPPED_FIELD_ENABLED,
                                  0U, 0U, 0U);
    Send_Cmd_Ctrl(PLASMA_CMDWORD, 0U);
}

uint8_t check_passwd_format(uint8_t *u8buf)
{
    uint8_t i;
    for (i = 0; i < PASSWD_BTYELEN + 1; i++)
    {
        if (u8buf[i] == 0xFF)
        {
            break;
        }
    }

    if (i >= 4)
    {
        return 1;
    }
    else
    {
        write_dgus_vp(0x535A, "\x00\x02", 1); //输入4位密码
        G_Queue.showsuccse_time_ms = 1500;
        return 0;
    }
}

uint8_t compare_passwd(uint8_t *buf)
{
    uint8_t  ret;

    ret = check_passwd_format(buf);
    if (ret == 1)
    {
        if (0 == memcmp(buf, G_Device_Ctrl.Cfg.passwd, PASSWD_BTYELEN))
        {
            return 1; //密码正确
        }
        else
        {
            write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
            G_Queue.showsuccse_time_ms = 1500;
        }
    }
    return 0;
}

void touch_ctrl(uint16_t keyvalue)
{
    uint16_t u16buf[4];
    u16buf[0] = 0x5AA5;
    u16buf[1] = 0x0004;
    u16buf[2] = 0xFF00 | (keyvalue);
    u16buf[3] = 0x0001;
    write_dgus_vp(0x00D4, (uint8_t *)&u16buf[0], 4);
}

void key_scanf(void)
{
    uint8_t u8buf[20];
    uint16_t key_value, tmp;
 
    read_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
    if (key_value != 0)
    {
        // Send_Cmd_Ctrl(Exhaust,1,1,3);
        // UartSendData(&Uart2,"\xAA\xFF\n",3);

        switch (key_value)
        {

        case 0x301:
            //进入设置-密码
            SwitchPageById(4);
        case 0x403:
            touch_ctrl(4);
            break;
        case 0x401:
            read_dgus_vp(0x5380, u8buf, 3);
            if (compare_passwd(u8buf))
            {
                SwitchPageById(5);
            }
            break;
        case 0x1401:
            //锁密码-确认
            break;
            //===============================加热start================================//
        case 0x500: //加热-开关
            //需要同步检测温度是否到达目标温度
            if (G_Device_Ctrl.Heater.enable == FALSE)
            {
                read_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.target_temp, 1);
                Heater_On((int16_t)G_Device_Ctrl.Heater.target_temp);
            }
            else
            {
                Heater_Off(0);
            }
            break;
        case 0x601: //加热-确定
            read_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.target_temp, 1);
            Heater_On(G_Device_Ctrl.Heater.target_temp);

            break;
        case 0x602: //加热-取消
            write_dgus_vp(HEATER_VP + 1, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1);

            break;
            //===============================加热end================================//
            //===============================雾化start================================//
        case 0x501: //雾化-开关
            //读取时间，设置定时间隔
            if (G_Device_Ctrl.Humidifier.enable == 0)
            {
                Humidifier_On();
            }
            else
            {
                Humidifier_Off(0);
            }
            break;
        case 0x701: //雾化-确定
            Humidifier_On();
            break;
        case 0x702: //雾化-取消
            write_dgus_vp(MIST_VP + 1, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1);
            write_dgus_vp(MIST_VP + 2, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
            break;
            //===============================雾化end================================//
            //===============================照明灯start================================//
        case 0x502: //照明灯-开关
            //读取档位
            if (G_Device_Ctrl.Light.enable == 0)
            {
                read_dgus_vp(LIGHT_VP + 1, (uint8_t *)&tmp, 1);
                Light_On(tmp);
            }
            else if (G_Device_Ctrl.Light.enable == 1)
            {
                // G_Device_Ctrl.Light.target_brightness = 0;
                Light_Off();
            }
            break;
        case 0x801: //照明灯-确定
            //只执行打开操作
            read_dgus_vp(LIGHT_VP + 1, (uint8_t *)&G_Device_Ctrl.Light.target_brightness, 1);
            Light_On(G_Device_Ctrl.Light.target_brightness);
            break;
        case 0x802: //照明灯-取消
            write_dgus_vp(LIGHT_VP + 1, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1);
            break;
            //===============================照明灯end================================//
            //===============================排风start================================//
        case 0x503: //排风-开关
            //读取档位，设置定时间隔
            if (G_Device_Ctrl.Exhaust.enable == 0)
            {
                Exhaust_On();
            }
            else
            {
                Exhaust_Off(0);
            }
            break;
        case 0x901: //排风-确定
            //只执行打开操作
            Exhaust_On();
            break;
        case 0x902: //排风-取消
            write_dgus_vp(OUTWIND_VP + 1, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1);
            write_dgus_vp(OUTWIND_VP + 2, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1);
            break;
            //===============================排风end================================//
            //===============================负离子 start================================//

        case 0x504: //负离子开关
            if(G_Device_Ctrl.Anion.enable == 0U)
            {
                Anion_On();
            }
            else
            {
                Anion_Off();
            }
            break;
            //===============================负离子 end================================//

            //===============================UVB Start================================//
        case 0x505: // UVB-开关
            //读取档位，设置定时间隔
            if (G_Device_Ctrl.UVB.enable == FALSE)
            {
                read_dgus_vp(UVB_VP + 1, (uint8_t *)&tmp, 1);
                UVB_On(tmp);
            }
            else
            {
                UVB_Off(0);
            }
            break;
        case 0x1001: // UVB-确定
            //只执行打开操作
            read_dgus_vp(UVB_VP + 1, (uint8_t *)&tmp, 1);
            UVB_On(tmp);
            break;
        case 0x1002: // UVB-取消
            write_dgus_vp(UVB_VP + 1, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1);
            write_dgus_vp(UVB_VP + 2, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);
            break;
            //===============================UVB End================================//
            //===============================等离子 start================================//
        case 0x506: //等离子开关
            if(G_Device_Ctrl.Plasma.enable == 0U)
            {
                Plasma_On();
            }
            else
            {
                Plasma_Off();
            }
            break;
            //===============================等离子 end================================//
            //===============================紫外消杀 start================================//
        case 0x1101:
            SwitchPageById(15);
        case 0x1503:
            write_dgus_vp(0x5360, "\xFF\xFF", 1);
            touch_ctrl(5);
            break;

        case 0x1201: //紫外消杀-确认
            UVC_On();
            break;
        case 0x1202: //紫外消杀-取消
            write_dgus_vp(UVC_VP + 1, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1);
            break;
        case 0x1301: //紫外消杀-中断
            UVC_Off();
            break;
        case 0x1501: //紫外消杀-密码确认
                     //密码比对
            read_dgus_vp(0x5360, u8buf, 3);
            if (compare_passwd(u8buf))
            {
                SwitchPageById(12);
            }

            break;
            //===============================紫外消杀 end================================//
            //===============================滤芯 start================================//
        case 0x508:

            SwitchPageById(34);
            break;
        case 0x3403:
            //暂时忽略

            break;
        case 0x3404:
            //立即更新

            break;
        case 0x3402:
            //取消
            write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);
            break;
        case 0x3401:
            //滤芯设定
            read_dgus_vp(0x5349, (uint8_t *)&tmp, 1);
            if (G_Device_Ctrl.Humidifier.filter_remind_month != tmp)
            {
                G_Device_Ctrl.Humidifier.filter_remind_month = tmp;
                Start_Once_SaveData();
            }
            break;
            //===============================滤芯 end================================//
        case 0x2101: // 21页-确认
            read_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1);
            if (G_Device_Ctrl.Cfg.temp_uint == 1)
            {
                tmp = (uint16_t)(45 << 8) | 0;
                write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
            }
            else
            {
                tmp = (uint16_t)(6 << 8) | 0;
                write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
            }
            Flash_SaveData();
            break;
        case 0x2102: // 21页-取消
            write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1);
            break;
        case 0x2201: // 22页-显示-确认
            read_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp, 2);
            u8buf[0] = (uint8_t)(tmp);
            u8buf[1] = (uint8_t)(tmp);
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 2);

            read_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
            read_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);

            Start_Once_SaveData();
            break;
        case 0x2202: // 22页-显示-取消
            read_dgus_vp(0x82, (uint8_t *)&u8buf, 1);

            G_Device_Ctrl.Cfg.ligth_value = u8buf[0];
            tmp = (uint16_t)G_Device_Ctrl.Cfg.ligth_value;
            write_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp, 1); //真实亮度

            tmp = (((uint16_t)(tmp - 20) * 125) / 100);     //范围20-100
            write_dgus_vp(LIGHT_VALUE, (uint8_t *)&tmp, 1); // 1.25倍  20-0 60-50 100-100

            tmp = ((uint16_t)G_Device_Ctrl.Cfg.ligth_value - 20) * 4.5; // 0-50  75-180  100-360
            write_dgus_vp(LIGHT_NEEDLE_VP, (uint8_t *)&tmp, 1);         //指针

            write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
            write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);
            break;
        case 0x2601:
            //密码设置确认
            read_dgus_vp(0x5370, &u8buf[0], 18);

            if (check_passwd_format(&u8buf[0]))
            {
                if (check_passwd_format(&u8buf[6]))
                {
                    if (check_passwd_format(&u8buf[12]))
                    {
                        if (0 == memcmp(&u8buf[0], &G_Device_Ctrl.Cfg.passwd[0], 4))
                        {
                            if (0 == memcmp(&u8buf[6], &u8buf[12], 4))
                            {
                                //密码正确
                                memcpy(G_Device_Ctrl.Cfg.passwd, &u8buf[6], 4);
                                Start_Once_SaveData();
                            }
                            else
                            {
                                write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
                                G_Queue.showsuccse_time_ms = 1500;
                            }
                        }
                        else
                        {
                            write_dgus_vp(0x535A, "\x00\x03", 1); //密码错误
                            G_Queue.showsuccse_time_ms = 1500;
                        }
                    }
                }
            }
            break;
        case 0x26A1:
        case 0x26A2:
        case 0x26A3:
            tmp = key_value - 0x26A0;
            write_dgus_vp(0x5370 + ((tmp - 1) * 3), "\xFF\xFF", 1);
            touch_ctrl(tmp);
            break;
        case 0x2801:
            read_dgus_vp(0x5366, u8buf, 3);
            if (compare_passwd(u8buf))
            {
                SwitchPageById(29);
            }
            break;
        case 0x2006:
            write_dgus_vp(0x5366, "\xFF\xFF", 1);
            SwitchPageById(28);
        case 0x2803:
            touch_ctrl(6);
            break;
        case 0x3001:
            read_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1);
            Start_Once_SaveData();
            break;
        case 0x3002:
            write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1);
            tmp = G_Device_Ctrl.Cfg.volume * 3.6;
            write_dgus_vp(VOLUME_NEEDLE_VP, (uint8_t *)&tmp, 1); //指针
            break;
        case 0x3101:
            break;
        case 0x3102:
            break;
        case 0x3201:
            //语言-确认
            read_dgus_vp(0x535B, (uint8_t *)&G_Device_Ctrl.Cfg.language, 1);
            Start_Once_SaveData();
            break;
        default:
            break;
        }
        key_value = 0;
        write_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
    }
}

void SysCfg_Init()
{
    uint8_t u8buf[2];
    uint16_t tmp, tmp2;

    Flash_DataInit();
    sys_config(1, 0);

    //系统显示
    u8buf[0] = G_Device_Ctrl.Cfg.ligth_value;
    u8buf[1] = G_Device_Ctrl.Cfg.ligth_value;
    write_dgus_vp(0x82, (uint8_t *)&u8buf, 1); //亮度

    tmp = (uint16_t)G_Device_Ctrl.Cfg.ligth_value;
    write_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp, 1); //真实亮度

    tmp2 = (((uint16_t)(tmp - 20) * 125) / 100);     //范围20-100
    write_dgus_vp(LIGHT_VALUE, (uint8_t *)&tmp2, 1); // 1.25倍  20-0 60-50 100-100

    tmp2 = (tmp - 20) * 4.5;                             // 0-50  75-180  100-360
    write_dgus_vp(LIGHT_NEEDLE_VP, (uint8_t *)&tmp2, 1); //指针

    //息屏or屏保
    G_Device_Ctrl.Cfg.off_display_flag = (uint8_t)G_Device_Ctrl.Cfg.screen_save & 0x0F;
    //读取时间
    read_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1);
    G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);

    //音量

    SwitchPageById(3);
}

//判断屏幕是否被触摸
#define SCREEN_TOUCH_ADDR 0x0016
uint8_t screen_touch(void)
{
    uint8_t rewrite[2];
    uint16_t touch_data[3];
    rewrite[0] = 0x00;

    read_dgus_vp(SCREEN_TOUCH_ADDR, (uint8_t *)touch_data, 3);
    if ((touch_data[0] >> 8) == 0x5A)
    {
        rewrite[1] = touch_data[0] & 0xff;
        write_dgus_vp(SCREEN_TOUCH_ADDR, (uint8_t *)rewrite, 1);
        return 1;
    }
    return 0;
}

void Sys_Cfg(void)
{
    static uint16_t volume;
    static uint16_t last_page;
    static uint8_t page_chage = 0;
    uint8_t u8buf[8];
    uint16_t tmp1, tmp2;

    if (screen_touch())
    {
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);
        if (page_chage == 1)
        {
            page_chage = 0;
            SwitchPageById(last_page);
            u8buf[0] = G_Device_Ctrl.Cfg.ligth_value;
            u8buf[1] = G_Device_Ctrl.Cfg.ligth_value;
            write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);
            G_Device_Ctrl.Cfg.off_display_flag &= 0X0F;
        }
    }
    //读取系统亮度
    //触摸屏背光待机设置：
    // D3=开启亮度，0x00-0x64；背光待机控制关闭时，D3为软件亮度调节接口。
    // D2=关闭亮度0x00-0x64；
    // D1:0=开启时间/10mS。
    read_dgus_vp(LIGHT_ADD_SUB_VP, (uint8_t *)&tmp1, 1);
    if (tmp1 != (uint16_t)G_Device_Ctrl.Cfg.ligth_value)
    {
        G_Device_Ctrl.Cfg.ligth_value = (uint8_t)tmp1;
        tmp2 = (((uint16_t)(G_Device_Ctrl.Cfg.ligth_value - 20) * 125) / 100);
        write_dgus_vp(LIGHT_VALUE, (uint8_t *)&tmp2, 1);

        // // read_dgus_vp(0x82, (uint8_t *)&u8buf, 2);
        // u8buf[0] = (uint8_t)(tmp1);
        // u8buf[1] = (uint8_t)(tmp1);
        // write_dgus_vp(0x82, (uint8_t *)&u8buf, 2);

        tmp2 = (tmp1 - 20) * 4.5;
        write_dgus_vp(LIGHT_NEEDLE_VP, (uint8_t *)&tmp2, 1); //指针
    }

    // volume
    read_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&tmp1, 1);
    if (tmp1 != volume)
    {
        volume = tmp1;
        tmp2 = tmp1 * 3.6;
        write_dgus_vp(VOLUME_NEEDLE_VP, (uint8_t *)&tmp2, 1); //指针
    }
    //
    if (G_Device_Ctrl.Cfg.off_display_flag == 0x11)
    {
        if (page_chage == 0)
        {
            page_chage = 1;
            read_dgus_vp(0x14, (uint8_t *)&last_page, 1);
        }
        SwitchPageById(1);
        G_Device_Ctrl.Cfg.off_display_flag &= 0x01;
        G_Device_Ctrl.Cfg.off_display_flag |= 0x21;
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);
        // G_Device_Ctrl.Cfg.off_display_flag|=0x32;
    }
    else if (G_Device_Ctrl.Cfg.off_display_flag == 0x31)
    {
        SwitchPageById(2);
        G_Device_Ctrl.Cfg.off_display_flag &= 0x01;
        G_Device_Ctrl.Cfg.off_display_time_s = (G_Device_Ctrl.Cfg.off_display_time_min * 5 * 60);
    }
    else if (G_Device_Ctrl.Cfg.off_display_flag == 0x12)
    {
        if (page_chage == 0)
        {
            page_chage = 1;
            read_dgus_vp(0x14, (uint8_t *)&last_page, 1);
        }
        SwitchPageById(0);
        u8buf[0] = 0;
        u8buf[1] = 0;
        write_dgus_vp(0x82, (uint8_t *)&u8buf, 1);
        G_Device_Ctrl.Cfg.off_display_flag &= 0x02;
    }
}

//放入秒中断
void Dev_Time_Check()
{
    if (G_Device_Ctrl.Cfg.save_interval_times)
    {
        G_Device_Ctrl.Cfg.save_interval_times--;
    }

    //显示
    if (G_Device_Ctrl.Cfg.off_display_time_s)
    {
        G_Device_Ctrl.Cfg.off_display_time_s--;
        if (G_Device_Ctrl.Cfg.off_display_time_s == 0)
        {
            if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF1) == 0x01)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x11; //屏保1
            }
            else if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF1) == 0x21)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x31; //屏保2
            }
            else if ((G_Device_Ctrl.Cfg.off_display_flag & 0xF2) == 0x02)
            {
                G_Device_Ctrl.Cfg.off_display_flag |= 0x12;
            }
        }
    }

    //
    if (G_Device_Ctrl.Exhaust.running_time_sec)
    {
        G_Device_Ctrl.Exhaust.running_time_sec--;
        if (G_Device_Ctrl.Exhaust.running_time_sec == 0)
        {
            G_Device_Ctrl.Exhaust.run_status = 1;
        }
    }
    if (G_Device_Ctrl.Exhaust.interval_time_sec)
    {
        G_Device_Ctrl.Exhaust.interval_time_sec--;
        if (G_Device_Ctrl.Exhaust.interval_time_sec == 0)
        {
            G_Device_Ctrl.Exhaust.run_status = 2;
        }
    }

    if (G_Device_Ctrl.Humidifier.running_time_s)
    {
        G_Device_Ctrl.Humidifier.running_time_s--;
        if (G_Device_Ctrl.Humidifier.running_time_s == 0)
        {
            G_Device_Ctrl.Humidifier.run_status = 1;
        }
    }
    if (G_Device_Ctrl.Humidifier.interval_time_s)
    {
        G_Device_Ctrl.Humidifier.interval_time_s--;
        if (G_Device_Ctrl.Humidifier.interval_time_s == 0)
        {
            G_Device_Ctrl.Humidifier.run_status = 2;
        }
    }

    if (G_Device_Ctrl.UVB.running_time_s)
    {
        G_Device_Ctrl.UVB.running_time_s--;
        if (G_Device_Ctrl.UVB.running_time_s == 0)
        {
            G_Device_Ctrl.UVB.run_status = 1;
        }
    }
    if (G_Device_Ctrl.UVB.interval_time_s)
    {
        G_Device_Ctrl.UVB.interval_time_s--;
        if (G_Device_Ctrl.UVB.interval_time_s == 0)
        {
            G_Device_Ctrl.UVB.run_status = 2;
        }
    }

    if (G_Device_Ctrl.UVC.running_time_s)
    {
        G_Device_Ctrl.UVC.running_time_s--;
        if (G_Device_Ctrl.UVC.running_time_s == 0)
        {
            G_Device_Ctrl.UVC.run_status = 1;
        }
    }

    if (G_Device_Ctrl.Inlet_Fan.off_delay_sec)
    {
        G_Device_Ctrl.Inlet_Fan.off_delay_sec--;
        if (G_Device_Ctrl.Inlet_Fan.off_delay_sec == 0)
        {
            G_Device_Ctrl.Inlet_Fan.run_status = 1;
        }
    }
}

void Dev_Aotu_Procese()
{
    if (G_Device_Ctrl.Exhaust.auto_vent_en == 1)
    {
        if (G_Device_Ctrl.Exhaust.run_status == 1)
        {
            G_Device_Ctrl.Exhaust.run_status = 0;
            G_Device_Ctrl.Exhaust.interval_time_sec = Get_Interval_Run_Sec(Type_OutWind, 0, G_Device_Ctrl.Exhaust.interval_time_h);
            Exhaust_Off(1);
        }
        if (G_Device_Ctrl.Exhaust.run_status == 2)
        {
            G_Device_Ctrl.Exhaust.run_status = 0;
            Exhaust_On();
        }
    }

    if (G_Device_Ctrl.Humidifier.auto_mist_en == 1)
    {
        if (G_Device_Ctrl.Humidifier.run_status == 1)
        {
            G_Device_Ctrl.Humidifier.run_status = 0;
            G_Device_Ctrl.Humidifier.interval_time_s = Get_Interval_Run_Sec(Type_MIST, 0, G_Device_Ctrl.Humidifier.interval_time_h);
            Humidifier_Off(1);
        }
        if (G_Device_Ctrl.Humidifier.run_status == 2)
        {
            G_Device_Ctrl.Humidifier.run_status = 0;
            Humidifier_On();
        }
    }

    if (G_Device_Ctrl.UVB.auto_UVB_en == 1)
    {
        if (G_Device_Ctrl.UVB.run_status == 1)
        {
            G_Device_Ctrl.UVB.run_status = 0;
            G_Device_Ctrl.UVB.interval_time_s = Get_Interval_Run_Sec(Type_UVB, 0, G_Device_Ctrl.UVB.running_time_h);
            UVB_Off(1);
        }
        if (G_Device_Ctrl.UVB.run_status == 2)
        {
            G_Device_Ctrl.UVB.run_status = 0;
            UVB_On(G_Device_Ctrl.UVB.brightness);
        }
    }

    if (G_Device_Ctrl.UVC.run_status == 1)
    {
        G_Device_Ctrl.UVC.run_status = 0;
        UVC_Off();
    }

    if (G_Device_Ctrl.Heater.aotu_heater_en)
    {
        if ((G_Device_Ctrl.environment.temperature + 5) < (int16_t)G_Device_Ctrl.Heater.target_temp)
        {

            if (G_Device_Ctrl.Heater.run_status != 2)
            {
                G_Device_Ctrl.Heater.run_status = 2;
                Heater_On(G_Device_Ctrl.Heater.target_temp);
            }
        }
        else
        {
            if (G_Device_Ctrl.Heater.run_status == 2)
            {
                G_Device_Ctrl.Heater.run_status = 1;
                Heater_Off(1);
            }
        }
    }

    if (G_Device_Ctrl.Inlet_Fan.heater_auto_en)
    {
        if (G_Device_Ctrl.Inlet_Fan.run_status != 2)
        {
            G_Device_Ctrl.Inlet_Fan.run_status = 2;
            InWind_On(4);
        }
    }
    else
    {
        if (G_Device_Ctrl.Inlet_Fan.anion_auto_en || G_Device_Ctrl.Inlet_Fan.plasma_auto_en)
        {
            if (G_Device_Ctrl.Inlet_Fan.run_status != 2)
            {
                G_Device_Ctrl.Inlet_Fan.run_status = 2;
                InWind_On(1);
            }
        }
        else
        {
            if (G_Device_Ctrl.Inlet_Fan.run_status == 1)
            {
                G_Device_Ctrl.Inlet_Fan.run_status = 0;
                InWind_Off();
            }
        }
    }
}

void STC_ReporData_Procese(uint32_t addr)
{
    uint16_t u16buf[32];
    if (addr < 0x6000 || addr > 0x600F)
        return;
    read_dgus_vp(0x6000, (uint8_t *)&u16buf, 32);

    if (u16buf[2] == 0)
    {
        G_Device_Ctrl.environment.temperaturex10 = (int16_t)(u16buf[0]);
        G_Device_Ctrl.environment.temperature = (int16_t)(u16buf[0] / 10);
        G_Device_Ctrl.environment.humidity = u16buf[1];
    }
    else
    {
        G_Device_Ctrl.environment.temperaturex10 = (int16_t)(u16buf[0]);
        G_Device_Ctrl.environment.temperature = (int16_t)(u16buf[3] / 10);
    }
    if (u16buf[20] == 0)
    {
        write_dgus_vp(0x5310, "\x00\x00", 1);
    }
    else
    {
        //温度 GXHTC3_故障码
        write_dgus_vp(0x5310, "\x00\x01", 1);
    }
    write_dgus_vp(0x5311, (uint8_t *)&G_Device_Ctrl.environment.temperature, 1);

    if (u16buf[6] != 0)
    {
        //排风扇_故障码
        write_dgus_vp(0x5328, "\x00\x01", 1);
    }
    else
    {
        write_dgus_vp(0x5328, "\x00\x00", 1);
    }
}

/*****************************************************************************
 函 数 名  :uint16_t Calculate_CRC16_Flash(unsigned char *updata, unsigned char len)
 功能描述  : CRC-16校验
 说明: 在通信协议中，发送方将 原始数据 + 其 CRC 校验码 一起传输。接收方调用此函数计算整个数据包(包括校验码部分)的CRC值，若结果为0，则说明数据无传输错误
 输入参数  : updata		要处理的数据
            len	长度
            mode  0用于验算CRC      1用于生成CRC值，并复制到数组中
 输出参数  :
*****************************************************************************/
uint16_t Calculate_CRC16_Flash(unsigned char *updata, unsigned int len, unsigned char mode)
{
    unsigned int Reg_CRC = 0xffff;
    unsigned int i, j;
    for (i = 0; i < len; i++)
    {
        Reg_CRC ^= *updata++;
        for (j = 0; j < 8; j++)
        {
            if (Reg_CRC & 0x0001)
            {
                Reg_CRC = Reg_CRC >> 1 ^ 0XA001;
            }
            else
            {
                Reg_CRC >>= 1;
            }
        }
    }
    if (mode == 1)
    {
        *updata++ = (uint8_t)Reg_CRC;
        *updata = (uint8_t)(Reg_CRC >> 8);
    }

    return Reg_CRC;
}

void Flash_DataInit(void)
{

    int16_t crc1, crc2, tmp;
    uint16_t err_sta = 0;
    uint16_t databuf[32], backdatabuf[32];
    uint8_t u8buf[4];
    memset(&G_Device_Ctrl, 0, 64);
    memset(&databuf, 0, 64);

    //读主区和备份区
    T5lNorFlashRW(flashREAD_FLAG, 0, MAIN_ADDR, MAIN_ADDR, (uint8_t *)&databuf, DATALEN / 2);
    T5lNorFlashRW(flashREAD_FLAG, 0, MAIN_ADDR, MAIN_ADDR, (uint8_t *)&backdatabuf, DATALEN / 2);

    if (0 == Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN, 0))
    {
        crc1 = 1;
    }
    else
    {
        crc1 = 0;
    }

    if (0 == Calculate_CRC16_Flash((uint8_t *)&backdatabuf, DATALEN, 0))
    {
        crc2 = 1;
    }
    else
    {
        crc2 = 0;
    }

    err_sta = 0; // 为0时,代表正常,无需更新数据

    if (crc1 == 0 && crc2 == 0)
    {
        /*
         * 1. 出场设置
         * 2. 两份都是错误的,需要执行出厂初始化
         * */
        memset(&databuf, 0, DATALEN);
        //排风
        databuf[0] = G_Device_Ctrl.Exhaust.speed = 2;
        databuf[1] = G_Device_Ctrl.Exhaust.interval_time_h = OutWind_TIMER_1H;
        write_dgus_vp(0x5101, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风
        write_dgus_vp(0x5102, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1);
        // write_dgus_vp(0x5328, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风

        // UVB
        databuf[2] = G_Device_Ctrl.UVB.brightness = 2;
        databuf[3] = G_Device_Ctrl.UVB.running_time_h = UVB_TIMER_2H;
        write_dgus_vp(0x510D, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1); // UVB
        write_dgus_vp(0x510E, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);

        //雾化
        databuf[4] = G_Device_Ctrl.Humidifier.interval_time_h = MIST_INTERVAL_TIMER_2H;
        databuf[5] = G_Device_Ctrl.Humidifier.running_time_h = MIST_RUN_TIMER_1H;
        write_dgus_vp(0x5111, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1); //雾化
        write_dgus_vp(0x5112, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
        //灯光
        databuf[6] = G_Device_Ctrl.Light.brightness = 2;
        write_dgus_vp(0x5115, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1); //灯光

        //加热
        databuf[7] = G_Device_Ctrl.Heater.set_temp = 26;
        write_dgus_vp(0x5119, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1); //加热

        // UVC
        databuf[8] = G_Device_Ctrl.UVC.running_time_h = UVC_TIMER_15M;
        write_dgus_vp(0x5109, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1); // UVC

        //温度单位
        databuf[9] = G_Device_Ctrl.Cfg.temp_uint = 1;
        write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1); //温度单位
        if (G_Device_Ctrl.Cfg.temp_uint == 1)
        {
            tmp = (uint16_t)(45 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        else
        {
            tmp = (uint16_t)(6 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        //屏保&息屏
        read_dgus_vp(0x82, u8buf, 1);
        databuf[10] = G_Device_Ctrl.Cfg.ligth_value = u8buf[0];
        databuf[11] = G_Device_Ctrl.Cfg.screen_save = 1;
        databuf[12] = G_Device_Ctrl.Cfg.off_display_time_min = 1;
        write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&databuf[11], 1); //屏保&息屏
        write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&databuf[12], 1);     //屏保&息屏

        //音量
        databuf[13] = G_Device_Ctrl.Cfg.volume = 50;
        write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&databuf[13], 1); //

        //进风
        databuf[14] = G_Device_Ctrl.Inlet_Fan.speed = 2;
        databuf[15] = 0;

        //密码
        memcpy(&G_Device_Ctrl.Cfg.passwd[0], PASSWD, 4);
        memcpy(&databuf[16], &G_Device_Ctrl.Cfg.passwd[0], 4);

        //语言
        databuf[18] = G_Device_Ctrl.Cfg.language = 1;
        write_dgus_vp(0x535B, (uint8_t *)&G_Device_Ctrl.Cfg.language, 1);

        //滤芯提醒
        databuf[19] = G_Device_Ctrl.Humidifier.filter_remind_month = 2;
        set_filter_month();
        databuf[20] = G_Device_Ctrl.TargetDate.year;
        databuf[21] = ((uint16_t)G_Device_Ctrl.TargetDate.month << 8) | G_Device_Ctrl.TargetDate.day;
        write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);

        databuf[22] =G_Device_Ctrl.Humidifier.filter_need_replace = 0;

        Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN - 2, 1);
        // 复制到备份
        T5lNorFlashRW(flashWRITE_FLAG, 0, BACK_ADDR, MAIN_ADDR, (uint8_t *)&databuf, DATALEN / 2);
        T5lNorFlashRW(flashWRITE_FLAG, 0, MAIN_ADDR, BACK_ADDR, (uint8_t *)&databuf, DATALEN / 2);
        err_sta = 4;
    }
    else if (crc1 == 1 || crc2 == 1)
    {

        if (crc1 == 1 && crc2 == 0) // 仅主份正常
        {
            T5lNorFlashRW(flashWRITE_FLAG, 0, BACK_ADDR, MAIN_ADDR, (uint8_t *)&databuf, DATALEN / 2);
            err_sta = 2;
        }
        else if (crc1 == 0 && crc2 == 1) // 仅备份正常
        {
            memcpy(&databuf, &backdatabuf, DATALEN);
            T5lNorFlashRW(flashWRITE_FLAG, 0, MAIN_ADDR, BACK_ADDR, (uint8_t *)&backdatabuf, DATALEN / 2);
            err_sta = 3;
        }
        G_Device_Ctrl.Exhaust.speed = databuf[0];
        G_Device_Ctrl.Exhaust.interval_time_h = databuf[1];
        write_dgus_vp(0x5101, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1);           //排风
        write_dgus_vp(0x5102, (uint8_t *)&G_Device_Ctrl.Exhaust.interval_time_h, 1); //排风
        // write_dgus_vp(0x5328, (uint8_t *)&G_Device_Ctrl.Exhaust.speed, 1); //排风

        G_Device_Ctrl.UVB.brightness = databuf[2];
        G_Device_Ctrl.UVB.running_time_h = databuf[3];
        write_dgus_vp(0x510D, (uint8_t *)&G_Device_Ctrl.UVB.brightness, 1); // UVB
        write_dgus_vp(0x510E, (uint8_t *)&G_Device_Ctrl.UVB.running_time_h, 1);

        //雾化
        G_Device_Ctrl.Humidifier.interval_time_h = databuf[4];
        G_Device_Ctrl.Humidifier.running_time_h = databuf[5];
        write_dgus_vp(0x5111, (uint8_t *)&G_Device_Ctrl.Humidifier.interval_time_h, 1); //雾化
        write_dgus_vp(0x5112, (uint8_t *)&G_Device_Ctrl.Humidifier.running_time_h, 1);
        //灯光
        G_Device_Ctrl.Light.brightness = databuf[6];
        write_dgus_vp(0x5115, (uint8_t *)&G_Device_Ctrl.Light.brightness, 1); //灯光
        // write_dgus_vp(0x5320,(uint8_t*)&G_Device_Ctrl.Light.brightness,1);//更新灯光状态页图标状态

        //加热
        G_Device_Ctrl.Heater.set_temp = databuf[7];
        write_dgus_vp(0x5119, (uint8_t *)&G_Device_Ctrl.Heater.set_temp, 1); //加热

        // UVC
        G_Device_Ctrl.UVC.running_time_h = databuf[8];
        write_dgus_vp(0x5109, (uint8_t *)&G_Device_Ctrl.UVC.running_time_h, 1); // UVC

        //温度单位
        G_Device_Ctrl.Cfg.temp_uint = databuf[9];
        write_dgus_vp(0x5126, (uint8_t *)&G_Device_Ctrl.Cfg.temp_uint, 1); //温度单位

        if (G_Device_Ctrl.Cfg.temp_uint == 1)
        {
            tmp = (uint16_t)(45 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }
        else
        {
            tmp = (uint16_t)(6 << 8) | 0;
            write_dgus_vp(0x5800 + 7, (uint8_t *)&tmp, 1);
        }

        //屏保&息屏
        G_Device_Ctrl.Cfg.ligth_value = databuf[10];
        G_Device_Ctrl.Cfg.screen_save = databuf[11];
        G_Device_Ctrl.Cfg.off_display_time_min = databuf[12];
        write_dgus_vp(SCREEN_MIN_VP, (uint8_t *)&G_Device_Ctrl.Cfg.off_display_time_min, 1); //屏保&息屏
        write_dgus_vp(SCREEN_SAVESTA_VP, (uint8_t *)&G_Device_Ctrl.Cfg.screen_save, 1);
        //音量
        G_Device_Ctrl.Cfg.volume = databuf[13];
        write_dgus_vp(VOLUME_ADD_SUB_VP, (uint8_t *)&G_Device_Ctrl.Cfg.volume, 1); //屏保&息屏

        //进风
        G_Device_Ctrl.Inlet_Fan.speed = databuf[14];

        //密码
        memcpy(&G_Device_Ctrl.Cfg.passwd[0], &databuf[16], 4);

        //语言
        G_Device_Ctrl.Cfg.language = databuf[18];
        write_dgus_vp(0x535B, (uint8_t *)&G_Device_Ctrl.Cfg.language, 1);

        //滤芯提醒
        G_Device_Ctrl.Humidifier.filter_remind_month = databuf[19];
        G_Device_Ctrl.Humidifier.filter_need_replace = databuf[22];

        G_Device_Ctrl.TargetDate.year = databuf[20];
        G_Device_Ctrl.TargetDate.month = (uint8_t)(databuf[21] >> 8);
        G_Device_Ctrl.TargetDate.day = (uint8_t)(databuf[21] & 0xff);
        write_dgus_vp(0x5349, (uint8_t *)&G_Device_Ctrl.Humidifier.filter_remind_month, 1);
    }
}

void Start_Once_SaveData()
{
    G_Device_Ctrl.Cfg.save_flag = 1;
    G_Device_Ctrl.Cfg.save_interval_times = 4;
}

void Flash_SaveData()
{
    uint16_t databuf[32];

    if (G_Device_Ctrl.Cfg.save_flag != 1)
        return;
    if (G_Device_Ctrl.Cfg.save_interval_times != 0)
        return;
    G_Device_Ctrl.Cfg.save_flag = 0;
    memset(&databuf, 0, DATALEN);
    //排风
    databuf[0] = G_Device_Ctrl.Exhaust.speed;
    databuf[1] = G_Device_Ctrl.Exhaust.interval_time_h;

    // UVB
    databuf[2] = G_Device_Ctrl.UVB.brightness;
    databuf[3] = G_Device_Ctrl.UVB.running_time_h;

    //雾化
    databuf[4] = G_Device_Ctrl.Humidifier.interval_time_h;
    databuf[5] = G_Device_Ctrl.Humidifier.running_time_h;

    //灯光
    databuf[6] = G_Device_Ctrl.Light.brightness;

    //加热
    databuf[7] = G_Device_Ctrl.Heater.set_temp;

    // UVC
    databuf[8] = G_Device_Ctrl.UVC.running_time_h;

    //温度单位
    databuf[9] = G_Device_Ctrl.Cfg.temp_uint;

    //屏保&息屏
    databuf[10] = (uint16_t)G_Device_Ctrl.Cfg.ligth_value;
    databuf[11] = G_Device_Ctrl.Cfg.screen_save;
    databuf[12] = G_Device_Ctrl.Cfg.off_display_time_min;

    //音量
    databuf[13] = G_Device_Ctrl.Cfg.volume;

    //进风
    databuf[14] = G_Device_Ctrl.Inlet_Fan.speed;

    //密码
    memcpy(&databuf[16], &G_Device_Ctrl.Cfg.passwd[0], 4);

    //语言
    databuf[18] = G_Device_Ctrl.Cfg.language;

    //滤芯提醒
    databuf[19] = G_Device_Ctrl.Humidifier.filter_remind_month;
    databuf[20] = G_Device_Ctrl.TargetDate.year;
    databuf[21] = ((uint16_t)G_Device_Ctrl.TargetDate.month << 8) | G_Device_Ctrl.TargetDate.day;
    databuf[22] = G_Device_Ctrl.Humidifier.filter_need_replace; //滤芯是否需要更换
    Calculate_CRC16_Flash((uint8_t *)&databuf, DATALEN - 2, 1);
    T5lNorFlashRW(flashWRITE_FLAG, 0, BACK_ADDR, MAIN_ADDR, (uint8_t *)&databuf, DATALEN / 2);
    T5lNorFlashRW(flashWRITE_FLAG, 0, MAIN_ADDR, BACK_ADDR, (uint8_t *)&databuf, DATALEN / 2);
}

void Updata_Vpdata_To_Report()
{
    uint16_t databuf[64];
    databuf[0] = G_Device_Ctrl.Cfg.temp_uint;
    databuf[1] = (((uint16_t)(G_Device_Ctrl.Cfg.ligth_value - 20) * 125) / 100);
    databuf[2] = G_Device_Ctrl.Cfg.off_display_time_min;
    databuf[3] = G_Device_Ctrl.Cfg.screen_save;
    databuf[4] = G_Device_Ctrl.Cfg.volume;
    databuf[5] = G_Device_Ctrl.Cfg.ligth_value;
    write_dgus_vp(0x3000, (uint8_t *)&databuf[0], 6);

    *(float *)&databuf[0] = ((float)G_Device_Ctrl.environment.temperaturex10 / 10);
    databuf[2] = G_Device_Ctrl.environment.humidity;
    databuf[3] = 0;
    databuf[4] = 0;
    databuf[5] = 0;
    databuf[6] = 0;
    databuf[7] = 0;

    databuf[8] = G_Device_Ctrl.Lock.enable;
    databuf[9] = G_Device_Ctrl.Lock.enable;

    databuf[10] = G_Device_Ctrl.Exhaust.enable;
    databuf[11] = G_Device_Ctrl.Exhaust.speed;
    databuf[12] = G_Device_Ctrl.Exhaust.interval_time_h;
    databuf[13] = G_Device_Ctrl.Exhaust.running;
    databuf[14] = 0;

    databuf[15] = G_Device_Ctrl.Light.enable;
    databuf[16] = G_Device_Ctrl.Light.brightness;
    databuf[17] = G_Device_Ctrl.Light.running;
    databuf[18] = 0;
    databuf[19] = 0;

    databuf[20] = G_Device_Ctrl.UVB.enable;
    databuf[21] = G_Device_Ctrl.UVB.brightness;
    databuf[22] = G_Device_Ctrl.UVB.running_time_h;
    databuf[23] = G_Device_Ctrl.UVB.running;
    databuf[24] = 0;

    databuf[25] = G_Device_Ctrl.Anion.enable;
    databuf[26] = G_Device_Ctrl.Anion.running;
    databuf[27] = 0;
    databuf[28] = 0;
    databuf[29] = 0;

    databuf[30] = G_Device_Ctrl.Plasma.enable;
    databuf[31] = G_Device_Ctrl.Plasma.running;
    databuf[32] = 0;
    databuf[33] = 0;
    databuf[34] = 0;

    databuf[35] = G_Device_Ctrl.Heater.enable;
    databuf[36] = G_Device_Ctrl.Heater.set_temp;
    if (G_Device_Ctrl.Heater.enable == 0)
    {
        databuf[37] = 0;
    }
    else if ((G_Device_Ctrl.Heater.running != 0) ||
             (G_Device_Ctrl.Heater.run_status == 2))
    {
        databuf[37] = 1;
    }
    else
    {
        databuf[37] = 2;
    }
    databuf[38] = G_Device_Ctrl.Heater.running;
    databuf[39] = 0;

    databuf[40] = G_Device_Ctrl.Humidifier.enable;
    databuf[41] = G_Device_Ctrl.Humidifier.interval_time_h;
    databuf[42] = G_Device_Ctrl.Humidifier.running_time_h;
    databuf[43] = G_Device_Ctrl.Humidifier.running;
    databuf[44] = G_Device_Ctrl.Humidifier.liquid_status;

    databuf[45] = G_Device_Ctrl.Inlet_Fan.enable;
    databuf[46] = G_Device_Ctrl.Inlet_Fan.speed;
    databuf[47] = G_Device_Ctrl.Inlet_Fan.running;
    databuf[48] = 0;
    databuf[49] = 0;

    databuf[50] = G_Device_Ctrl.Humidifier.filter_life_percense;
    databuf[51] = G_Device_Ctrl.Humidifier.filter_need_replace;
    databuf[52] = 0;
    databuf[53] = 0;
    databuf[54] = 0;

    write_dgus_vp(0x3010, (uint8_t *)&databuf[0], 55);
}

// 判断是否为闰年
bit isLeapYear(unsigned short year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

// 获取某年某月的天数
unsigned char getDaysInMonth(unsigned int year, unsigned char month)
{
    unsigned char days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    return days[month - 1];
}

// 计算当前日期加上指定月数后的日期
TDate addMonths(TDate t, unsigned char monthsToAdd)
{
    unsigned char maxDay ;
    unsigned char totalMonths = t.month - 1 + monthsToAdd;
    unsigned short yearsToAdd = totalMonths / 12;
    unsigned char newMonth = (totalMonths % 12) + 1;

    t.year += yearsToAdd;
    t.month = newMonth;

    // 获取目标月份的天数
    maxDay = getDaysInMonth(t.year, t.month);
    if (t.day > maxDay)
    {
        t.day = maxDay;
    }

    return t;
}

void set_filter_month()
{
    uint8_t u8buf[8];
    TDate currentDate;
    read_dgus_vp(0x10, (uint8_t *)&u8buf[0], 2);
    currentDate.year = (uint32_t)u8buf[0] + 2000;
    currentDate.month = u8buf[1];
    currentDate.day = u8buf[2];
    G_Device_Ctrl.TargetDate = addMonths(currentDate, G_Device_Ctrl.Humidifier.filter_remind_month);

    // Start_Once_SaveData();
}

void Check_Filter_Month()
{
    uint8_t u8buf[8];
    TDate currentDate;
    read_dgus_vp(0x10, (uint8_t *)&u8buf[0], 2);
    currentDate.year = (uint32_t)u8buf[0] + 2000;
    currentDate.month = u8buf[1];
    currentDate.day = u8buf[2];
    if (currentDate.year == G_Device_Ctrl.TargetDate.year)
    {
        if (currentDate.month == G_Device_Ctrl.TargetDate.month)
        {
            if (currentDate.day == G_Device_Ctrl.TargetDate.day)
            {
                if(G_Device_Ctrl.Humidifier.filter_need_replace == 0)
                {
                    G_Device_Ctrl.Humidifier.filter_need_replace = 1;
                    Start_Once_SaveData();

                }
            }
        }
    }
}

void Mult_Task(){
    Check_Filter_Month();
    Updata_Vpdata_To_Report();
}
