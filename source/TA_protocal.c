#include "TA_protocal.h"
#include "sys.h"
#if uartTA_PROTOCOL_ENABLED
#define BASIC_GRAPH_ADDR 0x3900
#define MAX_STRING_NUM      10

uint16_t string_vp_addr[MAX_STRING_NUM] = {0x3000,0x3020,0x3040,0x3060,0x3080,0x30A0,0x30C0,0x30E0,0x3100,0x3120};
uint16_t string_sp_addr[MAX_STRING_NUM] = {0x2000,0x2020,0x2040,0x2060,0x2080,0x20A0,0x20C0,0x20E0,0x2100,0x2120};

static uint16_t FindNumberIsInArray(uint16_t x_point,uint16_t y_point,uint16_t *array,uint16_t len)
{
    uint16_t i;
    for(i=0;i<len;i++)
    {
        if(array[i] == x_point&&array[i+MAX_STRING_NUM] == y_point)
        {
            return i;
        }
    }

    for(i=0;i<MAX_STRING_NUM;i++)
    {
        if(array[i] == 0)
        {
            array[i] = x_point;
            array[i+MAX_STRING_NUM] = y_point;
            return i;
        }
    }
    return 0xffff;
}


void UartStandardTAProtocal(UART_TYPE *uart, uint8_t *frame, uint16_t len)
{
    uint16_t crc16,write_param[32];
    uint16_t index,string_mode;
    static uint16_t string_point_array[MAX_STRING_NUM*2] = {0};
    read_dgus_vp(sysDGUS_SYSTEM_CONFIG,(uint8_t*)&crc16,1);
    crc16 = crc16 & 0x0080;
    if(crc16 != 0)
    {
        crc16 = crc_16(frame,len-6);
        if(crc16 != ((frame[len-6] << 8) | frame[len-5]))
        {
            return; 
        }
    }
    if(frame[1] == 0x70)
    {
        SwitchPageById((uint16_t)frame[2]);
    }else if(frame[1] == 0x71)
    {
        write_param[0]=0x0006;
        write_param[1]=0x0001;
        write_param[2]=(uint16_t)frame[2];
        write_param[3]=frame[3]<<8|frame[4];        //x坐标
        write_param[4]=frame[5]<<8|frame[6];        //y坐标
        write_param[5]=frame[7]<<8|frame[8];       
        write_param[5]=write_param[3]+write_param[5];      //x+w坐标
        write_param[6]=frame[9]<<8|frame[10];
        write_param[6]=write_param[4]+write_param[6];      //y+h坐标
        write_param[7]=frame[11]<<8|frame[12];        //x坐标
        write_param[8]=frame[13]<<8|frame[14];        //y坐标
        write_param[9]=0xff00;                                    //结束
        write_dgus_vp(BASIC_GRAPH_ADDR,(uint8_t*)&write_param[0],10);    //写入模式
    }else if(frame[1] == 0x98)
    {
        write_param[0] = frame[2]<<8|frame[3];    //写入x坐标
        write_param[1] = frame[4]<<8|frame[5];    //写入y坐标
        write_param[2] = frame[9]<<8|frame[10];   //写入颜色
        index = FindNumberIsInArray(write_param[0],write_param[1],string_point_array,MAX_STRING_NUM);
        if(index != 0xffff)
        {
            read_dgus_vp(string_sp_addr[index]+0x04,(uint8_t*)&write_param[3],5);
            write_param[9] = 0x0000 | frame[6]; //字库位置
            string_mode = frame[7]&0x0f;
            /** 字体大小 */
            if(string_mode == 0 || string_mode == 5)
            {
                switch(frame[8])
                {
                    /**
                     * 00=8*8 01=6*12 02=8*16 03=12*24 04=16*32 05=20*40 06=24*48 07=28*56 08=32*64
                     * 09(00)=12*12 0A(01)=16*16 0B(02)=24*24 0C(03)=32*32 0D(04)=40*40 0E(05)=48*48 0F(06)=56*56 
                     * 10(07)=64*64 11(08)=40*80 12(09)=48*96 13(0A)=56*112 14(0B)=64*128 15(0C)=80*80 
                     * 16(0D)=96*96 17(0E)=112*112 18(0F)=128*128 19(10)=6*8 1A(11)=8*10 1B(12)=8*12 
                     * 1C(13)=100*200 1D(14)=200*200 1E(15)=48*64
                     */
                    case 0x00: write_param[10] = 0x08<<8|0x08;break;     
                    case 0x01: write_param[10] = 0x06<<8|0x0C;break;    
                    case 0x03: write_param[10] = 0x0C<<8|0x18;break;     
                    case 0x04: write_param[10] = 0x10<<8|0x20;break;   
                    case 0x05: write_param[10] = 0x14<<8|0x28;break;    
                    case 0x06: write_param[10] = 0x18<<8|0x30;break;   
                    case 0x07: write_param[10] = 0x1C<<8|0x38;break;    
                    case 0x08: write_param[10] = 0x20<<8|0x40;break;  
                    case 0x09: write_param[10] = 0x0C<<8|0x0C;break; 
                    case 0x0A: write_param[10] = 0x10<<8|0x10;break; 
                    case 0x0B: write_param[10] = 0x18<<8|0x18;break; 
                    case 0x0C: write_param[10] = 0x20<<8|0x20;break; 
                    case 0x0D: write_param[10] = 0x28<<8|0x28;break; 
                    case 0x0E: write_param[10] = 0x30<<8|0x30;break; 
                    case 0x0F: write_param[10] = 0x38<<8|0x38;break;    
                    case 0x10: write_param[10] = 0x40<<8|0x40;break;    
                    case 0x11: write_param[10] = 0x28<<8|0x50;break;    
                    case 0x12: write_param[10] = 0x30<<8|0x60;break;    
                    case 0x13: write_param[10] = 0x38<<8|0x70;break;    
                    case 0x14: write_param[10] = 0x40<<8|0x80;break;    
                    case 0x15: write_param[10] = 0x50<<8|0x50;break;    
                    case 0x16: write_param[10] = 0x60<<8|0x60;break;    
                    case 0x17: write_param[10] = 0x70<<8|0x70;break;    
                    case 0x18: write_param[10] = 0x80<<8|0x80;break;    
                    case 0x19: write_param[10] = 0x06<<8|0x08;break;    
                    case 0x1A: write_param[10] = 0x08<<8|0x0A;break;    
                    case 0x1B: write_param[10] = 0x08<<8|0x0C;break;
                    case 0x1C: write_param[10] = 0x64<<8|0xc8;break;
                    case 0x1D: write_param[10] = 0xc8<<8|0xc8;break;
                    case 0x1E: write_param[10] = 0x30<<8|0x40;break;   

                    default:write_param[10] = 0x08<<8|0x08;break;    
                }
            }else{
                switch (frame[8])
                {
                    case 0x00: write_param[10] = 0x0C<<8|0x0C;break; 
                    case 0x01: write_param[10] = 0x10<<8|0x10;break; 
                    case 0x02: write_param[10] = 0x18<<8|0x18;break; 
                    case 0x03: write_param[10] = 0x20<<8|0x20;break; 
                    case 0x04: write_param[10] = 0x28<<8|0x28;break; 
                    case 0x05: write_param[10] = 0x30<<8|0x30;break; 
                    case 0x06: write_param[10] = 0x38<<8|0x38;break;    
                    case 0x07: write_param[10] = 0x40<<8|0x40;break;    
                    case 0x08: write_param[10] = 0x28<<8|0x50;break;    
                    case 0x09: write_param[10] = 0x30<<8|0x60;break;    
                    case 0x0A: write_param[10] = 0x38<<8|0x70;break;    
                    case 0x0B: write_param[10] = 0x40<<8|0x80;break;    
                    case 0x0C: write_param[10] = 0x50<<8|0x50;break;    
                    case 0x0D: write_param[10] = 0x60<<8|0x60;break;    
                    case 0x0E: write_param[10] = 0x70<<8|0x70;break;    
                    case 0x0F: write_param[10] = 0x80<<8|0x80;break;    
                    case 0x10: write_param[10] = 0x06<<8|0x08;break;    
                    case 0x11: write_param[10] = 0x08<<8|0x0A;break;    
                    case 0x12: write_param[10] = 0x08<<8|0x0C;break;
                    case 0x13: write_param[10] = 0x64<<8|0xc8;break;
                    case 0x14: write_param[10] = 0xc8<<8|0xc8;break;    
                    case 0x15: write_param[10] = 0x30<<8|0x40;break;
                    default:write_param[10] = 0x0C<<8|0x0C;break;
                }
            }
            
            read_dgus_vp(string_sp_addr[index]+0x0b,(uint8_t*)&write_param[11],1);
            write_param[11] = (write_param[11]&0x00ff) | (frame[7]<<8); //显示模式
            write_dgus_vp(string_sp_addr[index],(uint8_t*)&write_param[0],11);    
            write_dgus_vp(string_vp_addr[index],&frame[11],(len-17+1)>>1);    
        }
    }else if(frame[1] == 0x54)
    {
        /* 显示16*16 GBK字符串，字库是3*/
        write_param[0] = frame[2]<<8|frame[3];    //写入x坐标
        write_param[1] = frame[4]<<8|frame[5];    //写入y坐标
        index = FindNumberIsInArray(write_param[0],write_param[1],string_point_array,MAX_STRING_NUM);
        if(index != 0xffff)
        {
            read_dgus_vp(string_sp_addr[index]+0x04,(uint8_t*)&write_param[2],6);
            write_param[9] = 0x0003; //字库位置
            write_param[10] = 0x10<<8|0x10; //字体大小16*16
            read_dgus_vp(string_sp_addr[index]+0x0b,(uint8_t*)&write_param[11],1);
            write_param[11] = (write_param[11]&0x00ff) | 0x0200; //显示模式
            write_dgus_vp(string_sp_addr[index],(uint8_t*)&write_param[0],11);    
            write_dgus_vp(string_vp_addr[index],&frame[6],(len-12+1)>>1);    
        }
    }else if(frame[1] == 0x55)
    {
        /* 显示32*32 GB2312字符串，字库是5*/
        write_param[0] = frame[2]<<8|frame[3];    //写入x坐标
        write_param[1] = frame[4]<<8|frame[5];    //写入y坐标
        index = FindNumberIsInArray(write_param[0],write_param[1],string_point_array,MAX_STRING_NUM);
        if(index != 0xffff)
        {
            read_dgus_vp(string_sp_addr[index]+0x04,(uint8_t*)&write_param[2],6);
            write_param[9] = 0x0005; //字库位置
            write_param[10] = 0x20<<8|0x20; //字体大小32*32
            read_dgus_vp(string_sp_addr[index]+0x0b,(uint8_t*)&write_param[11],1);
            write_param[11] = (write_param[11]&0x00ff) | 0x0100; //显示模式
            write_dgus_vp(string_sp_addr[index],(uint8_t*)&write_param[0],11);    
            write_dgus_vp(string_vp_addr[index],&frame[6],(len-12+1)>>1);    
        }
    }else if(frame[1] == 0x6e)
    {
        /* 显示12*12 GBK字符串，字库是2*/
        write_param[0] = frame[2]<<8|frame[3];    //写入x坐标
        write_param[1] = frame[4]<<8|frame[5];    //写入y坐标
        index = FindNumberIsInArray(write_param[0],write_param[1],string_point_array,MAX_STRING_NUM);
        if(index != 0xffff)
        {
            read_dgus_vp(string_sp_addr[index]+0x04,(uint8_t*)&write_param[2],6);
            write_param[9] = 0x0002; //字库位置
            write_param[10] = 0x0C<<8|0x0C; //字体大小12*12
            read_dgus_vp(string_sp_addr[index]+0x0b,(uint8_t*)&write_param[11],1);
            write_param[11] = (write_param[11]&0x00ff) | 0x0200; //显示模式
            write_dgus_vp(string_sp_addr[index],(uint8_t*)&write_param[0],11);    
            write_dgus_vp(string_vp_addr[index],&frame[6],(len-12+1)>>1);    
        }
    }else if(frame[1] == 0x6f)
    {
        /* 显示24*24 GB2312字符串，字库是4*/
        write_param[0] = frame[2]<<8|frame[3];    //写入x坐标
        write_param[1] = frame[4]<<8|frame[5];    //写入y坐标
        index = FindNumberIsInArray(write_param[0],write_param[1],string_point_array,MAX_STRING_NUM);
        if(index != 0xffff)
        {
            read_dgus_vp(string_sp_addr[index]+0x04,(uint8_t*)&write_param[2],6);
            write_param[9] = 0x0004; //字库位置
            write_param[10] = 0x18<<8|0x18; //字体大小24*24
            read_dgus_vp(string_sp_addr[index]+0x0b,(uint8_t*)&write_param[11],1);
            write_param[11] = (write_param[11]&0x00ff) | 0x0100; //显示模式
            write_dgus_vp(string_sp_addr[index],(uint8_t*)&write_param[0],11);    
            write_dgus_vp(string_vp_addr[index],&frame[6],(len-12+1)>>1);    
        }
    }
}

#endif /* uartTA_PROTOCOL_ENABLED */