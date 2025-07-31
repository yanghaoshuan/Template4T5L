/**
 * @file    i2c.c
 * @brief   I2C通信接口驱动程序实现文件
 * @details 本文件实现了基于GPIO模拟的I2C总线通信协议，包括时序控制、
 *          数据传输和应答机制的完整实现
 * @author  [作者姓名]
 * @date    2025-07-31
 * @version 1.0
 */

#include "i2c.h"
#include "sys.h"

/**
 * @brief I2C启动信号生成
 * @details 生成I2C总线启动条件：在SCL为高电平时，SDA从高电平变为低电平
 * @param 无
 * @return 无
 * @pre SDA引脚配置为推挽输出模式
 * @pre SDA和SCL引脚初始状态应为高电平
 * @post I2C总线进入启动状态，准备数据传输
 * @note 函数执行后SCL和SDA都将处于低电平状态
 */
static void I2cStart(void)
{
    i2cSDA_HIGH;
    i2cSDA_PIN = 1;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 1;
    i2cDelay(i2cDELAY_TICK);
    i2cSDA_PIN = 0;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 0;
}

/**
 * @brief I2C停止信号生成
 * @details 生成I2C总线停止条件：在SCL为高电平时，SDA从低电平变为高电平
 * @param 无
 * @return 无
 * @pre SCL和SDA引脚当前应处于低电平状态
 * @post I2C总线进入空闲状态，通信结束
 * @note 函数执行后SDA和SCL都将处于高电平状态
 */
static void I2cStop(void)
{				
	i2cSDA_HIGH;
	i2cSDA_PIN = 0;
	i2cSCL_PIN = 0;
	i2cDelay(i2cDELAY_TICK);
	i2cSCL_PIN = 1;
	i2cDelay(i2cDELAY_TICK);
	i2cSDA_PIN = 1;
}

/**
 * @brief I2C应答信号生成
 * @details 主机生成ACK应答信号，表示数据接收成功，从机应继续发送数据
 * @param 无
 * @return 无
 * @pre SCL引脚当前应处于低电平状态
 * @post 从机将准备发送下一字节数据
 * @note ACK信号：SDA在SCL高电平期间保持低电平
 */
static void I2cAck(void)
{
    i2cSDA_HIGH;
    i2cSDA_PIN = 0;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 1;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 0;
    i2cDelay(i2cDELAY_TICK);  	           
}

/**
 * @brief I2C非应答信号生成
 * @details 主机生成NACK非应答信号，表示不需要更多数据，从机应停止发送
 * @param 无
 * @return 无
 * @pre SCL引脚当前应处于低电平状态
 * @post 从机将停止数据发送，等待停止信号
 * @note NACK信号：SDA在SCL高电平期间保持高电平
 */
static void I2cNack(void)
{
    i2cSDA_HIGH;
    i2cSDA_PIN = 1;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 1;
    i2cDelay(i2cDELAY_TICK);
    i2cSCL_PIN = 0;
    i2cDelay(i2cDELAY_TICK);  	           
}

/**
 * @brief I2C等待从机应答信号
 * @details 主机等待从机的ACK应答信号，确认数据传输成功
 * @param 无
 * @return 无
 * @pre SDA引脚必须配置为输入模式
 * @post 从机应答检测完成
 * @note 函数会等待最多255个延时周期来检测ACK信号
 * @note 如果从机未应答，函数仍会正常返回
 * @warning 长时间无应答可能表示从机故障或地址错误
 */
static void I2cRecvAck(void)
{
	uint8_t i=0;
	i2cSDA_LOW;
	i2cSDA_PIN = 1;
	i2cDelay(i2cDELAY_TICK);
	i2cSCL_PIN = 1;

	i2cDelay(i2cDELAY_TICK);

	for(i=0;i<255;i++)
	{   
		if(!i2cSDA_PIN)
		{		
			break;
		}
		i2cDelay(i2cDELAY_TICK);
	}
	i2cSCL_PIN = 0;
	i2cSDA_HIGH;
	i2cDelay(i2cDELAY_TICK);
}

/**
 * @brief I2C发送单字节数据
 * @details 按I2C协议时序发送8位数据，MSB先发送
 * @param[in] dat 要发送的8位数据 (0x00-0xFF)
 * @return 无
 * @pre SDA引脚必须配置为输出模式
 * @post 8位数据已按时序发送到总线上
 * @note 数据从最高位(bit7)开始发送到最低位(bit0)
 * @note 发送完成后SDA引脚将配置为输入模式等待应答
 * @warning 调用后必须调用I2cRecvAck()检查从机应答
 */
static void I2cSendByte(uint8_t dat)
{
    uint8_t i;
	i2cSDA_HIGH;	
	for(i=0;i<8;i++)
	{
		i2cSCL_PIN = 0;
		if((dat >> 7) & 0x01) 
			i2cSDA_PIN = 1;				
		else
			i2cSDA_PIN = 0; 				
		i2cDelay(i2cDELAY_TICK);
		i2cSCL_PIN = 1;
		i2cDelay(i2cDELAY_TICK);
		i2cSCL_PIN = 0;
		dat <<= 1;				
	}
	i2cSDA_PIN = 1; 
    i2cDelay(i2cDELAY_TICK); 
	i2cSCL_PIN = 0; 
    i2cDelay(i2cDELAY_TICK);	
	i2cSDA_PIN = 1; 
    i2cDelay(i2cDELAY_TICK);
}

/**
 * @brief I2C接收单字节数据
 * @details 按I2C协议时序接收8位数据，MSB先接收
 * @param 无
 * @return 接收到的8位数据值
 * @retval 0x00-0xFF 从I2C总线接收的数据字节
 * @pre SDA引脚必须配置为输入模式
 * @post 从总线接收的8位数据将被返回
 * @note 数据从最高位(bit7)开始接收到最低位(bit0)
 * @note 接收完成后需要主机发送ACK或NACK应答
 */
static uint8_t I2cReceiveByte()
{
    uint8_t i;
	uint8_t dat = 0;
	i2cSDA_HIGH;
	i2cSCL_PIN = 0;
	i2cDelay(i2cDELAY_TICK);
	i2cSDA_PIN = 1;
	i2cSDA_LOW;
	for(i = 0;i < 8;i++)
	{
		i2cDelay(i2cDELAY_TICK);
		i2cSCL_PIN = 1;
		i2cDelay(i2cDELAY_TICK);
		dat = ((dat << 1) | i2cSDA_PIN);
		i2cSCL_PIN = 0;
	}
	return dat;
}


void I2cWriteSingleByte(uint8_t register_address, uint8_t register_data)
{
    i2cDelay(5*i2cDELAY_TICK);
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS);
    I2cRecvAck();
    I2cSendByte(register_address);
    I2cRecvAck();
    I2cSendByte(register_data);
    I2cStop();
}

void I2cWriteMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS);
    I2cRecvAck();
    I2cSendByte(register_address);
    I2cRecvAck();
    
    for (i = 0; i < length; i++)
    {
        I2cSendByte(buffer[i]);
        I2cRecvAck();
    }
    I2cStop();
}

uint8_t I2cReadSingleByte(uint8_t register_address)
{
    uint8_t dat = 0;
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS);
    I2cRecvAck();
    I2cSendByte(register_address);
    I2cRecvAck();
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS | 0x01); 
    I2cRecvAck();
    dat = I2cReceiveByte();
    I2cNack(); 
    I2cStop();
    return dat;
}

void I2cReadMultipleBytes(uint8_t register_address, uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS);
    I2cRecvAck();
    I2cSendByte(register_address);
    I2cRecvAck();
    I2cStart();
    I2cSendByte(i2cSLAVE_ADDRESS | 0x01); 
    I2cRecvAck();
    
    for (i = 0; i < length; i++)
    {
        buffer[i] = I2cReceiveByte();
        if (i < length - 1)
        {
            I2cAck(); 
        }
        else
        {
            I2cNack(); 
        }
    }
    I2cStop();
}

