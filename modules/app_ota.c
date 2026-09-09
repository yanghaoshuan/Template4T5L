#include "app_ota.h"

/* Only a received upgrade command triggers reset; the BOOT load word may remain
 * at 0x0020 after startup and must not cause an application reset loop. */
void AppOtaControl(UART_TYPE *uart, uint8_t *frame, uint16_t length)
{
#if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    uint8_t control[4];
    uint8_t reset[4] = {0x55, 0xAA, 0x5A, 0xA5};
    if(uart != &Uart_R11 || length != 10 || frame[0] != 0x5A || frame[1] != 0xA5 ||
       frame[2] != 7 || frame[3] != 0x82 || frame[4] != 0 || frame[5] != 0x20 ||
       frame[6] != 0x5A || frame[7] != 0xA5 || frame[8] != 0x5A || frame[9] != 0xA5) return;
    write_dgus_vp(0x0020, frame + 6, 2);
    DgusToFlash(flashMAIN_BLOCK_ORDER, 0x0020, 0x0020, 2);
    FlashToDgusWithData(flashMAIN_BLOCK_ORDER, 0x0020, 0x0020, control, 2);
    if(control[0] != 0x5A || control[1] != 0xA5 || control[2] != 0x5A || control[3] != 0xA5) return;
    write_dgus_vp(0x0004, reset, 2);
#endif
}
