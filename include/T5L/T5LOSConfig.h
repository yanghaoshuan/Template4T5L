/**
 * @file    T5LOSConfig.h
 * @brief   T5L操作系统配置头文件
 * @details 本文件包含T5L嵌入式操作系统的全局配置参数，包括数据类型定义、
 *          系统参数、定时器配置、UART配置和I2C配置等
 * @author  yangming
 * @version 1.0.0
 */

#ifndef T5LOS_CONFIG_H
#define T5LOS_CONFIG_H

#include "t5los8051.h"


/****************************************
***************** 数据类型定义 *************
******************************************/
typedef unsigned   char         uint8_t;
typedef unsigned   int        uint16_t;
typedef unsigned   long         uint32_t;
typedef signed     char		    int8_t;
typedef            int	    int16_t;
typedef            long			int32_t;


/****************************************
***************** 系统宏定义 *************
******************************************/
#define __NOP()                          /* no operation */ 
#define NULL          ((void*)0)      /*标准C语言空指针定义*/
#define TRUE          1
#define FALSE         0           
#define UINT8_PORT_MAX        0xffU
#define UINT16_PORT_MAX       0xffffU
#define UINT32_PORT_MAX       0xfffffffUL
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define SysEnterCritical()  EA = 0;  /* 进入临界区 */
#define SysExitCritical()   EA = 1;  /* 退出临界区 */



/**
 * @brief 正值递减宏定义
 * @details 如果值大于0则递减，否则保持不变
 * @param x 要递减的变量
 * @note 常用于超时计数器的递减操作
 * @warning 会带来额外的汇编负担，尽量不使用
 */
#define DECREASE_IF_POSITIVE(x) ((x)>0 && (x)--)

/* GPIO操作宏定义 */
/**
 * @brief GPIO位设置为推挽输出模式
 * @details 将指定端口的指定位设置为推挽输出模式
 * @param port 目标端口寄存器
 * @param bit 目标位号 (0-7)
 */
#define GPIO_BIT_SET_OUT(port, _bit)          (port |= (1 << _bit))

/**
 * @brief GPIO位设置为输入模式
 * @details 将指定端口的指定位设置为输入模式
 * @param port 目标端口寄存器
 * @param bit 目标位号 (0-7)
 */
#define GPIO_BIT_SET_IN(port, _bit)           (port &= ~(1 << _bit))

/**
 * @brief GPIO字节设置为输出模式
 * @details 将指定端口的指定字节位设置为输出模式
 * @param port 目标端口寄存器
 * @param byte 要设置的位掩码
 */
#define GPIO_BYTE_SET_OUT(port, byte)        (port |= byte)

/**
 * @brief GPIO字节设置为输入模式
 * @details 将指定端口的指定字节位设置为输入模式
 * @param port 目标端口寄存器
 * @param byte 要清除的位掩码
 */
#define GPIO_BYTE_SET_IN(port, byte)         (port &= ~byte)



/* 系统配置参数 */

/**
 * @brief 端口驱动模式配置
 * @details 配置GPIO端口的驱动能力模式 00:4mA,01:8mA(推荐),10:16mA，11:32mA
 */
#define sysPORTDRV_MODE              0x03            

/* 看门狗 */
#define sysWDT_ON                    MUX_SEL |= 0x02
#define sysWDT_OFF                   MUX_SEL &= ~0x02
#define sysWDT_RESET                 MUX_SEL |= 0x01

/**
 * @brief 系统最大任务数量
 */
#define sysMAX_TASK_NUM             10



/**
 * @brief 广告屏和美容屏功能使能标志
 * @details 1: 启用广告屏和美容屏功能, 0: 禁用
 * @warning 打开这个宏后需要去startup文件中配置R11模块,禁用时需要关闭配置R11模块以使用外部中断0
 * @warning 广告屏美容屏和模拟摄像头开关互斥，注意只能打开一个
 */
#define sysADVERTISE_MODE_ENABLED       0
#define sysN5CAMERA_MODE_ENABLED       0
#define sysBEAUTY_MODE_ENABLED         0

#if ((sysN5CAMERA_MODE_ENABLED + sysBEAUTY_MODE_ENABLED + sysADVERTISE_MODE_ENABLED) > 1)
#error "ONLY CAN CHOOSE ONE:ADVERTISE,N5CAMERA,BEAUTY!"
#endif /* ((sysN5CAMERA_MODE_ENABLED + sysBEAUTY_MODE_ENABLED + sysADVERTISE_MODE_ENABLED) > 1) */

#if sysN5CAMERA_MODE_ENABLED
#define Uart_R11                     Uart3
#define R11_WIFI_ENABLED              0
#endif /* sysN5CAMERA_MODE_ENABLED */

#if sysBEAUTY_MODE_ENABLED
#define Uart_R11                     Uart5
#define R11_WIFI_ENABLED              1
#define R11_HAIR_ANALYZE_ENABLED      1          /**< 头皮检测分析功能使能标志 */
#endif /* sysBEAUTY_MODE_ENABLED */

#if sysADVERTISE_MODE_ENABLED
#define Uart_R11                     Uart5
#define R11_WIFI_ENABLED              1
#endif /* sysADVERTISE_MODE_ENABLED */  

#ifndef R11_WIFI_ENABLED
#define R11_WIFI_ENABLED              0
#endif /* R11_WIFI_ENABLED */

#define sysSET_FROM_LIB              sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED

#if sysSET_FROM_LIB
extern uint16_t sys_2k_ratio;
extern uint32_t sysFOSC;
extern uint32_t sysFCLK;
#else
/**
 * @brief 2k分辨率模式
 * @details 1: 1920*1080分辨率屏幕, 0: 其他
 */
#define sys2K_RATIO                  0

#if sys2K_RATIO
/**
 * @brief 系统振荡器频率 (2K分辨率)
 * @details 系统主振荡器频率，单位Hz
 */
#define sysFOSC                      383385600UL
#else
/**
 * @brief 系统振荡器频率 (其他)
 * @details 系统主振荡器频率，单位Hz
 */
#define sysFOSC                      206438400UL
#endif /* sys2K_RATIO */

#define sysFCLK                      sysFOSC
#endif /* sysSET_FROM_LIB */

#define sysTEST_ENABLED                 1        /**< 测试模式使能标志 */


#define uartTA_PROTOCOL_ENABLED          0
#define v851PROTOCOL_ENABLED             1      /**< UART4 V851 TLV/Wi-Fi协议 */
#define v851STATE_FULL_REPORT_ENABLED    1      /**< 暂时关闭每分钟一次的0x37全量上报 */
#define pb03fBLE_ENABLED                 0      /**< PB-03F源码保留，当前产品禁用 */
#define blePB03F_UART_ID                 5      /**< 仅在重新启用PB-03F时选择UART */
#define v851CONTROL_MOCK_ENABLED         0      /**< 旧JSON控制模拟器停用 */

/* V851工厂设备信息，产品变更时可在此覆盖。 */
#define v851FACTORY_SALES_COUNTRY        "CN"
#define v851FACTORY_PRODUCT_KEY          "MCQX_PET_CABIN"
#define v851FACTORY_MODEL                "MCQX-PET-CABIN-V1"
#define v851FACTORY_HARDWARE_VERSION     "HW-V2.0"
#define v851FACTORY_FIRMWARE_VERSION     "FW-V1.0.0"

#if pb03fBLE_ENABLED && !v851PROTOCOL_ENABLED
#error "PB-03F compatibility requires the V851 protocol module."
#endif

#if pb03fBLE_ENABLED && \
    ((blePB03F_UART_ID != 2) && (blePB03F_UART_ID != 5))
#error "blePB03F_UART_ID must be 2 or 5."
#endif

#if pb03fBLE_ENABLED && (blePB03F_UART_ID == 2) && uartTA_PROTOCOL_ENABLED
#error "TA protocol cannot share UART2 with PB-03F."
#endif

#define sysDGUS_AUTO_UPLOAD_ENABLED      0      /**< V851配网直接轮询0x0600，不占用UART2 */
#if sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED
#define sysDGUS_AUTO_UPLOAD_VP_ADDR            0x0f00
#define sysDGUS_AUTO_UPLOAD_LEN                 40
#endif /* sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED */

#define sysDEFAULT_ZERO              (uint8_t )0

#define sysDGUS_CHART_ENABLED              0        /**< 图表功能使能标志 */

#define flashDUAL_BACKUP_ENABLED            0               /**< 双备份使能标志 */

/**
 * @brief OTA升级功能配置
 * @details 应用固件通过UART4接收AB CD升级数据帧并写入NAND。
 */
#define otaOTA_ENABLED                 1              /**< 启用V851应用层OTA */
#define otaCRC32_CHECK_ENABLED         1              /**< OTA整文件CRC32校验使能标志 */
#define otaDEBUG_ENABLED               0              /**< OTA调试输出使能标志，当前默认关闭 */
#define otaTASK_INTERVAL               2              /**< OTA周期任务执行间隔，单位为系统任务节拍 */
#define otaDOWNLOAD_MAX                20             /**< 单次OTA最多支持的文件数量 */

#define otaNAND_START_ADDR             0x04000000UL   /**< OTA文件下载到NAND Flash的起始地址 */
#define otaDATA_START_BLOCK            64             /**< OTA文件数据起始4KB块号，0-63块保留给头文件区域 */
#define otaCACHE_VP_A                  0x7000         /**< OTA写NAND使用的第一个DGUS 4KB缓存区 */
#define otaCACHE_VP_B                  0x7800         /**< OTA写NAND使用的第二个DGUS 4KB缓存区 */
#define otaSPEED_VP_ADDR               0x3FFE         /**< OTA下载进度数值VP地址 */
#define otaSPEED_SP_ADDR               0x4FE0         /**< OTA下载进度控件SP地址 */
#define otaTEST_TRIGGER_ADDR           0x40C1         /**< OTA测试触发VP地址，写入0x5AA5后发送F3命令 */
#define otaUPGRADE_FLAG_ADDR           0x51F0         /**< OTA升级完成标志VP/NOR地址 */
#define otaUPDATE_INFO_ADDR            0x4100         /**< OTA版本号、时间段等信息起始VP地址 */
#define otaCHARGE_STATUS_ADDR          0x1000         /**< 参考项目保留的充电状态VP地址 */

#if otaOTA_ENABLED && !v851PROTOCOL_ENABLED && !(sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED)
#error "OTA requires the V851 bridge or one legacy R11 mode."
#endif /* otaOTA_ENABLED && no transport */



#define gpioGPIO_ENABLE             0  /**< GPIO使能标志 */
#define gpioPWM_ENABLE              0  /**< PWM使能标志 */


#define timeUS_DELAY_TICK                22         /* 微秒延时的定时器计数值，在level8的优化模式下 */

/**
 * @brief 定时器0重装载值计算
 * @details 根据系统频率和定时模式计算的定时器0重装载值
 * @note 用于产生1ms的定时中断
 */
#define timeT0_TICK                      (65536UL-sysFOSC/12/1000)


/**
 * @brief 定时器1使能标志
 * @details 1: 启用定时器1, 0: 禁用定时器1
 */
#define timeTIMER1_ENABLED              0

#if timeTIMER1_ENABLED  

/**
 * @brief 定时器1重装载值计算
 * @details 根据系统频率和分频模式计算的定时器1重装载值
 * @note 用于产生1ms的定时中断
 */
#define timeT1_TICK                       (65536UL-sysFOSC/12/1000)
#endif /* timeTIMER1_ENABLED */

#define timeTIMER2_ENABLED              0
#if timeTIMER2_ENABLED
/**
 * @brief 定时器2频率模式选择
 * @details 1: 12分频模式, 2: 24分频模式
 */
#define timeT2_FREQ_MODE                 2

#define timeT2_TICK                     (65536UL-sysFOSC/timeT2_FREQ_MODE/12/1000)  /* 定时器2重装载值计算 */
#endif /* timeTIMER2_ENABLED */

/* UART通用配置参数 */
/**
 * @brief UART通用帧缓冲区大小
 * @details 所有UART接口共用的数据帧缓冲区大小，单位为字节
 */
#define uartUART_COMMON_FRAME_SIZE     4160

/**
 * @brief Modbus协议支持使能标志
 * @details 1: 启用Modbus协议支持, 0: 禁用Modbus协议支持
 */
#define uartMODBUS_PROTOCOL_ENABLED      1

#define uartUART_82CMD_RETURN            0       /*UART2命令返回使能标志 0:禁用 1启用 */   

/* UART2配置参数 */
/**
 * @brief UART2使能标志
 * @details 1: 启用UART2接口, 0: 禁用UART2接口
 */
#define uartUART2_ENABLED               1

#if uartUART2_ENABLED
    /**
     * @brief UART2发送和接收缓冲区大小
     */
    #define uartUART2_TXBUF_SIZE         256
    #define uartUART2_RXBUF_SIZE         256
    
    /**
     * @brief UART2超时功能使能标志
     * @details 1: 启用接收超时检测, 0: 禁用接收超时检测
     */
    #define uartUART2_TIMEOUT_ENABLED    uartUART2_ENABLED
    
    #if uartUART2_TIMEOUT_ENABLED
        /**
         * @brief UART2超时设置值
         * @details 接收超时的计数值，单位为定时器中断周期
         */
        #define uartUART2_TIMEOUTSET     5
    #endif /* uartUART2_TIMEOUT_ENABLED */
    
    #define uartUART2_BAUDRATE              115200     /* UART2波特率设置 9600-460800 */
    #define uartUART2_485_ENABLED            0         /* UART2 RS485模式使能标志 0:禁用 1启用 */
    
    #if uartUART2_485_ENABLED
        sbit TR4 = P0^0;    /* UART2 RS485发送使能引脚定义 */
    #endif  /* uartUART2_485_ENABLED */
#endif  /* uartUART2_ENABLED */

/* UART3配置参数，配置同uart2 */
#define uartUART3_ENABLED               0

#if uartUART3_ENABLED
    #define uartUART3_TXBUF_SIZE         256
    #define uartUART3_RXBUF_SIZE         uartUART_COMMON_FRAME_SIZE
    #define uartUART3_TIMEOUT_ENABLED    uartUART3_ENABLED
    #if uartUART3_TIMEOUT_ENABLED
        #define uartUART3_TIMEOUTSET     5
    #endif /* uartUART3_TIMEOUT_ENABLED */  
    #define uartUART3_BAUDRATE           921600
#endif /* uartUART3_ENABLED */

/* UART4配置参数，配置同uart2 */
#define uartUART4_ENABLED               1

#if uartUART4_ENABLED
    #define uartUART4_TXBUF_SIZE         2112
    #define uartUART4_RXBUF_SIZE         4160
    #define uartUART4_TIMEOUT_ENABLED    uartUART4_ENABLED
    
    #if uartUART4_TIMEOUT_ENABLED
        #define uartUART4_TIMEOUTSET     5
    #endif  /* uartUART4_TIMEOUT_ENABLED */   
    #define uartUART4_BAUDRATE           115200
    #define uartUART4_485_ENABLED        0
    
    #if uartUART4_485_ENABLED
        sbit TR4 = P0^0;
    #endif  /* uartUART4_485_ENABLED */
#endif  /* uartUART4_ENABLED */

#if v851PROTOCOL_ENABLED && !uartUART4_ENABLED
#error "V851 protocol requires UART4."
#endif

#if v851PROTOCOL_ENABLED && (uartUART_COMMON_FRAME_SIZE < 4160U)
#error "V851 UART scratch buffer must be at least 4160 bytes."
#endif

#if v851PROTOCOL_ENABLED && \
    ((uartUART4_RXBUF_SIZE < 4160U) || (uartUART4_TXBUF_SIZE <= 2048U))
#error "V851 UART4 buffers do not meet TLV/OTA limits."
#endif

/* UART5配置参数，配置同uart2 */
#define uartUART5_ENABLED               (sysBEAUTY_MODE_ENABLED || \
                                         sysADVERTISE_MODE_ENABLED || \
                                         (pb03fBLE_ENABLED && \
                                          (blePB03F_UART_ID == 5)))

#if uartUART5_ENABLED
    #define uartUART5_TXBUF_SIZE         256
    #define uartUART5_RXBUF_SIZE         2048
    #define uartUART5_TIMEOUT_ENABLED    uartUART5_ENABLED
    
    #if uartUART5_TIMEOUT_ENABLED
        #define uartUART5_TIMEOUTSET     5
    #endif  /* uartUART5_TIMEOUT_ENABLED */ 
    #if sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED
    #define uartUART5_BAUDRATE           921600
    #else
    #define uartUART5_BAUDRATE           115200
    #endif /* sysBEAUTY_MODE_ENABLED || sysN5CAMERA_MODE_ENABLED || sysADVERTISE_MODE_ENABLED */
    #define uartUART5_485_ENABLED        0
    
    #if uartUART5_485_ENABLED
        sbit TR5 = P0^1;
    #endif  /* uartUART5_485_ENABLED */
#endif  /* uartUART5_ENABLED */


/* I2C配置参数 */
/**
 * @brief I2C功能使能标志
 * @details 1: 启用I2C通信功能, 0: 禁用I2C通信功能
 */
#define i2cI2C_ENABLED                  1

#if i2cI2C_ENABLED
    /**
     * @brief I2C GPIO端口定义
     * @details 指定I2C通信使用的GPIO端口
     */
    #define i2cGPIO_SFR_PORT            P3
    
    /**
     * @brief I2C GPIO端口模式寄存器定义
     * @details 指定I2C GPIO端口的模式控制寄存器
     */
    #define i2cGPIO_SFR_PORTMDOUT       P3MDOUT
    
    /**
     * @brief I2C SDA数据线引脚号
     * @details SDA(Serial Data)数据线使用的GPIO引脚号
     */
    #define i2cSDA_GPIO_PIN             3
    
    /**
     * @brief I2C SCL时钟线引脚号
     * @details SCL(Serial Clock)时钟线使用的GPIO引脚号
     */
    #define i2cSCL_GPIO_PIN             2
    
    /**
     * @brief I2C时序延时节拍数
     * @details I2C通信时序控制的基本延时单位，单位为微秒
     */
    #define i2cDELAY_TICK               ( (const uint16_t) 5 )
    
    /**
     * @brief I2C从机地址定义
     * @details 当前设备作为I2C从机时的地址标识
     */
    #define i2cSLAVE_ADDRESS            0x64
    
    /**
     * @brief I2C SDA引脚位操作定义
     * @details SDA数据线的位级操作变量定义
     */
    sbit i2cSDA_PIN = i2cGPIO_SFR_PORT^i2cSDA_GPIO_PIN;
    
    /**
     * @brief I2C SCL引脚位操作定义
     * @details SCL时钟线的位级操作变量定义
     */
    sbit i2cSCL_PIN = i2cGPIO_SFR_PORT^i2cSCL_GPIO_PIN;
    
    /**
     * @brief I2C SDA引脚设置为输出高电平
     * @details 将SDA引脚配置为输出模式并设置为高电平
     */
    #define i2cSDA_HIGH         i2cGPIO_SFR_PORTMDOUT |= (1 << i2cSDA_GPIO_PIN)
    
    /**
     * @brief I2C SDA引脚设置为输入模式
     * @details 将SDA引脚配置为输入模式，用于接收数据或应答信号
     */
    #define i2cSDA_LOW          i2cGPIO_SFR_PORTMDOUT &= ~(1 << i2cSDA_GPIO_PIN)
#endif  /* i2cI2C_ENABLED */

/* SPI配置参数 */
/**
 * @brief SPI功能使能标志
 * @details 1: 启用SPI功能, 0: 禁用SPI功能
 */
#define spiSPI_ENABLED                  1
#if spiSPI_ENABLED
/**
 * @brief SPI GPIO端口定义
 * @details 指定SPI通信使用的GPIO端口
 */
#define spiGPIO_PORT                    P1     

/**
 * @brief SPI GPIO端口模式寄存器定义
 * @details 指定SPI GPIO端口的模式控制寄存器
 */
#define spiGPIO_SFR_PORTMDOUT         P1MDOUT

/**
 * @brief SPI时钟引脚号
 * @details SCK(Serial Clock)时钟线使用的GPIO引脚号
 */
#define spiSCK_PIN                      0

/**
 * @brief SPI主机输出从机输入引脚号
 * @details MOSI(Master Out Slave In)数据线使用的GPIO引脚号
 */
#define spiMOSI_PIN                     1

/**
 * @brief SPI主机输入从机输出引脚号
 * @details MISO(Master In Slave Out)数据线使用的GPIO引脚号
 */
#define spiMISO_PIN                     2

/**
 * @brief SPI片选引脚号
 * @details CS(Chip Select)片选信号使用的GPIO引脚号
 */
#define spiCS_PIN                       3
#endif /* spiSPI_ENABLED */

#define canCAN_ENABLED                  0

#define _4G_AIR780E_ENABLED             0

#endif /* T5LOS_CONFIG_H */
