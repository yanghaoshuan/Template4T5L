#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

#include "sys.h"

#define BOOT_HANDOFF_FILE_INFO_MAX       280U

uint8_t BootHandoffIsUpgradeFrame(const uint8_t *frame, uint16_t len);
void BootHandoffRequestUpgrade(void);

#endif /* BOOT_HANDOFF_H */
