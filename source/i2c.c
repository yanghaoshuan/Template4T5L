#include "i2c.h"
#include "sys.h"


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


static uint8_t I2cReceiveByte()
{
    uint8_t i;
	uint8_t dat = 0;
	i2cSDA_HIGH;
	i2cSCL_PIN = 0;
	i2cDelay(5);
	i2cSDA_PIN = 1;
	i2cSDA_LOW;
	for(i = 0;i < 8;i++)
	{
		i2cDelay(5);
		i2cSCL_PIN = 1;
		i2cDelay(5);
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

