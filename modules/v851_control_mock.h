#ifndef V851_CONTROL_MOCK_H
#define V851_CONTROL_MOCK_H

#include "v851_control_info.h"

#if bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED

typedef enum
{
    V851_CONTROL_MOCK_EXHAUST = 0,
    V851_CONTROL_MOCK_CLIMATE,
    V851_CONTROL_MOCK_LIGHT,
    V851_CONTROL_MOCK_DEVICE_SETTINGS,
    V851_CONTROL_MOCK_COUNT
} V851ControlMockCase;

/**
 * @brief 构造并注入一条带AA55帧头、长度和CRC的控制命令。
 */
uint8_t V851ControlMockInject(V851ControlMockCase mock_case);

/**
 * @brief 依次注入全部内置控制命令Mock。
 */
uint8_t V851ControlMockInjectAll(void);

#endif /* bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED */

#endif /* V851_CONTROL_MOCK_H */
