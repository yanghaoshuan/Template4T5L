#ifndef SLOVAK_IME_H
#define SLOVAK_IME_H

#include "sys.h"

/*
 * 斯洛伐克语输入法的周期任务配置。
 * 任务 ID 1 在当前工程中为空闲，20 ms 周期用于轮询 DGUS 事件 VP。
 */
#define SLOVAK_IME_TASK_ID              1U
#define SLOVAK_IME_TASK_INTERVAL        20U

/* DGUS 与 T5L OS 之间的 VP 接口；所有文本均按 UTF-16BE 传输。 */
#define SLOVAK_IME_KEY_VP               0x0700U
#define SLOVAK_IME_LAUNCH_VP            0x0710U
#define SLOVAK_IME_CANDIDATE1_VP        0x0730U
#define SLOVAK_IME_CANDIDATE2_VP        0x0740U
#define SLOVAK_IME_CANDIDATE3_VP        0x0750U
#define SLOVAK_IME_CANDIDATE4_VP        0x0760U
#define SLOVAK_IME_STATUS_VP            0x0770U
#define SLOVAK_IME_BUFFER_VP            0x0780U

/* 页面号、正文容量和同时显示的候选词数量。 */
#define SLOVAK_IME_KEYBOARD_PAGE        11U
#define SLOVAK_IME_MAX_LENGTH           63U
#define SLOVAK_IME_CANDIDATE_COUNT      4U

/** 初始化运行状态，并清空本模块占用的事件、候选词和显示 VP。 */
void SlovakImeInit(void);

/** 轮询启动/按键事件；应由系统调度器每 20 ms 调用一次。 */
void SlovakImeTask(void);

#endif /* SLOVAK_IME_H */
