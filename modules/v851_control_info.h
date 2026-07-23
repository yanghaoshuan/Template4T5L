#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if bleV851_BRIDGE_ENABLED

#define V851_CONTROL_PARAMS_SNAPSHOT_MAX        256U  /**< 单类控制参数快照上限 */
#define V851_CONTROL_DGUS_TASK_INTERVAL          10U
#define V851_CONTROL_DGUS_BASE_ADDR              0x3000UL
#define V851_CONTROL_DGUS_ADDR_STRIDE            0x0010UL
#define V851_CONTROL_DGUS_RECORD_BYTES           16U
#define V851_CONTROL_DGUS_RECORD_WORDS           (V851_CONTROL_DGUS_RECORD_BYTES / 2U)

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
 * @brief 将指定的控制信息更新同步到DGUS变量空间。
 *
 * @details 固定地址映射如下：
 * - 0x3000：排风控制
 * - 0x3010：温控
 * - 0x3020：灯光
 * - 0x3030：设备设置
 *
 * 每条记录固定16字节：
 * byte0为控制类型，byte1为状态标志，byte2-3为原始参数长度，
 * byte4-7为版本号，byte8-11为接收时刻，byte12-13为已保存参数长度，
 * byte14-15为已保存参数的CRC16。多字节数值使用大端顺序。
 */
void V851ControlInfoDgusTask(void);

#endif /* bleV851_BRIDGE_ENABLED */

#endif /* V851_CONTROL_INFO_H */
