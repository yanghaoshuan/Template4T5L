#ifndef SLOVAK_IME_H
#define SLOVAK_IME_H

#include "sys.h"

#define SLOVAK_IME_TASK_ID              1U
#define SLOVAK_IME_TASK_INTERVAL        20U

#define SLOVAK_IME_KEY_VP               0x0700U
#define SLOVAK_IME_LAUNCH_VP            0x0710U
#define SLOVAK_IME_CANDIDATE1_VP        0x0730U
#define SLOVAK_IME_CANDIDATE2_VP        0x0740U
#define SLOVAK_IME_CANDIDATE3_VP        0x0750U
#define SLOVAK_IME_CANDIDATE4_VP        0x0760U
#define SLOVAK_IME_STATUS_VP            0x0770U
#define SLOVAK_IME_BUFFER_VP            0x0780U

#define SLOVAK_IME_KEYBOARD_PAGE        11U
#define SLOVAK_IME_MAX_LENGTH           63U
#define SLOVAK_IME_CANDIDATE_COUNT      4U

void SlovakImeInit(void);
void SlovakImeTask(void);

#endif /* SLOVAK_IME_H */
