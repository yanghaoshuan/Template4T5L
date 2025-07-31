#include "timer.h"
#include "uart.h"

uint16_t SysTaskTimerTick = 0;

static void StartTimer0(void)
{
	TMOD |= 0x01;
	TH0   = timeT0_TICK >> 8;
	TL0   = timeT0_TICK;
	ET0   = 1;
	TR0   = 1;
}


void TimerInit(void)
{
    TCON  = 0x0F; 
    TMOD  = 0x11;
    
	TH0   = 0x00;
	TL0   = 0x00;
	TR0   = 0x00;

    TH1   = 0x00;
    TL1   = 0x00;
    TR1   = 0x00;

    T2CON = 0x70;  
    TH2   = 0x00; 
    TL2   = 0x00; 

    StartTimer0();
}


void Timer0Isr() interrupt 1
{
    ET0 = 0;
    TH0 = timeT0_TICK >> 8;
    TL0 = timeT0_TICK;

	UartTimerDecrease();

    SysTaskTimerTick++;
    
    ET0 = 1;
}