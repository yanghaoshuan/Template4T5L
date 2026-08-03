#ifndef PB03F_BLE_H
#define PB03F_BLE_H

#include "sys.h"
#include "uart.h"

#if pb03fBLE_ENABLED

#define PB03F_BLE_TASK_INTERVAL                 1U
#define PB03F_BLE_MTU                           240U
#define PB03F_BLE_ATT_PAYLOAD_MAX               (PB03F_BLE_MTU - 3U)
#define PB03F_BLE_FRAME_MAGIC_HIGH              0x4DU
#define PB03F_BLE_FRAME_MAGIC_LOW               0x51U
#define PB03F_BLE_FRAME_VERSION                 0x01U
#define PB03F_BLE_FRAME_FLAG_LAST               0x01U
#define PB03F_BLE_FRAME_HEADER_SIZE             12U
#define PB03F_BLE_FRAME_CRC_SIZE                2U
#define PB03F_BLE_CHUNK_PAYLOAD_MAX             (PB03F_BLE_ATT_PAYLOAD_MAX - PB03F_BLE_FRAME_HEADER_SIZE - PB03F_BLE_FRAME_CRC_SIZE)
#define PB03F_BLE_MAX_CHUNKS                    ((2000U + PB03F_BLE_CHUNK_PAYLOAD_MAX - 1U) / PB03F_BLE_CHUNK_PAYLOAD_MAX)
#define PB03F_BLE_REASSEMBLY_TIMEOUT_MS         5000UL

#if blePB03F_UART_ID == 2
    #define PB03F_BLE_UART                       Uart2
    #define PB03F_BLE_UART_ENABLED               uartUART2_ENABLED
    #define PB03F_BLE_UART_TXBUF_SIZE            uartUART2_TXBUF_SIZE
    #define PB03F_BLE_UART_RXBUF_SIZE            uartUART2_RXBUF_SIZE
#elif blePB03F_UART_ID == 5
    #define PB03F_BLE_UART                       Uart5
    #define PB03F_BLE_UART_ENABLED               uartUART5_ENABLED
    #define PB03F_BLE_UART_TXBUF_SIZE            uartUART5_TXBUF_SIZE
    #define PB03F_BLE_UART_RXBUF_SIZE            uartUART5_RXBUF_SIZE
#else
    #error "PB-03F UART must be UART2 or UART5."
#endif

#if !uartUART4_ENABLED || !PB03F_BLE_UART_ENABLED
#error "PB-03F/V851 protocol requires UART4 and the selected PB-03F UART."
#endif

#if PB03F_BLE_UART_TXBUF_SIZE < (PB03F_BLE_ATT_PAYLOAD_MAX + 1U)
#error "The selected PB-03F UART TX buffer is too small for one BLE frame."
#endif

#if PB03F_BLE_UART_RXBUF_SIZE < PB03F_BLE_ATT_PAYLOAD_MAX
#error "The selected PB-03F UART RX buffer is too small for one BLE frame."
#endif

#if uartUART_COMMON_FRAME_SIZE < PB03F_BLE_UART_RXBUF_SIZE
#error "The common UART scratch buffer must hold the PB-03F receive ring."
#endif

void Pb03fBleInit(void);
void Pb03fBleTask(void);
void Pb03fBleReceive(const uint8_t *_data, uint16_t len);
void Pb03fBleFrameError(uint16_t msg_id, const char *message);
uint8_t Pb03fBleIsTransparent(void);
uint8_t Pb03fBleSendJson(const uint8_t *_data, uint16_t len);
uint8_t Pb03fBleSetProvisionedIdentity(const uint8_t *ble_id, uint16_t len);
const char *Pb03fBleGetMac(void);

#endif /* pb03fBLE_ENABLED */

#endif /* PB03F_BLE_H */
