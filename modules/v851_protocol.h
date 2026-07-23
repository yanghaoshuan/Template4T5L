#ifndef V851_PROTOCOL_H
#define V851_PROTOCOL_H

#include "sys.h"
#include "uart.h"
#include "timer.h"

#if bleV851_BRIDGE_ENABLED

#define V851_PROTOCOL_TASK_INTERVAL              1U
#define V851_DEVICE_SN_MAX                       64U
#define V851_PRODUCT_KEY_MAX                     32U
#define V851_BLE_ID_MAX                          6U
#define V851_SHORT_TEXT_MAX                      32U
#define V851_COMMAND_ID_MAX                      64U
#define V851_COMMAND_NAME_MAX                    32U

#if uartUART4_TXBUF_SIZE < 2008U
#error "UART4 TX buffer is too small for a maximum AA55 JSON frame."
#endif

#if otaOTA_ENABLED
    #if uartUART4_RXBUF_SIZE < 4128U
    #error "UART4 RX buffer is too small for a maximum AB CD OTA frame."
    #endif
#else
    #if uartUART4_RXBUF_SIZE < 2008U
    #error "UART4 RX buffer is too small for a maximum AA55 JSON frame."
    #endif
#endif

#if uartUART_COMMON_FRAME_SIZE < uartUART4_RXBUF_SIZE
#error "The common UART scratch buffer must hold the UART4 receive ring."
#endif

typedef struct
{
    char device_sn[V851_DEVICE_SN_MAX + 1U];
    char ble_id[V851_BLE_ID_MAX + 1U];
    char ble_name[V851_SHORT_TEXT_MAX + 1U];
    char product_key[V851_PRODUCT_KEY_MAX + 1U];
    char model[V851_SHORT_TEXT_MAX + 1U];
    char hardware_version[V851_SHORT_TEXT_MAX + 1U];
    char firmware_version[V851_SHORT_TEXT_MAX + 1U];
    char sales_country_code[4];
    char bind_status[16];
} V851DeviceInfo;

typedef enum
{
    V851_CONTROL_EXHAUST = 0,
    V851_CONTROL_PLASMA,
    V851_CONTROL_ANION,
    V851_CONTROL_CLIMATE,
    V851_CONTROL_INLET_FAN,
    V851_CONTROL_HUMIDIFIER,
    V851_CONTROL_UVB,
    V851_CONTROL_LIGHT,
    V851_CONTROL_DEVICE_SETTINGS,
    V851_CONTROL_DEVICE_PASSWORD,
    V851_CONTROL_OTA,
    V851_CONTROL_COUNT,
    V851_CONTROL_INVALID = 0xFF
} V851ControlType;

typedef enum
{
    V851_CONTROL_STATUS_SUCCESS = 0,
    V851_CONTROL_STATUS_FAILED,
    V851_CONTROL_STATUS_BUSY,
    V851_CONTROL_STATUS_UNSUPPORTED
} V851ControlStatus;

typedef struct
{
    V851ControlType type;
    const char *command_id;
    uint16_t command_id_len;
    const char *server_msg_id;
    uint16_t server_msg_id_len;
    const char *cmd;
    uint16_t cmd_len;
    const uint8_t *params_json;
    uint16_t params_len;
    uint32_t expire_at;
} V851ControlCommand;

typedef struct
{
    V851ControlStatus status;
    const char *error_code;
    const char *error_message;
    const uint8_t *applied_json;
    uint16_t applied_len;
} V851ControlResult;

typedef struct
{
    const V851ControlCommand *command;
    V851ControlResult *result;
} V851ControlHandlerContext;

typedef void (*V851ControlHandler)(V851ControlHandlerContext *context);

typedef enum
{
    V851_OTA_STAGE_INSTALLING = 0,
    V851_OTA_STAGE_VERIFYING,
    V851_OTA_STAGE_REBOOTING,
    V851_OTA_STAGE_SUCCESS,
    V851_OTA_STAGE_FAILED
} V851OtaStage;

void V851ProtocolInit(void);
void V851ProtocolTask(void);
void V851ProtocolReceive(UART_TYPE *uart, const uint8_t *_data, uint16_t len);
uint8_t V851ProtocolSendJson(const uint8_t *_data, uint16_t len);
uint8_t V851ProtocolSendOtaFrame(const uint8_t *_data, uint16_t len);
uint8_t V851ProtocolRegisterControlHandler(V851ControlType type,
                                           V851ControlHandler handler);
const V851DeviceInfo *V851ProtocolGetDeviceInfo(void);
uint32_t V851ProtocolGetTimestamp(void);
void V851ProtocolNotifyOtaState(V851OtaStage stage,
                                uint8_t progress,
                                const char *error_code);

#endif /* bleV851_BRIDGE_ENABLED */

#endif /* V851_PROTOCOL_H */
