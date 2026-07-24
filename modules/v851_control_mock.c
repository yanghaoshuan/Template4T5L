#include "v851_control_mock.h"

#if bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED

#include "bridge_json.h"
#include "timer.h"

#include <string.h>

#define V851_CONTROL_MOCK_JSON_MAX               512U
#define V851_CONTROL_MOCK_START_DELAY_MS          1000UL

static uint8_t xdata v851_control_mock_json[V851_CONTROL_MOCK_JSON_MAX];
static uint32_t v851_control_mock_sequence;
static V851ControlMockCase v851_control_mock_task_case;
static uint32_t v851_control_mock_task_tick;
static uint8_t v851_control_mock_task_started;

static const char *V851ControlMockCommand(V851ControlMockCase mock_case)
{
    switch(mock_case)
    {
        case V851_CONTROL_MOCK_EXHAUST:
            return "exhaust.set";

        case V851_CONTROL_MOCK_CLIMATE:
            return "climate.set";

        case V851_CONTROL_MOCK_LIGHT:
            return "light.set";

        case V851_CONTROL_MOCK_DEVICE_SETTINGS:
            return "device_settings.set";

        default:
            return NULL;
    }
}

static const char *V851ControlMockParams(V851ControlMockCase mock_case)
{
    switch(mock_case)
    {
        case V851_CONTROL_MOCK_EXHAUST:
            return "{\"enabled\":true,\"level\":3}";

        case V851_CONTROL_MOCK_CLIMATE:
            return "{\"enabled\":true,\"mode\":\"AUTO\",\"target_temperature\":24}";

        case V851_CONTROL_MOCK_LIGHT:
            return "{\"enabled\":true,\"brightness\":80,\"color_temperature\":4500}";

        case V851_CONTROL_MOCK_DEVICE_SETTINGS:
            return "{\"language\":\"zh-CN\",\"volume\":60,\"screen_brightness\":70}";

        default:
            return NULL;
    }
}

static V851ControlType V851ControlMockType(V851ControlMockCase mock_case)
{
    switch(mock_case)
    {
        case V851_CONTROL_MOCK_EXHAUST:
            return V851_CONTROL_EXHAUST;

        case V851_CONTROL_MOCK_CLIMATE:
            return V851_CONTROL_CLIMATE;

        case V851_CONTROL_MOCK_LIGHT:
            return V851_CONTROL_LIGHT;

        case V851_CONTROL_MOCK_DEVICE_SETTINGS:
            return V851_CONTROL_DEVICE_SETTINGS;

        default:
            return V851_CONTROL_INVALID;
    }
}

uint8_t V851ControlMockInject(V851ControlMockCase mock_case)
{
    BridgeJsonWriter writer;
    const char *command;
    const char *params;
    V851ControlType type;
    uint16_t json_len;

    command = V851ControlMockCommand(mock_case);
    params = V851ControlMockParams(mock_case);
    type = V851ControlMockType(mock_case);
    if((command == NULL) || (params == NULL) ||
       (type == V851_CONTROL_INVALID))
    {
        return 0U;
    }

    v851_control_mock_sequence++;
    if(v851_control_mock_sequence == 0UL)
    {
        v851_control_mock_sequence = 1UL;
    }

    BridgeJsonWriterInit(&writer, v851_control_mock_json,
                         V851_CONTROL_MOCK_JSON_MAX);
    BridgeJsonWriterText(&writer, "{\"msg_id\":\"MOCK-");
    BridgeJsonWriterUint32(&writer, v851_control_mock_sequence);
    BridgeJsonWriterText(&writer,
                         "\",\"msg_type\":\"server.command\",\"data\":{"
                         "\"command_id\":\"MOCK-CMD-");
    BridgeJsonWriterUint32(&writer, v851_control_mock_sequence);
    BridgeJsonWriterText(&writer, "\",\"cmd\":");
    BridgeJsonWriterQuoted(&writer, command, (uint16_t)strlen(command));
    BridgeJsonWriterText(&writer, ",\"expire_at\":0,\"params\":");
    BridgeJsonWriterRaw(&writer, (const uint8_t *)params,
                        (uint16_t)strlen(params));
    BridgeJsonWriterText(&writer, "}}");
    json_len = BridgeJsonWriterFinish(&writer);
    if(json_len == 0U)
    {
        return 0U;
    }

    (void)V851ControlInfoClearUpdated(type);
    V851ProtocolReceiveJson(v851_control_mock_json, json_len);
    return V851ControlInfoIsUpdated(type);
}

uint8_t V851ControlMockInjectAll(void)
{
    V851ControlMockCase mock_case;

    for(mock_case = V851_CONTROL_MOCK_EXHAUST;
        mock_case < V851_CONTROL_MOCK_COUNT;
        mock_case++)
    {
        if(V851ControlMockInject(mock_case) == 0U)
        {
            return 0U;
        }
    }
    v851_control_mock_task_case = V851_CONTROL_MOCK_COUNT;
    return 1U;
}

void V851ControlMockTask(void)
{
    if(v851_control_mock_task_case >= V851_CONTROL_MOCK_COUNT)
    {
        return;
    }

    if(v851_control_mock_task_started == 0U)
    {
        v851_control_mock_task_tick = GetSysTick();
        v851_control_mock_task_started = 1U;
        return;
    }

    if((uint32_t)(GetSysTick() - v851_control_mock_task_tick) <
       V851_CONTROL_MOCK_START_DELAY_MS)
    {
        return;
    }

    if(V851ControlMockInject(v851_control_mock_task_case) != 0U)
    {
        v851_control_mock_task_case++;
    }
}

#endif /* bleV851_BRIDGE_ENABLED && v851CONTROL_MOCK_ENABLED */
