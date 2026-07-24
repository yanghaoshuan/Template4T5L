#ifndef V851_CONTROL_MOCK_H
#define V851_CONTROL_MOCK_H

#include "v851_control_info.h"

#if bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED

#define V851_CONTROL_MOCK_TASK_INTERVAL          50U

typedef enum
{
    V851_CONTROL_MOCK_EXHAUST = 0,
    V851_CONTROL_MOCK_CLIMATE,
    V851_CONTROL_MOCK_LIGHT,
    V851_CONTROL_MOCK_DEVICE_SETTINGS,
    V851_CONTROL_MOCK_COUNT
} V851ControlMockCase;

/**
 * @brief 构造并通过统一JSON入口注入一条控制命令。
 */
uint8_t V851ControlMockInject(V851ControlMockCase mock_case);

/**
 * @brief 依次注入全部内置控制命令Mock。
 */
uint8_t V851ControlMockInjectAll(void);

/**
 * @brief 系统启动后延迟并逐条注入内置控制命令。
 *
 * @details 避免在main初始化阶段同步写DGUS，也避免连续应答占满V851发送队列。
 */
void V851ControlMockTask(void);

#endif /* bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED */

#endif /* V851_CONTROL_MOCK_H */
