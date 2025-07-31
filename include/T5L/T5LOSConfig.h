#ifndef T5LOS_CONFIG_H
#define T5LOS_CONFIG_H

#include "t5los8051.h"


typedef unsigned   char uint8_t;
typedef unsigned   int  uint16_t;
typedef unsigned   long uint32_t;
typedef unsigned char   u8;
typedef unsigned short  u16;

#define __NOP()     
#define NULL          ((void*)0)      
#define TRUE          1
#define FALSE         0           
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define DECREASE_IF_POSITIVE(x) ((x)>0 && (x)--)

#define GPIO_BIT_SET_OUT(port, bit)          (port |= (1 << bit))
#define GPIO_BIT_SET_IN(port, bit)           (port &= ~(1 << bit))
#define GPIO_BYTE_SET_OUT(port, byte)        (port |= byte)
#define GPIO_BYTE_SET_IN(port, byte)         (port &= ~byte)

#define sysMUXSEL_MODE               0x60
#define sysPORTDRV_MODE              0x01            
#define sysWDT_ON                    MUX_SEL |= 0x02
#define sysWDT_OFF                   MUX_SEL &= ~0x02
#define sysWDT_RESET                 MUX_SEL |= 0x01

#define sysMAX_TASK_NUM             10

#define sys2K_RATIO                  1
#if sys2K_RATIO
#define sysFOSC                      383385600UL
#else
#define sysFOSC                      206438400UL
#endif
#define sysFCLK                      sysFOSC

#define sysTEST_ENABLED             1

#define timeUS_DELAY_TICK            22
#define timeT0_TICK                  (65536UL-1*sysFOSC/12/1000) 

#define uartUART_COMMON_FRAME_SIZE      100

#define uartMODBUS_PROTOCOL_ENABLED      1

#define uartUART2_ENABLED               1
#if uartUART2_ENABLED
    #define uartUART2_TXBUF_SIZE         256
    #define uartUART2_RXBUF_SIZE         256
    #define uartUART2_TIMEOUT_ENABLED    0
    #if uartUART2_TIMEOUT_ENABLED
        #define uartUART2_TIMEOUTSET     5
    #endif
    #define uartUART2_82CMD_RETURN       
    #define uartUART2_BAUDRATE           115200
    
    #define uartUART2_485_ENABLED        0
    #if uartUART2_485_ENABLED
        sbit TR4 = P0^0;
    #endif
#endif

#define uartUART3_ENABLED               0
#if uartUART3_ENABLED
    #define uartUART3_TXBUF_SIZE         256
    #define uartUART3_RXBUF_SIZE         256
    #define uartUART3_TIMEOUT_ENABLED    0
    #if uartUART3_TIMEOUT_ENABLED
        #define uartUART3_TIMEOUTSET     5
    #endif
    #define uartUART3_82CMD_RETURN       
    #define uartUART3_BAUDRATE           115200
#endif

#define uartUART4_ENABLED               1
#if uartUART4_ENABLED
    #define uartUART4_TXBUF_SIZE         256
    #define uartUART4_RXBUF_SIZE         256
    #define uartUART4_TIMEOUT_ENABLED    0
    #if uartUART4_TIMEOUT_ENABLED
        #define uartUART4_TIMEOUTSET     5
    #endif
    #define uartUART4_82CMD_RETURN       
    #define uartUART4_BAUDRATE           115200
    #define uartUART4_485_ENABLED        0
    #if uartUART4_485_ENABLED
        sbit TR4 = P0^0;
    #endif
#endif

#define uartUART5_ENABLED               0
#if uartUART5_ENABLED
    #define uartUART5_TXBUF_SIZE         256
    #define uartUART5_RXBUF_SIZE         256
    #define uartUART5_TIMEOUT_ENABLED    0
    #if uartUART5_TIMEOUT_ENABLED
        #define uartUART5_TIMEOUTSET     5
    #endif
    #define uartUART5_82CMD_RETURN       
    #define uartUART5_BAUDRATE           115200
    #define uartUART5_485_ENABLED        0
    #if uartUART5_485_ENABLED
        sbit TR5 = P0^1;
    #endif
#endif


#define i2cI2C_ENABLED                  1
#if i2cI2C_ENABLED
    #define i2cGPIO_SFR_PORT            P3
    #define i2cGPIO_SFR_PORTMDOUT       P3MDOUT
    #define i2cSDA_GPIO_PIN             3
    #define i2cSCL_GPIO_PIN             2
    #define i2cDELAY_TICK               ( (const uint16_t) 5 )
    #define i2cSLAVE_ADDRESS            0x64
    sbit i2cSDA_PIN = i2cGPIO_SFR_PORT^i2cSDA_GPIO_PIN;
    sbit i2cSCL_PIN = i2cGPIO_SFR_PORT^i2cSCL_GPIO_PIN;
    #define i2cSDA_HIGH         i2cGPIO_SFR_PORTMDOUT |= (1 << i2cSDA_GPIO_PIN)
    #define i2cSDA_LOW          i2cGPIO_SFR_PORTMDOUT &= ~(1 << i2cSDA_GPIO_PIN)
#endif
		
#endif
