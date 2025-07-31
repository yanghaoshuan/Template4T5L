#include "rtc.h"
#include <string.h>

#define rtcBCD_2_HEX(bcd) ((bcd >> 4) * 10 + (bcd & 0x0F))

static uint8_t RtcCalcWeek(uint8_t *prtc_set)
{
    uint8_t year = prtc_set[0] +2000;
    uint8_t month = prtc_set[1];
    uint8_t day = prtc_set[2];
    uint8_t week;

    if(month < 3)
    {
        month += 12;
        year--;
    }
    week = (day + (13 * (month + 1)) / 5 + year + (year / 4) - (year / 100) + (year / 400)) % 7;
    return week;
}


static void RtcGetTime(uint8_t *prtc_get,uint8_t *prtc_out)
{
    uint8_t i;
    for(i = 0; i < 3; i++)
    {
        prtc_out[i] = rtcBCD_2_HEX(prtc_get[6-i]);
    }
    prtc_out[3] = (prtc_get[3] & 0x07) - 1; //星期从0开始
    for(i = 4; i < 7; i++)
    {
        prtc_out[i] = rtcBCD_2_HEX(prtc_get[6-i]);
    }
    prtc_out[7] = 0;
}


#ifdef rtcRX_8130
void RtcSetTime(uint8_t *prtc_set)
{
    uint8_t write_param[7];
    I2cWriteSingleByte(0x30, 0x00); 
    write_param[0] = 0x48; 
    write_param[1] = 0x00;
    write_param[2] = 0x40;
    write_param[3] = 0x10;
    I2cWriteMultipleBytes(0x1c, write_param, 4);
    write_param[0] = prtc_set[6];
    write_param[1] = prtc_set[5];
    write_param[2] = prtc_set[4];
    write_param[3] = prtc_set[3];
    write_param[4] = prtc_set[2];
    write_param[5] = prtc_set[1];
    write_param[6] = prtc_set[0];
    I2cWriteMultipleBytes(0x10, write_param, 7);
    write_param[0] = 0x00;
    write_param[1] = 0x10;
    I2cWriteMultipleBytes(0x1e, write_param, 2);
}


void RtcInit(void)
{
    uint8_t reset_status;
    uint8_t write_param[7] = {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x20};
    GPIO_BYTE_SET_OUT(i2cGPIO_SFR_PORTMDOUT, (1 << i2cSDA_GPIO_PIN) | (1 << i2cSCL_GPIO_PIN));
    reset_status = I2cReadSingleByte(0x1d);
    if((reset_status & 0x02) == 0x02)
    {
        RtcSetTime(write_param);
    }
}


void RtcReadTime(void)
{
    uint8_t read_param[8],write_param[8];
    I2cReadMultipleBytes(0x10, read_param, 7);
    RtcGetTime(read_param, write_param);
    write_dgus_vp(0x0010, write_param, 4);
}
#elif defined(rtcSD_2058)
void RtcSetTime(uint8_t *prtc_set)
{
    uint8_t write_param[7];
    uint8_t read_param[2];
    I2cReadMultipleBytes(0x0f, read_param, 2);
    read_param[0] |= 0x84;
    read_param[1] |= 0x80;
    I2cWriteSingleByte(0x10, read_param[1]);
    I2cWriteSingleByte(0x0f, read_param[0]);
    write_param[0] = prtc_set[6];
    write_param[1] = prtc_set[5];
    write_param[2] = 0x80;
    write_param[3] = prtc_set[3] % 7;
    write_param[4] = prtc_set[2];
    write_param[5] = prtc_set[1];
    write_param[6] = prtc_set[0];
    I2cWriteMultipleBytes(0x00, write_param, 7);
    read_param[0] &= ~0x84;
    read_param[1] &= ~0x80;
    I2cWriteSingleByte(0x10, read_param[1]);
    I2cWriteSingleByte(0x0f, read_param[0]);
}


void RtcInit(void)
{
    uint8_t read_param[2];
    uint8_t write_param[7] = {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x20};
    GPIO_BYTE_SET_OUT(i2cGPIO_SFR_PORTMDOUT, (1 << i2cSDA_GPIO_PIN) | (1 << i2cSCL_GPIO_PIN));

    I2cReadMultipleBytes(0x0f, read_param, 2);
    if(read_param[0] & 0x01)
    {
        if(read_param[0] & 0x84)
            I2cWriteSingleByte(0x0f, read_param[0] & ~0x84);
        read_param[0] &= ~0x01;
        if(read_param[1] & 0x80)
            I2cWriteSingleByte(0x10, read_param[1] & ~0x80);
        read_param[1] |= 0x80;
        read_param[0] |= 0x84;
        I2cWriteSingleByte(0x10, read_param[1]);
        I2cWriteSingleByte(0x0f, read_param[0]);
    } 
}


void RtcReadTime(void)
{
    uint8_t read_param[8],write_param[8];
    uint8_t i;
    I2cReadMultipleBytes(0x00, read_param, 7);
    RtcGetTime(write_param, read_param);
    write_dgus_vp(0x0010, write_param, 4);
}
#else
void RtcInit(void)
{
    __NOP();
}


void RtcSetTime(uint8_t *prtc_set)
{
    __NOP();
}


void RtcReadTime(void)
{
    __NOP();
}
#endif

void RtcWriteTime(void)
{
    uint8_t read_param[8],write_param[8];
    uint8_t i;
    read_dgus_vp(0x009c, read_param, 4);
    if(read_param[0] == 0x5a && read_param[1] == 0xa5)
    {
        memcpy(write_param, read_param + 2, 3);
        write_param[3] = RtcCalcWeek(write_param);
        memcpy(write_param + 4, read_param + 5, 3);
        write_param[7] = 0;
        for(i=0;i<8;i++)
        {
            write_param[i] = rtcBCD_2_HEX(write_param[i]);
        }
        RtcSetTime(write_param);
        memset(read_param, 0, 2);
        write_dgus_vp(0x009c, read_param, 1);
    }
}


void RtcTask(void)
{
    RtcReadTime();
    RtcWriteTime();
}