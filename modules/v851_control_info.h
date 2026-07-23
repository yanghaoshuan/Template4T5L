#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if bleV851_BRIDGE_ENABLED

#define V851_CONTROL_PARAMS_SNAPSHOT_MAX        256U  /**< 单类控制参数快照上限 */
#define V851_CONTROL_DGUS_SLOT_WORDS             0x0010U
#define V851_CONTROL_DGUS_SLOT_BYTES             (V851_CONTROL_DGUS_SLOT_WORDS * 2U)
#define V851_CONTROL_DGUS_TEXT_BYTES             8U
#define V851_CONTROL_DGUS_EXHAUST_ADDR           0x3000UL
#define V851_CONTROL_DGUS_CLIMATE_ADDR           0x3010UL
#define V851_CONTROL_DGUS_LIGHT_ADDR             0x3020UL
#define V851_CONTROL_DGUS_SETTINGS_ADDR          0x3030UL
#define V851_CONTROL_DGUS_HUMIDIFIER_ADDR        0x3040UL

typedef struct
{
    V851ControlType type;
    uint8_t valid;
    uint8_t updated;
    uint8_t params_truncated;
    uint8_t params_redacted;
    uint32_t revision;
    uint32_t received_tick;
    char command_id[V851_COMMAND_ID_MAX + 1U];
    char server_msg_id[V851_COMMAND_ID_MAX + 1U];
    char cmd[V851_COMMAND_NAME_MAX + 1U];
    uint32_t expire_at;
    uint16_t params_len;
    uint16_t stored_len;
    uint8_t params_json[V851_CONTROL_PARAMS_SNAPSHOT_MAX + 1U];
} V851ControlDetails;

/**
 * @brief 初始化全部控制信息快照。
 */
void V851ControlInfoInit(void);

/**
 * @brief 由V851协议层保存一条已接受的控制命令。
 */
uint8_t V851ControlInfoUpdate(const V851ControlCommand *command);

/**
 * @brief 查询是否存在任意尚未确认读取的控制更新。
 */
uint8_t V851ControlInfoAnyUpdated(void);

/**
 * @brief 查询指定控制类型是否存在尚未确认读取的更新。
 */
uint8_t V851ControlInfoIsUpdated(V851ControlType type);

/**
 * @brief 获取指定控制类型的最近一次快照，调用本函数不会清除更新标志。
 *
 * @note 返回的指针由模块持有，在同类型下一次更新前保持有效。
 * 若params_truncated非0，params_json仅包含前stored_len字节。
 * 密码控制的params_redacted为1，params_json不保存明文。
 */
const V851ControlDetails *V851ControlInfoGet(V851ControlType type);

/**
 * @brief 显式确认指定控制类型的更新已经读取。
 */
uint8_t V851ControlInfoClearUpdated(V851ControlType type);

/**
 * @brief 校验控制参数并同步写入对应DGUS控制槽。
 *
 * @note 该入口需要由协议层直接调用，使Keil C51链接器能够正确分析
 *       write_dgus_vp调用链，避免函数指针间接调用造成XDATA overlay冲突。
 */
void V851ControlInfoDgusHandler(V851ControlHandlerContext *context);

/**
 * @brief 注册写入DGUS变量空间的控制处理器。
 *
 * @details 固定地址映射如下：
 * - 0x3000：排风，+0 enabled，+1 level
 * - 0x3010：温控，+0 enabled，+1 target_temperature，+2至+5 mode
 * - 0x3020：灯光，+0 enabled，+1 brightness，+2 color_temperature
 * - 0x3030：设备设置，+0 volume，+1 screen_brightness，+2至+5 language
 * - 0x3040：加湿，+0 enabled，+1 target_humidity
 *
 * @return 五类控制处理器全部注册成功返回1，否则返回0。
 * @note 每个控制槽占0x10个VP，写入时先整体清零。
 */
uint8_t V851ControlInfoDgusInit(void);

#endif /* bleV851_BRIDGE_ENABLED */

#endif /* V851_CONTROL_INFO_H */
