
#ifndef SYS_H
#define SYS_H

#include "T5LOSConfig.h"

typedef struct TaskDefine
{
    uint8_t taskID;
    uint16_t taskInterval;
    uint16_t taskCounter;
    void (*taskFunction)(void);
} SysTask;
static SysTask SysTasks[sysMAX_TASK_NUM];
static uint8_t SysTaskCount = 0;


#define COUNT_TASK_INTERVAL 1000
void CountTask(void);
void TaskAdd(uint8_t taskID, uint16_t taskInterval, void (*taskFunction)(void));
void TaskRemove(uint8_t taskID);
void TaskRun(void);

void delay_us(const uint16_t us);
void delay_ms(const uint16_t ms);


uint16_t crc_16(uint8_t *pBuf, uint16_t buf_len);


void read_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);


void write_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t len);


void T5LCpuInit(void);

#endif /* SYS_H */