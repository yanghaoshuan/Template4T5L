#include "r11_common.h"
#include "r11_netskinAnalyze.h"
#include "sys.h"
#include "T5LOSConfig.h"
#include "uart.h"
#include <string.h>

#if sysBEAUTY_MODE_ENABLED

/** 调试数据定义区域 */
volatile uint8_t    xdata       buf_jpeg[0x8000]            _at_    0x8000;
volatile uint8_t    data        EX1_Len                     _at_    0x20;
volatile uint8_t    data        Picture_Count               _at_    0x21;
volatile uint16_t   data        buf_tail                    _at_    0x22;
volatile uint16_t   data        buf_os_tail                 _at_    0x24;
volatile uint8_t    data        Packet_Count                _at_    0x26;
volatile uint8_t    data        Packet_TotalCount           _at_    0x27;
volatile uint32_t   data        JpegLaber_OS_ADR            _at_    0x28;
volatile uint32_t   data        JpegLaber_DGUSII_VP         _at_    0x2C;

volatile uint8_t    data        Other_Cmd_Count             _at_    0x30;
volatile uint8_t    data        Miss_Align_16KB_Count       _at_    0x31;
volatile uint8_t    data        NotDeal_OverBuffer_Count    _at_    0x32;
volatile uint8_t    data        Over_Max_16KB_Count         _at_    0x33;
volatile uint8_t    data        CRC_ERR_Count               _at_    0x34;
volatile uint8_t    data        data_write_f                _at_    0x35;
volatile uint8_t    data        state                       _at_    0x36;
volatile uint8_t    data        Kind                        _at_    0x37;

volatile uint8_t    data        Icon_Num                    _at_    0x38;
volatile uint8_t    data        Max_16KB_Count              _at_    0x39;
volatile uint8_t    data        Big_Small_Flag              _at_    0x3A;
volatile uint8_t    data        Rx3_Len                     _at_    0x3B;
volatile uint8_t    data        END_CRC                     _at_    0x3C;
volatile uint8_t    data        Dgus_Need_Restart           _at_    0x3D;
volatile uint8_t    data        Restart_Dgus_Time           _at_    0x3E;
volatile uint8_t    data        TimeOut_Count               _at_    0x3F;

volatile uint8_t    data        TimeOut                     _at_    0x40;
volatile uint16_t   data        Jpeg_Word_Len               _at_    0x41;
volatile uint8_t    data        Pick_Picture                _at_    0x43;
volatile uint8_t    data        Picture_Flag                _at_    0x44;
volatile uint8_t    data        pic_time_cnt                _at_    0x45;
volatile uint8_t    data        Bit8_16_Flag                _at_    0x46;
volatile uint16_t   data        write_f_time                _at_    0x47;

volatile uint8_t    idata       Pic_Count[10]               _at_    0xb0;


code uint16_t Icon_Overlay_SP[11] = {0x0700,0x0700,0x0700,0x0700,0x0710,0x0720,0x0730,0x0740,0x0750,0x0760,0x770};
uint32_t Icon_Overlay_SP_VP[11] = {0x07000,0x1d000,0x07000,0x1d000,0x33000,0x37000,0x3b000,0x3f000,0x3f000,0x3f000,0x3F000};
uint16_t Icon_Overlay_SP_X[10];  
uint16_t Icon_Overlay_SP_Y[10];
uint16_t Icon_Overlay_SP_L[10];
uint16_t Icon_Overlay_SP_H[10]; 
uint16_t Locate_arr[10]; 

uint8_t mp4_name[5][MAX_MP3_NAME_LEN]=0;
uint16_t mp4_name_len[5];
PLAYER_T r11_player;






void T5lJpegInit(void)
{
    P1MDOUT         =   0x00;
    P2MDOUT         =   0x00;
    P3MDOUT         &= ~0x03;
    IT0             =   1;
    IT1             =   1;
    EX0             =   0;
    EX1             =   0;
    data_write_f    =   0;
    memset(buf_jpeg, 0, sizeof(buf_jpeg));
    EX1_Start();
}


void T5lSendUartDataToR11( uint8_t cmd, uint8_t *buf)
{
    #define BUF_MAX_LEN  256
    uint8_t 	i,len,now_len,Temp_Buf_uint8_t[10];
    uint8_t     r11_buf[BUF_MAX_LEN];
    uint16_t    CRC16;
    r11_buf[0] = 0xAA;
    r11_buf[1] = 0x55;
    r11_buf[4] = cmd;
    switch (cmd)
    {
        case 0x60: 
        case 0x61: 
        case 0x6E: 
        case 0x6F: 
        case 0x78: 
        case 0x79: 
        case 0x90:
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x03;
            r11_buf[5] = buf[0];
            r11_buf[6] = buf[1];
            break;
        case 0x6B:
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x02;
            r11_buf[5] = buf[0];
            break;
        case 0x62:
        case 0x63: 
        case 0x65: 
        case 0x66: 
        case 0x67: 
        case 0x68: 
        case 0x69: 
        case 0x6C: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x02;
            r11_buf[5] = 0x00;
            break;
        case 0x64: 
            len = mp4_name_len[r11_player.serial];
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x01;
            if (len > MAX_MP3_NAME_LEN)
            {
                return;
            }
            now_len = 5;

            switch (r11_player.store_type)
            {
                case 1:
                case 2:
                case 3:
                    Temp_Buf_uint8_t[0] = strlen ( DirPath ( r11_player.store_type ) );
                    memcpy ( &r11_buf[5], DirPath ( r11_player.store_type ), Temp_Buf_uint8_t[0] );
                    now_len += Temp_Buf_uint8_t[0];
                    break;
                default:
                    r11_player.store_type = 2;
                    Temp_Buf_uint8_t[0] = strlen ( DirPath ( r11_player.store_type ) ) ;
                    memcpy ( &r11_buf[5], DirPath ( r11_player.store_type ), Temp_Buf_uint8_t[0] );
                    now_len += Temp_Buf_uint8_t[0];
                    break;
            }
            memcpy ( &r11_buf[now_len], buf, len );
            len += now_len;
            r11_buf[len++] = 0x00;
            r11_buf[3] += len - 5;
            break;
        case 0x6A: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x05;
            memcpy ( &r11_buf[5], buf, 4 );
            break;
        case 0x76: 
        case 0x77: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x05;
            r11_buf[5] = buf[0];
            r11_buf[6] = buf[1];
            r11_buf[7] = buf[2];
            r11_buf[8] = buf[3];
            break;
        case 0x6D: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x02;
            r11_buf[5] = 0x01;
            break;
        case 0x75: 
            r11_buf[2] = 0x00;
            r11_buf[3] = 0x01;
            break;
        case 0xF0:
            r11_buf[2] = 0x00;
            r11_buf[3] = 12;
            read_dgus_vp(0x411, ( uint8_t * ) &r11_buf[5],5 );
            r11_buf[15] = '\0';
            break;
        case 0x85:
        case 0x88: 
            r11_buf[2] = buf[2];
            r11_buf[3] = buf[3];
            r11_buf[4] = buf[4];
            r11_buf[5] = buf[5];
            break;
        case 0xF1:
            read_dgus_vp(0x680, ( uint8_t * ) &r11_buf[5],32 );
            for ( i=0; i<64; i++ )
            {
                if ( r11_buf[5+i] == 0xFF )
                {
                    r11_buf[5+i] = '\0';
                    i++;
                    break;
                }
            }
            r11_buf[2] = 0x00;
            r11_buf[3] = i+1;
            break;
        case 0xF2:
            break;
        default:
            return;
            break;
    }
    CRC16 = crc_16 ( &r11_buf[4], r11_buf[3] );
    r11_buf[r11_buf[3]+4] = CRC16 >> 8;
    r11_buf[r11_buf[3]+5] = CRC16 & 0xFF;
    r11_buf[3] += 2;
    UartSendData ( &Uart_R11, r11_buf, r11_buf[3]+4 );
    return;
}





void inter_extern1_1_fun_C(void)    interrupt 2
{
    uint8_t data Temp,Index, ADR_H_Bak,ADR_M_Bak,ADR_L_Bak,ADR_INC_Bak,DATA3_Bak,DATA2_Bak,DATA1_Bak,DATA0_Bak,RAMMODE_Bak;
	uint16_t zero_value = 0;
    state = P1;
    /** 进中断进行栈备份 */
    if ( RAMMODE )
    {
        while ( APP_EN );
    }
    RAMMODE_Bak = RAMMODE;
    ADR_H_Bak = ADR_H;
    ADR_M_Bak = ADR_M;
    ADR_L_Bak = ADR_L;
    ADR_INC_Bak = ADR_INC;
    DATA3_Bak = DATA3;
    DATA2_Bak = DATA2;
    DATA1_Bak = DATA1;
    DATA0_Bak = DATA0;
    RAMMODE = 0x00;
    EX1_Len++;
    Temp = state & 0xE0;
    Display_Debug_Message();
    switch ( Temp )
    {
        case 0x80:
        {
            if ( data_write_f == 0x00 )
            {
                Kind = state & 0x1F;
                switch ( Kind )
                {
                    case 0:
                    case 1:
                    case 3:
                    case 4:
                    case 10:
                    {
                        Max_16KB_Count = ( Icon_Overlay_SP_VP[ ( Big_Small_Flag<<1 )+1] - Icon_Overlay_SP_VP[ ( Big_Small_Flag<<1 )] ) >>13;
                        Icon_Num = ( Pic_Count[0]%2 ) + ( Big_Small_Flag<<1 ) + 1;
                        Index = Icon_Num - 1;
                        JpegLaber_DGUSII_VP = Icon_Overlay_SP_VP[Index];
                        JpegLaber_OS_ADR = JpegLaber_DGUSII_VP >> 1;
                        Init_Jpeg_Parament();
                        data_write_f = 1;
                        EX0_Start();
                        break;
                    }
                    default:
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                        break;
                    }
                }
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0x40:
        {
            if ( data_write_f == 0x01 )
            {
                if ( 1 | ( Packet_Count < 1 ) )
                {
                    Temp = ( state & 0xC0 ) | ( Packet_TotalCount + 1 );
                    if ( Temp == state )
                    {
                        if ( Packet_TotalCount + 1 < Max_16KB_Count )
                        {
                            Packet_Count++;
                            Packet_TotalCount++;
                            if ( Packet_TotalCount & 0x01 )
                            {
                                if ( buf_tail == 0xC000 )
                                {
                                    Judge_Packet_Count();
                                }
                                else
                                {
                                    Miss_Align_16KB_Count++;
                                    data_write_f = 0x03;
                                    EX0 = 0;
                                    EX1 = 0;
                                }
                            }
                            else
                            {
                                if ( buf_tail == 0x8000 )
                                {
                                    Judge_Packet_Count();
                                }
                                else
                                {
                                    Miss_Align_16KB_Count++;
                                    data_write_f = 0x03;
                                    EX0 = 0;
                                    EX1 = 0;
                                }
                            }
                        }
                        else
                        {
                            Over_Max_16KB_Count++;
                            data_write_f = 9;
                            EX0 = 0;
                            EX1 = 0;
                        }
                    }
                    else
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                    }
                }
                else
                {
                    NotDeal_OverBuffer_Count++;
                    data_write_f = 4;
                    EX0 = 0;
                    EX1 = 0;
                }
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0xA0:
        {
            if ( data_write_f == 0x00 )
            {

                Kind = state & 0x1F;
                Icon_Num = 5 + Kind;
                Max_16KB_Count = ( Icon_Overlay_SP_VP[Icon_Num] - Icon_Overlay_SP_VP[Icon_Num-1] ) >>13;
                JpegLaber_DGUSII_VP = Icon_Overlay_SP_VP[Icon_Num-1];
                JpegLaber_OS_ADR = JpegLaber_DGUSII_VP >> 1;
                Index = Icon_Num - 1;
                Init_Jpeg_Parament();
                data_write_f = 1;
                EX0_Start();
            }
            else
            {
                NotDeal_OverBuffer_Count++;
                data_write_f = 4;
                EX0 = 0;
                EX1 = 0;
            }
            break;
        }
        case 0xC0:
        {
            EX0 = 0;
            if ( data_write_f == 0x01 )
            {
                if ( 1 | ( Packet_Count < 1 ) )
                {
                    Temp = ( state & 0xC0 ) | ( Packet_TotalCount + 1 );
                    if ( Temp == state )
                    {
                        if ( Packet_TotalCount <  Max_16KB_Count )
                        {
                            if ( 1 || END_CRC == 0 )
                            {
                                Packet_Count++;
                                Packet_TotalCount++;
                                if ( Packet_TotalCount & 0x01 )
                                {

                                    if ( buf_tail == 0xC000 )
                                    {
                                        data_write_f = 2;
                                        {
                                            Judge_Packet_Count();
                                            if ( END_CRC != 0 )
                                            {
                                                CRC_ERR_Count++;
                                                data_write_f = 8;
                                                EX0 = 0;
                                                EX1 = 0;
                                            }
                                            else
                                            {

												ADR_H = JpegLaber_DGUSII_VP >> 17;
												ADR_M = JpegLaber_DGUSII_VP >> 9;
												ADR_L = JpegLaber_DGUSII_VP >> 1;
												ADR_INC = 0x00;
												RAMMODE = 0xAF;
												while ( !APP_ACK );
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x8F;
												DATA1 = DATA2;
												DATA3 = 0x5A;
												DATA2 = 0xA5;
												//DATA1 = 0xFF;
												DATA0 = 0xFE;
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x00;


                                                ADR_H = Icon_Overlay_SP[Index] >> 17;
                                                ADR_M = Icon_Overlay_SP[Index] >> 9;
                                                ADR_L = Icon_Overlay_SP[Index] >> 1;
                                                ADR_INC = 0x01;
                                                RAMMODE = 0x8F;
                                                while ( !APP_ACK );
                                                DATA3 = JpegLaber_DGUSII_VP >> 8;
                                                DATA2 = JpegLaber_DGUSII_VP >> 0;
                                                DATA1 = Icon_Overlay_SP_X[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_X[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_Y[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_Y[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_L[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_L[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_H[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_H[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_Mode >> 8;
                                                DATA0 = Icon_Overlay_SP_Mode >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = 0x00;
                                                DATA2 = 0x80 | ( JpegLaber_DGUSII_VP >> 16 );
                                                DATA1 = 00;
                                                DATA0 = 00;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                RAMMODE = 0x00;

               


                                                if ( Icon_Num == 1 ||Icon_Num == 2 )
                                                {
                                                    Pic_Count[ ( Icon_Num-1 ) /2]++;
                                                }
                                                else
                                                {
													
													if(Icon_Num == (r11_now_choose_pic+5))
													{
														r11_pic_capture_flag = 1;
													}
                                                    Pic_Count[ ( Icon_Num-1 )]++;
                                                }
                                                data_write_f = 0;
                                                EX1_Start();
                                            }
                                        }
                                    }
                                    else
                                    {
                                        Miss_Align_16KB_Count++;
                                        data_write_f = 0x03;
                                        EX0 = 0;
                                        EX1 = 0;
                                    }
                                }
                                else
                                {
                                    if ( buf_tail == 0x8000 )
                                    {
                                        data_write_f = 2;
                                        {
                                            Judge_Packet_Count();
                                            if ( END_CRC != 0 )
                                            {
                                                CRC_ERR_Count++;
                                                data_write_f = 8;
                                                EX0 = 0;
                                                EX1 = 0;
                                            }
                                            else
                                            {

												ADR_H = JpegLaber_DGUSII_VP >> 17;
												ADR_M = JpegLaber_DGUSII_VP >> 9;
												ADR_L = JpegLaber_DGUSII_VP >> 1;
												ADR_INC = 0x00;
												
												RAMMODE = 0xAF;
												while ( !APP_ACK );
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x8F;
												DATA1 = DATA2;
												DATA3 = 0x5A;
												DATA2 = 0xA5;
												//DATA1 = 0xFF;
												DATA0 = 0xFE;
												APP_EN = 1;
												while ( APP_EN );
												RAMMODE = 0x00;


											

                                                ADR_H = Icon_Overlay_SP[Index] >> 17;
                                                ADR_M = Icon_Overlay_SP[Index] >> 9;
                                                ADR_L = Icon_Overlay_SP[Index] >> 1;
                                                ADR_INC = 0x01;
                                                RAMMODE = 0x8F;
                                                while ( !APP_ACK );
                                                DATA3 = JpegLaber_DGUSII_VP >> 8;
                                                DATA2 = JpegLaber_DGUSII_VP >> 0;
                                                DATA1 = Icon_Overlay_SP_X[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_X[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_Y[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_Y[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_L[Index] >> 8;
                                                DATA0 = Icon_Overlay_SP_L[Index] >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = Icon_Overlay_SP_H[Index] >> 8;
                                                DATA2 = Icon_Overlay_SP_H[Index] >> 0;
                                                DATA1 = Icon_Overlay_SP_Mode >> 8;
                                                DATA0 = Icon_Overlay_SP_Mode >> 0;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                DATA3 = 0x00;
                                                DATA2 = 0x80 | ( JpegLaber_DGUSII_VP >> 16 );
                                                DATA1 = 00;
                                                DATA0 = 00;
                                                APP_EN = 1;
                                                while ( APP_EN );
                                                RAMMODE = 0x00;





                                                if ( Icon_Num == 1 ||Icon_Num == 2 )
                                                {
                                                    Pic_Count[ ( Icon_Num-1 ) /2]++;
                                                }
                                                else
                                                {
													if(Icon_Num == (r11_now_choose_pic+5))
													{
														r11_pic_capture_flag = 1;
													}
                                                    Pic_Count[ ( Icon_Num-1 )]++;
                                                }
                                                data_write_f = 0;
                                                EX1_Start();
                                            }
                                        }
                                    }
                                    else
                                    {
                                        Miss_Align_16KB_Count++;
                                        data_write_f = 0x03;
                                        EX0 = 0;
                                        EX1 = 0;
                                    }
                                }
                            }
                            else
                            {
                                CRC_ERR_Count++;
                                data_write_f = 8;
                                EX0 = 0;
                                EX1 = 0;
                            }
                        }
                        else
                        {
                            Over_Max_16KB_Count++;
                            data_write_f = 9;
                            EX0 = 0;
                            EX1 = 0;
                        }
                    }
                    else
                    {
                        Other_Cmd_Count++;
                        data_write_f = 7;
                        EX0 = 0;
                        EX1 = 0;
                    }
                }
                else
                {
                    NotDeal_OverBuffer_Count++;
                    data_write_f = 4;
                    EX0 = 0;
                    EX1 = 0;
                }
            }
            break;
        }
        default:
        {
            Other_Cmd_Count++;
            data_write_f = 7;
            EX0 = 0;
            EX1 = 0;
            break;
        }
    }
    DATA0 = DATA0_Bak;
    DATA1 = DATA1_Bak;
    DATA2 = DATA2_Bak;
    DATA3 = DATA3_Bak;
    ADR_INC = ADR_INC_Bak;
    ADR_L = ADR_L_Bak;
    ADR_M = ADR_M_Bak;
    ADR_H = ADR_H_Bak;
    RAMMODE = RAMMODE_Bak;
    if ( RAMMODE )
    {
        while ( ~APP_ACK );
    }
}

#endif /* sysBEAUTY_MODE_ENABLED */
