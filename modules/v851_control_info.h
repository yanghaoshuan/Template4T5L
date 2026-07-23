#ifndef V851_CONTROL_INFO_H
#define V851_CONTROL_INFO_H

#include "v851_protocol.h"

#if bleV851_BRIDGE_ENABLED

#define V851_CONTROL_PARAMS_SNAPSHOT_MAX        256U  /**< 单类控制参数快照上限 */

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

#endif /* bleV851_BRIDGE_ENABLED */

#endif /* V851_CONTROL_INFO_H */
