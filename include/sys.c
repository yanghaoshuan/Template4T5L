/**
 * @file sys.c
 * @brief T5L系统核心功能实现文件
 * @details 实现系统任务管理、延时、CRC计算、DGUS通信等核心功能
 * @author yangming
 * @version 1.0.0
 */

#include "sys.h"
#include "uart.h"
#include "gpio.h"
#include "timer.h"

/**
 * @brief 系统任务定时器时钟节拍计数器
 * @details 用于任务调度的时钟基准，由定时器中断递增
 */
extern volatile uint16_t SysTaskTimerTick;


void SysTaskAdd(uint8_t taskID, uint16_t taskInterval, void (*taskFunction)(void))
{
    if(SysTaskCount < sysMAX_TASK_NUM)
    {
        SysTasks[SysTaskCount].taskID = taskID;
        SysTasks[SysTaskCount].taskInterval = taskInterval;
        SysTasks[SysTaskCount].taskCounter = 0;
        SysTasks[SysTaskCount].taskFunction = taskFunction;
        SysTaskCount++;
    }
}


void SysTaskRemove(uint8_t taskID)
{
    uint8_t i, j;
    for(i = 0; i < SysTaskCount; i++)
    {
        if(SysTasks[i].taskID == taskID)
        {
            for(j = i; j < SysTaskCount - 1; j++)
            {
                SysTasks[j] = SysTasks[j + 1];
            }
            SysTaskCount--;
            return;
        }
    }
}

/**
 * @brief 系统任务调度器
 * @details 检查所有任务的计数器，执行计数器为0的任务并重置其计数器
 */
static void SysTaskScheduler(void)
{
    uint8_t i;
    for(i = 0; i < SysTaskCount; i++)
    {
        if(SysTasks[i].taskCounter == 0)
        {
            SysTasks[i].taskCounter = SysTasks[i].taskInterval;
            if(SysTasks[i].taskFunction != NULL)
            {
                SysTasks[i].taskFunction();
            }
        }
    }
}


void SysTaskRun(void)
{
    uint8_t i;
    if(SysTaskTimerTick > 0)
    {
        SysTaskTimerTick--;
        for(i = 0; i < SysTaskCount; i++)
        {
            if(SysTasks[i].taskCounter > 0)
            {
                SysTasks[i].taskCounter--;
            }
        }
        SysTaskScheduler();
    }
}


void CountTask(void)
{
    static uint16_t j=0;
    j++;
    write_dgus_vp(0x5000, (uint8_t *)&j, 1);
}


void delay_us(const uint16_t us)
{
    uint8_t i;
    uint16_t count = us;
    while(count)
    {
        for(i = 0; i < timeUS_DELAY_TICK; i++)
        {
            __NOP(); 
        }
        count--;
    }
}


void delay_ms(const uint16_t ms)
{
    uint16_t i;
    for(i = 0; i < ms; i++)
    {
        delay_us(1000);
    }
}


uint16_t crc_16 (uint8_t *pBuf, uint16_t buf_len )
{
    uint8_t i;
    uint16_t crc = 0xFFFF;
    while ( buf_len-- )
    {
        crc ^= ( ( *pBuf++ ) & 0x00FF );
        for ( i = 0; i < 8; i++ )
        {
            if ( crc & 0x0001 )
            {
                crc = ( crc >> 1 ) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 内核初始化函数
 * @details 初始化CPU内核相关寄存器，包括时钟控制、定时器控制等
 */
static void KernerlInit()
{
    CKCON 	= 0x00;
    T2CON 	= 0x70;
    DPC 	= 0x00;
    RAMMODE = 0x00;
			
//    sysWDT_ON;
}

/**
 * @brief GPIO初始化函数
 * @details 初始化所有GPIO端口和引脚模式，配置多路复用器和端口驱动模式
 * @param[in] MuxselMode 多路复用器选择模式
 * @param[in] PortdrvMode 端口驱动模式配置
 */
static void GpioInit(uint8_t MuxselMode,uint8_t PortdrvMode)
{
    P0      = 0x00; 
	P1      = 0x00;
	P2      = 0x00;
	P3      = 0x00;
	P0MDOUT = 0x00; 
	P1MDOUT = 0x00; 
	P2MDOUT = 0x00; 
	P3MDOUT = 0x00; 
    MUX_SEL = MuxselMode; 
    PORTDRV = PortdrvMode; 
}

/**
 * @brief 中断系统初始化函数
 * @details 初始化中断使能寄存器和中断优先级寄存器
 */
static void InterruptInit(void)
{
    IEN0 = 0x00;	
	IEN1 = 0x00;
	IEN2 = 0x00;
    IP0 = 0x00; 
	IP1 = 0x00;
}


void read_dgus_vp(uint32_t addr,uint8_t *buf,uint8_t len)
{
	uint16_t OS_addr = 0;
	uint16_t OS_addr_offset = 0;
	uint16_t OS_len = 0, OS_len_offset = 0;

	OS_addr = addr >> 1;
	OS_addr_offset = addr & 0x01;
    ADR_H=(uint8_t)(OS_addr>>16);
    ADR_M=(uint8_t)(OS_addr>>8);
    ADR_L=(uint8_t)OS_addr;
    ADR_INC=1;                 
    RAMMODE=0xAF;               
	while(!APP_ACK);     
    if(OS_addr_offset)       
    {
        APP_EN=1;
        while(APP_EN); 
        *buf++=DATA1;
        *buf++=DATA0;              
        len--;
    }
    OS_len=len>>1;
    OS_len_offset=len&0x01;
    while(OS_len--)
    {
        APP_EN=1;
        while(APP_EN);      
        *buf++=DATA3;
        *buf++=DATA2;
        *buf++=DATA1;
        *buf++=DATA0;                          
    }   
	if(OS_len_offset)
	{          
		APP_EN=1;
		while(APP_EN);      
		*buf++=DATA3;
		*buf++=DATA2;              
	} 
    RAMMODE=0x00;           
}


void write_dgus_vp(uint32_t addr,uint8_t *buf,uint8_t len)
{
	uint16_t OS_addr = 0;
	uint16_t OS_addr_offset = 0;
	uint16_t OS_len = 0, OS_len_offset = 0;
	
	EA = 0;
	OS_addr = addr >> 1;
	OS_addr_offset = addr & 0x01;
	ADR_H = 0;
	ADR_M = (uint8_t)(OS_addr >> 8);
	ADR_L = (uint8_t)OS_addr;
	ADR_INC = 0x01; 
	RAMMODE = 0x83;
	while (!APP_ACK)
		__NOP();
	if (OS_addr_offset)
	{
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while (APP_EN);
		len--;
	}
	OS_len = len >> 1;
	OS_len_offset = len & 0x01;
	RAMMODE = 0x8F; 
	while (OS_len--)
	{
		DATA3 = *buf++;
		DATA2 = *buf++;
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while (APP_EN)
			__NOP();
	}
	if (OS_len_offset)
	{
		RAMMODE = 0x8C;
		DATA3 = *buf++;
		DATA2 = *buf++;
		APP_EN = 1;
		while (APP_EN)
			__NOP();
	}
	RAMMODE = 0x00; 
	EA = 1;
} 

/**
 * @brief T5L CPU完整初始化函数
 * @details 按照预定顺序初始化CPU的各个功能模块
 * @note 初始化顺序：内核 -> GPIO -> 中断 -> UART -> 定时器
 */
void T5LCpuInit(void)
{
    /**
     * 1.内核初始化，kernerinit()
     * 2.定时器初始化，timerinit();
     * 下面是可选项目
     * 3.GpioInit();
     * 4.UartInit();
     * 5.caninit();
     * 6.interruptinit();
     */

     KernerlInit();
     GpioInit(sysMUXSEL_MODE, sysPORTDRV_MODE);
     InterruptInit();
     UartInit();
	 TimerInit();
}