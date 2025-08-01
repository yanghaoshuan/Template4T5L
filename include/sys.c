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
    write_dgus_vp(0x5001, (uint8_t *)&time1_i, 1);
    write_dgus_vp(0x5002, (uint8_t *)&time2_j, 1);
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
 * @details 基于迪文 T5L ASIC 应用开发指南进行配置
 */
static void KernerlInit()
{
    CKCON 	    = sysDEFAULT_ZERO;
    DPC 	    = sysDEFAULT_ZERO;
    PAGESEL     = 0x01;
    D_PAGESEL   = 0x02;
    RAMMODE     = sysDEFAULT_ZERO;


//    sysWDT_ON;
}

/**
 * @brief GPIO初始化函数
 * @details 初始化所有GPIO端口和引脚模式，配置多路复用器和端口驱动模式
 * @param[in] MuxselMode 多路复用器选择模式
 * @param[in] PortdrvMode 端口驱动模式配置
 */
static void GpioInit(void)
{
    /* 初始化GPIO端口和引脚模式，初始化为0 */
    /* * 0x00: 输入模式
     *   0x01: 推挽输出模式
     */
    P0      = sysDEFAULT_ZERO; 
	P1      = sysDEFAULT_ZERO;
	P2      = sysDEFAULT_ZERO;
	P3      = sysDEFAULT_ZERO;
	P0MDOUT = sysDEFAULT_ZERO; 
	P1MDOUT = sysDEFAULT_ZERO; 
	P2MDOUT = sysDEFAULT_ZERO; 
	P3MDOUT = sysDEFAULT_ZERO; 
    /* 多路复用器选择模式,有对应的初始化函数进行配置*/
    /*************************
     *   .7 1=CAN 接口引出到 P0.2、P0.3， 0=CAN 接口不引出，为 IO 口。 
     *   .6 1=UART2 接口引出到 P0.4、P0.5， 0=UART2 接口不引出，为 IO 口。 
     *   .5 1=UART3 接口引出到 P0.6、P0.7， 0=UART3 接口不引出，为 IO 口。 
     *   .4-.2 保留 
     *   .1 WDT 控制，1=开启 0=关闭。 
     *   .0 WDT 喂狗，1=喂狗一次（WDT 计数值归零，看门狗的溢出时间为 1 秒）。
     * 
     *****************************************************************/
    MUX_SEL = sysDEFAULT_ZERO; 
    PORTDRV = sysPORTDRV_MODE; 
}

/**
 * @brief 中断系统初始化函数
 * @details 初始化中断使能寄存器和中断优先级寄存器,后续初始化通过位寻址进行设置
 */
static void InterruptInit(void)
{
    IEN0    = sysDEFAULT_ZERO;	
	IEN1    = sysDEFAULT_ZERO;
	IEN2    = sysDEFAULT_ZERO;

    /******** default interrupt priority ************************************
    *权 值   11  10    9   8   7     6      5    4       3       2      1    0 
    *优先级 最高                                                             最低 
    *中 断  EX0 UART3 ET0 CAN EX1 UART4-TX ET1 UART4-RX UART2 UART5-TX ET2 UART5-RX
    ************************************************************************/

    /**************************************************
     ** 分组 IP0对应 IP1对应     中断高优先级    中断低优先级 
     *   G0  .0      .0          外部中断 0      UART3 中断 
     *   G1  .1      .1          T0 定时器中断   CAN 通信中断 
     *   G2  .2      .2          外部中断 1      UART4 发送中断 
     *   G3  .3      .3          T1 定时器中断   UART4 接收中断 
     *   G4  .4      .4          UART2 中断      UART5 发送中断 
     *   G5  .5      .5          T2 定时器中断   UART5 接收中断
     ********************************************************/
    IP0     = sysDEFAULT_ZERO; 
	IP1     = sysDEFAULT_ZERO;
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



uint16_t ReadPageId(void)
{
    uint16_t page_id;
    read_dgus_vp(sysDGUS_PIC_NOW, (uint8_t *)&page_id, 1);
    return page_id;
}


void SwitchPageById(uint16_t page_id)
{
    uint16_t write_param[2];
    write_param[0] = 0x5A01;
	write_param[1] = page_id;
	write_dgus_vp(0x0084,(uint8_t *)&write_param[0],2);
	while(write_param[0] & 0xFF00)
	{
		delay_us(100);
		read_dgus_vp(0x0084,(uint8_t*)&write_param[0],1);
	}
}


#if sysDGUS_AUTO_UPLOAD_ENABLED
void DgusAutoUpload()
{
    /*#todo: Implement the auto-upload logic here*/
    __NOP();
}
#endif /* sysDGUS_AUTO_UPLOAD_ENABLED */


/**
 * @brief T5L CPU完整初始化函数
 * @details 按照预定顺序初始化CPU的各个功能模块
 * @note 初始化顺序：内核 -> GPIO -> 中断 -> UART -> 定时器
 */
void T5LCpuInit(void)
{
    KernerlInit();
    GpioInit();
    InterruptInit();
    UartInit();
	TimerInit();
}


