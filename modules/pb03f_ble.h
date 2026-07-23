#ifndef PB03F_BLE_H
#define PB03F_BLE_H

#include "sys.h"
#include "uart.h"

#if bleV851_BRIDGE_ENABLED

#define PB03F_BLE_TASK_INTERVAL                 1U
#define PB03F_BLE_MTU                           240U
#define PB03F_BLE_ATT_PAYLOAD_MAX               (PB03F_BLE_MTU - 3U)
#define PB03F_BLE_FRAME_HEADER_SIZE             12U
#define PB03F_BLE_FRAME_CRC_SIZE                2U
#define PB03F_BLE_CHUNK_PAYLOAD_MAX             (PB03F_BLE_ATT_PAYLOAD_MAX - PB03F_BLE_FRAME_HEADER_SIZE - PB03F_BLE_FRAME_CRC_SIZE)
#define PB03F_BLE_MAX_CHUNKS                    ((2000U + PB03F_BLE_CHUNK_PAYLOAD_MAX - 1U) / PB03F_BLE_CHUNK_PAYLOAD_MAX)
#define PB03F_BLE_REASSEMBLY_TIMEOUT_MS         5000UL

#if !uartUART4_ENABLED || !uartUART5_ENABLED
#error "PB-03F/V851 protocol requires UART4 and UART5."
#endif

#if uartUART5_TXBUF_SIZE < (PB03F_BLE_ATT_PAYLOAD_MAX + 1U)
#error "UART5 TX buffer is too small for one PB-03F BLE frame."
#endif

#if uartUART_COMMON_FRAME_SIZE < uartUART5_RXBUF_SIZE
#error "The common UART scratch buffer must hold the UART5 receive ring."
#endif

void Pb03fBleInit(void);
void Pb03fBleTask(void);
void Pb03fBleReceive(UART_TYPE *uart, const uint8_t *_data, uint16_t len);
uint8_t Pb03fBleSendJson(const uint8_t *_data, uint16_t len);
uint8_t Pb03fBleSetProvisionedIdentity(const uint8_t *ble_id, uint16_t len);
const char *Pb03fBleGetMac(void);

#endif /* bleV851_BRIDGE_ENABLED */

#endif /* PB03F_BLE_H */
