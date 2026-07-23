#include "4G_air780e.h"
#include "core_json.h"
#include "string.h"

Air780E_Status air780e_status;
uint16_t air780e_num_rent[8] = {0};
uint8_t air780e_time_buffer[32]={0};
uint8_t air780e_mac_buffer[32]={0};
uint8_t air780e_ccid_buffer[32]={0};


static void Air780E_TimebufferHandle(void)
{
	air780e_time_buffer[0]=0x32;
    air780e_time_buffer[1]=0x30;
	air780e_time_buffer[2]=0x30+air780e_time_buffer[24+1]/10;
    air780e_time_buffer[3]=0x30+air780e_time_buffer[24+1]%10;
    air780e_time_buffer[4]='-';
	air780e_time_buffer[5]=0x30+air780e_time_buffer[24+2]/10;
    air780e_time_buffer[6]=0x30+air780e_time_buffer[24+2]%10;
    air780e_time_buffer[7]='-';
	air780e_time_buffer[8]=0x30+air780e_time_buffer[24+3]/10;
    air780e_time_buffer[9]=0x30+air780e_time_buffer[24+3]%10;
    air780e_time_buffer[10]=' ';
	air780e_time_buffer[11]=0x30+air780e_time_buffer[24+5]/10;
    air780e_time_buffer[12]=0x30+air780e_time_buffer[24+5]%10;
    air780e_time_buffer[13]=':';
	air780e_time_buffer[14]=0x30+air780e_time_buffer[24+6]/10;
    air780e_time_buffer[15]=0x30+air780e_time_buffer[24+6]%10;
    air780e_time_buffer[16]=':';
	air780e_time_buffer[17]=0x30+air780e_time_buffer[24+7]/10;
    air780e_time_buffer[18]=0x30+air780e_time_buffer[24+7]%10;
    air780e_time_buffer[19]=0x00;
}


static void Air780E_StartSendCmd(uint16_t cmd,uint16_t addr,uint16_t len)
{
    uint16_t write_param[3];
    write_param[0] = cmd;
    write_param[1] = addr;
    write_param[2] = len;
    write_dgus_vp(0x401,(uint8_t *)&write_param[0],3);
}


static void Air780E_InitServerPara(void)
{
    uint16_t write_param[10];
    switch(air780e_status.connect_type)
    {
        case air780eCONNECT_WEBSOCKET:
            air780e_status.url_start_addr = 0x7580;
            air780e_status.data_para_addr = 0x7500;
            air780e_status.data_json_addr = 0x7600;
            air780e_status.send_json_addr = 0x7800;
            air780e_status.download_addr = 0x7400;
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0;
            read_dgus_vp(0x2400,(uint8_t *)air780e_status.url_string,32);
            write_dgus_vp(air780eNORFLASH_RENT_ADDR+1, air780e_status.url_string,32);
            write_param[0] = 0x5aa5;
            write_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&write_param[0],1);
            DgusToFlash(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            break;
        case air780eCONNECT_HTTP:
            air780e_status.send_json_addr = 0x7800;
            air780e_status.data_json_addr = 0x7600;
            air780e_status.data_para_addr = 0x7500;
            air780e_status.download_addr = 0x7400;
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0;
            read_dgus_vp(0x2400,(uint8_t *)air780e_status.url_string,32);
            write_dgus_vp(air780eNORFLASH_RENT_ADDR+1, air780e_status.url_string,32);
            write_param[0] = 0x5aa5;
            write_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&write_param[0],1);
            DgusToFlash(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            break;
        case air780eCONNECT_MQTT:
            air780e_status.url_start_addr = 0x7580;
            air780e_status.data_para_addr = 0x7500;
            air780e_status.data_json_addr = 0x7600;
            air780e_status.send_json_addr = 0x7800;
            air780e_status.mqtt_topic_addr = 0x75c0;
            air780e_status.download_addr = 0x7400;
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0;
            read_dgus_vp(0x2400,(uint8_t *)air780e_status.url_string,32);
            read_dgus_vp(0x2600,(uint8_t *)air780e_status.machine_no_rx,32);
            read_dgus_vp(0x2700,(uint8_t *)air780e_status.machine_no_tx,32);
            write_dgus_vp(air780eNORFLASH_RENT_ADDR+1, air780e_status.url_string,32);
            write_dgus_vp(air780eNORFLASH_RENT_ADDR+1+32, air780e_status.machine_no_rx,32);
            write_dgus_vp(air780eNORFLASH_RENT_ADDR+1+32+32, air780e_status.machine_no_tx,32);
            write_param[0] = 0x5aa5;
            write_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&write_param[0],1);
            DgusToFlash(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            break;
        default:
            break;
    }
}


static void Air780E_ChangeServerPara(uint8_t type)
{
    uint16_t write_param[10],read_param[10];
    write_dgus_vp(0x2000,(uint8_t *)&type,1);
    memset((uint8_t *)&air780e_status,0,sizeof(air780e_status));
    air780e_status.connect_type = type;
    switch(air780e_status.connect_type)
    {
        case air780eCONNECT_DWINCLOUD:
            write_param[0] = 0x005a;
            write_dgus_vp(0x498+3,(uint8_t *)&write_param[0],1);
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0xff;
        break;
        case air780eCONNECT_WEBSOCKET:
            FlashToDgus(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            read_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&read_param[0],1);
            if(read_param[0] == 0x5aa5)
            {
                read_dgus_vp(air780eNORFLASH_RENT_ADDR+1,air780e_status.url_string,32);
            }else
            {
                CopyAsciiString(air780e_status.url_string,"ws://220.248.173.137:3009",0);
            }
            write_dgus_vp(0x2400,air780e_status.url_string,32);
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0xff;
        break;
        case air780eCONNECT_HTTP:
            FlashToDgus(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            read_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&read_param[0],1);
            if(read_param[0] == 0x5aa5)
            {
                read_dgus_vp(air780eNORFLASH_RENT_ADDR+1,air780e_status.url_string,32);
            }else
            {
                CopyAsciiString(air780e_status.url_string,"http://test.dwinhmi.com.cn:8882/720e",0);
            }
            write_dgus_vp(0x2400,air780e_status.url_string,32);
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0xff;
        break;
        case air780eCONNECT_MQTT:
            FlashToDgus(flashMAIN_BLOCK_ORDER,0x0000,air780eNORFLASH_RENT_ADDR,98);
            read_dgus_vp(air780eNORFLASH_RENT_ADDR,(uint8_t *)&read_param[0],1);
            if(read_param[0] == 0x5aa5)
            {
                read_dgus_vp(air780eNORFLASH_RENT_ADDR+1,air780e_status.url_string,32);
                read_dgus_vp(air780eNORFLASH_RENT_ADDR+1+32,air780e_status.machine_no_rx,32);
                read_dgus_vp(air780eNORFLASH_RENT_ADDR+1+32+32,air780e_status.machine_no_tx,32);
            }else
            {
                CopyAsciiString(air780e_status.url_string,"47.96.239.113:1883",0);
                CopyAsciiString(air780e_status.machine_no_rx,"dwin/rx/720e/",0);
                CopyAsciiString(air780e_status.machine_no_tx,"dwin/tx/720e/",0);
            }
            write_dgus_vp(0x2400,air780e_status.url_string,32);
            write_dgus_vp(0x2600,air780e_status.machine_no_rx,32);
            write_dgus_vp(0x2700,air780e_status.machine_no_tx,32);
            air780e_status.connect_flag = 0;
            air780e_status.connect_step = 0xff;
        break;
    }
}


static void Air780E_ServerConnectTask(void)
{
    uint16_t read_param[10],write_param[10];
    uint8_t json_cmd[64] = {0},i;
    uint8_t send_data[126];
    uint8_t air780e_receive_buf[DATA_MAX_LEN];
    switch(air780e_status.connect_type)
    {
        case air780eCONNECT_DWINCLOUD:
            read_dgus_vp(0x0761,(uint8_t *)&air780e_num_rent[0],1);
        break;
        case air780eCONNECT_WEBSOCKET:
            if(air780e_status.connect_flag)
            {
                read_dgus_vp(0x0761,(uint8_t *)&air780e_num_rent[0],1);
            }else
            {
                air780e_num_rent[0] = 0;
            }
        break;
        case air780eCONNECT_HTTP:
            air780e_num_rent[0] = 0;
        break;
        case air780eCONNECT_MQTT:
            if(air780e_status.connect_flag)
            {
                read_dgus_vp(air780e_status.data_json_addr,(uint8_t *)&air780e_num_rent[0],1);
            }else
            {
                air780e_num_rent[0] = 0;
            }
        break;
        default:
        break;
    }
    write_dgus_vp(0x2001,(uint8_t *)&air780e_num_rent[0],1);
    read_dgus_vp(0x401,(uint8_t*)&read_param[0],1);
    if(read_param[0] != 0)
    {
        return;
    }
    if(air780e_status.connect_flag)
    {
        if(air780e_status.connect_type == air780eCONNECT_MQTT)
        {
            read_dgus_vp(air780e_status.data_json_addr+1,(uint8_t *)&read_param[0],1);
        }else
        {
            read_dgus_vp(air780e_status.data_json_addr,(uint8_t *)&read_param[0],1);
        }
        if((air780e_status.connect_type == air780eCONNECT_MQTT && read_param[0] != 0) || 
            (air780e_status.connect_type == air780eCONNECT_WEBSOCKET && read_param[0] == 0x6aa6) ||
            (air780e_status.connect_type == air780eCONNECT_HTTP && read_param[0] == 0x5aa5))
        {
            read_dgus_vp(air780e_status.data_json_addr+1,(uint8_t *)&read_param[1],1);
            if(read_param[1] != 0)
            {
                if(read_param[1] > DATA_MAX_LEN)
                {
                    __NOP();
                }else
                {
                    read_dgus_vp(air780e_status.data_json_addr+2,air780e_receive_buf,read_param[1]);
                    switch(air780e_status.connect_type)
                    {
                        case air780eCONNECT_WEBSOCKET:
                        case air780eCONNECT_HTTP:
                            if('{' == air780e_receive_buf[0])
                            {
                                if(JSONSearchToArray(air780e_receive_buf, strlen(air780e_receive_buf),
                                                     "type", sizeof("type") - 1U,
                                                     (char *)json_cmd, sizeof(json_cmd)) == JSONSuccess)
                                {
                                    if(strcmp((char *)json_cmd,"up_date") == 0)
                                    {
                                        memset(json_cmd,0,sizeof(json_cmd));
                                        if(JSONSearchToArray(air780e_receive_buf, strlen(air780e_receive_buf),
                                                             "content", sizeof("content") - 1U,
                                                             (char *)json_cmd,
                                                             sizeof(json_cmd) - 1U) == JSONSuccess)
                                        {
                                            //在末尾补上两个0x00
                                            json_cmd[strlen(json_cmd)] = 0x00;
                                            json_cmd[strlen(json_cmd) + 1] = 0x00;
                                            write_dgus_vp(0x2c00,json_cmd,strlen(json_cmd) + 2);

                                        }
                                    }else if(strcmp((char *)json_cmd,"dwin_download_vedio") == 0)
                                    {
                                        __NOP();
                                    }
                                }
                            }
                            break;
                        case air780eCONNECT_MQTT:
                            if(JSONSearchToArray(air780e_receive_buf, strlen(air780e_receive_buf),
                                                 (const char *)air780e_status.machine_no_rx,
                                                 strlen(air780e_status.machine_no_rx),
                                                 (char *)json_cmd, sizeof(json_cmd)) == JSONSuccess)
                            {
                                if(JSONSearchToArray(air780e_receive_buf, strlen(air780e_receive_buf),
                                                     "type", sizeof("type") - 1U,
                                                     (char *)json_cmd, sizeof(json_cmd)) == JSONSuccess)
                                {
                                    if(strcmp((char *)json_cmd,"up_date") == 0)
                                    {
                                        memset(json_cmd,0,sizeof(json_cmd));
                                        if(JSONSearchToArray(air780e_receive_buf, strlen(air780e_receive_buf),
                                                             "content", sizeof("content") - 1U,
                                                             (char *)json_cmd,
                                                             sizeof(json_cmd) - 1U) == JSONSuccess)
                                        {
                                            //在末尾补上两个0x00
                                            json_cmd[strlen(json_cmd)] = 0x00;
                                            json_cmd[strlen(json_cmd) + 1] = 0x00;
                                            write_dgus_vp(0x2c00,json_cmd,strlen(json_cmd) + 2);

                                        }
                                    }else if(strcmp((char *)json_cmd,"dwin_download_vedio") == 0)
                                    {
                                        __NOP();
                                    }
                                }
                            }
                        default:
                        break;
                    }
                    Air780E_TimebufferHandle();
                }
                read_param[0] = 0;
                if(air780e_status.connect_type == 3)
                {
                    write_dgus_vp(air780e_status.data_json_addr+1,(uint8_t *)&read_param[0],1);
                }else
                {
                    write_dgus_vp(air780e_status.data_json_addr,(uint8_t *)&read_param[0],1);
                }
            }
        }
    }else
    {
        if(air780e_status.connect_step == 0xff)
        {
            return;
        }
        switch(air780e_status.connect_type)
        {
            case air780eCONNECT_WEBSOCKET:
                air780e_status.connect_step++;
                if(air780e_status.connect_step == 1)
                {
                    write_param[0] = air780e_status.download_addr;
                    write_dgus_vp(air780e_status.data_para_addr,(uint8_t *)&write_param[0],1);
                    Air780E_StartSendCmd(0x2ad1,air780e_status.data_para_addr,1);
                }else if(air780e_status.connect_step == 2)
                {
                    for(i=0;i<128;i++)
                    {
                        if(air780e_status.url_string[i] == 0x00)
                        {
                            break;
                        }
                    }
                    write_dgus_vp(air780e_status.url_start_addr,air780e_status.url_string,64);
                    Air780E_StartSendCmd(0x2aa1,air780e_status.url_start_addr,i);
                }else if(air780e_status.connect_step == 3)
                {
                    write_param[0] = air780e_status.data_json_addr;
                    write_param[1] = DATA_MAX_LEN;
                    write_dgus_vp(air780e_status.data_para_addr,(uint8_t *)&write_param[0],2);
                    Air780E_StartSendCmd(0x2aa2,air780e_status.data_para_addr,2);
                }else if(air780e_status.connect_step == 4)
                {
                    air780e_status.connect_step = 0xff;
                    air780e_status.connect_flag = 1;
                }
            break;
            case air780eCONNECT_HTTP:
                air780e_status.connect_step++;
                if(air780e_status.connect_step == 1)
                {
                    write_param[0] = air780e_status.download_addr;
                    write_dgus_vp(air780e_status.data_para_addr,(uint8_t *)&write_param[0],1);
                    Air780E_StartSendCmd(0x2ad1,air780e_status.data_para_addr,1);
                }else if(air780e_status.connect_step == 2)
                {
                    read_dgus_vp(0x4b0,air780e_mac_buffer,8);
                    air780e_mac_buffer[15] = 0;
                    if(air780e_mac_buffer[0] == 0x00)
                    {
                        return;
                    }
                    read_dgus_vp(0x4c0,air780e_ccid_buffer,10);
                    air780e_ccid_buffer[20] = 0;
                    read_dgus_vp(0x4ac,(uint8_t *)&air780e_time_buffer[24],4);
                    Air780E_TimebufferHandle();
                    air780e_status.connect_step = 0xff;
                    air780e_status.connect_flag = 1;
                }
            break;
            case air780eCONNECT_MQTT:
                air780e_status.connect_step++;
                if(air780e_status.connect_step == 1)
                {
                    write_param[0] = air780e_status.download_addr;
                    write_dgus_vp(air780e_status.data_para_addr,(uint8_t *)&write_param[0],1);
                    Air780E_StartSendCmd(0x2ad1,air780e_status.data_para_addr,1);
                }else if(air780e_status.connect_step == 2)
                {
                    for(i=0;i<128;i++)
                    {
                        if(air780e_status.url_string[i] == 0x00)
                        {
                            break;
                        }
                    }
                    write_dgus_vp(air780e_status.url_start_addr,air780e_status.url_string,64);
                    Air780E_StartSendCmd(0x2ac1,air780e_status.url_start_addr,i);
                }else if(air780e_status.connect_step == 3)
                {
                    air780e_status.mqtt_clean_start = 0;
                    write_param[0] = air780e_status.data_json_addr;
                    write_param[1] = DATA_MAX_LEN;
                    write_param[2] = air780e_status.mqtt_clean_start;
                    write_dgus_vp(air780e_status.data_para_addr,(uint8_t *)&write_param[0],3);
                    Air780E_StartSendCmd(0x2ac2,air780e_status.data_para_addr,3);
                }else if(air780e_status.connect_step == 4)
                {
                    read_dgus_vp(0x4b0,air780e_mac_buffer,8);
                    air780e_mac_buffer[15] = 0;
                    if(air780e_mac_buffer[0] == 0x00)
                    {
                        return;
                    }
                    read_dgus_vp(0x4c0,air780e_ccid_buffer,10);
                    air780e_ccid_buffer[20] = 0;
                    read_dgus_vp(0x4ac,(uint8_t *)&air780e_time_buffer[24],4);
                    Air780E_TimebufferHandle();
                    write_param[0] = CopyAsciiString(send_data,air780e_status.machine_no_tx,0);
                    write_param[1] = CopyAsciiString(send_data,air780e_mac_buffer,write_param[0]);
                    send_data[write_param[1]+1] = 0x00;
                    write_dgus_vp(air780e_status.mqtt_topic_addr,send_data,(write_param[1]+1)/2);
                    Air780E_StartSendCmd(0x2ac3,air780e_status.mqtt_topic_addr,(write_param[1]+1)/2);
                }else if(air780e_status.connect_step == 5)
                {
                    air780e_status.connect_step = 0xff;
                    air780e_status.connect_flag = 1;
                }
            break;
        }
        if(air780e_status.connect_flag)
        {
            read_dgus_vp(0x4b0,air780e_mac_buffer,8);
            air780e_mac_buffer[15] = 0;
            if(air780e_mac_buffer[0] == 0x00)
            {
                return;
            }
            read_dgus_vp(0x4c0,air780e_ccid_buffer,10);
            air780e_ccid_buffer[20] = 0;
            read_dgus_vp(0x4ac,(uint8_t *)&air780e_time_buffer[24],4);
            Air780E_TimebufferHandle();
        }
    }
}


static void Air780E_SendServerData()
{
    uint16_t now_len = 0,zero_value = 0,http_head_len = 0;
    uint8_t air780e_send_data[2048],tmp_arr[128];
    // if(air780e_status.connect_flag == 0)
    // {
    //     return;
    // }
    now_len = 0;
    switch(air780e_status.connect_type)
    {
        case air780eCONNECT_WEBSOCKET:
        break;
        case air780eCONNECT_HTTP:
            air780e_send_data[0] = 0x00;
            air780e_send_data[1] = 0x00;
            now_len = 2;
            air780e_send_data[now_len] = 1;  /*paratype 0普通字符串，1json字符串*/
            now_len++;
            air780e_send_data[now_len] = 1;  /*methord  0 get，1 post*/
            now_len++;
            now_len = CopyAsciiString(air780e_send_data,air780e_status.url_string,now_len);
            air780e_send_data[now_len] = 0x00;
            air780e_send_data[now_len+1] = 0x00;
            http_head_len = now_len;
            now_len += 2;
        break;
        case air780eCONNECT_MQTT:
            now_len = 0;
            now_len = CopyAsciiString(air780e_send_data,air780e_status.machine_no_tx,now_len);
            now_len = CopyAsciiString(air780e_send_data,air780e_mac_buffer,now_len);
            now_len = CopyAsciiString(air780e_send_data,"<==>",now_len);
        break;
        default:
        break;
    }
    air780e_send_data[now_len++] = '{';
    now_len = CopyAsciiString(air780e_send_data,"\"type\"\:",now_len);
    now_len = CopyAsciiString(air780e_send_data,"\"up_date\"",now_len);
    air780e_send_data[now_len++] = ',';
    now_len = CopyAsciiString(air780e_send_data,"\"target\"\:",now_len);
    now_len = CopyAsciiString(air780e_send_data,"\"server\"",now_len);
    air780e_send_data[now_len++] = ',';
    now_len = CopyAsciiString(air780e_send_data,"\"content\"\:",now_len);
    air780e_send_data[now_len++] = '"';
    now_len = CopyAsciiString(air780e_send_data,"ccid",now_len);
    air780e_send_data[now_len++] = '=';
    now_len = CopyAsciiString(air780e_send_data,air780e_ccid_buffer,now_len);
    air780e_send_data[now_len++] = '&';
    now_len = CopyAsciiString(air780e_send_data,"mac",now_len);
    air780e_send_data[now_len++] = '=';
    now_len = CopyAsciiString(air780e_send_data,air780e_mac_buffer,now_len);
    air780e_send_data[now_len++] = '&';
    now_len = CopyAsciiString(air780e_send_data,"tim",now_len);
    air780e_send_data[now_len++] = '=';
    read_dgus_vp(0x4ac,(uint8_t *)&air780e_time_buffer[24],4);
    Air780E_TimebufferHandle();
    now_len = CopyAsciiString(air780e_send_data,air780e_time_buffer,now_len);
    air780e_send_data[now_len++] = '&';
    now_len = CopyAsciiString(air780e_send_data,"note",now_len);
    air780e_send_data[now_len++] = '=';
    read_dgus_vp(0x2800,tmp_arr,64);
    now_len = CopyAsciiString(air780e_send_data,tmp_arr,now_len);
    air780e_send_data[now_len++] = '"';
    air780e_send_data[now_len++] = '}';
    switch(air780e_status.connect_type)
    {
        case air780eCONNECT_WEBSOCKET:
            write_dgus_vp(air780e_status.send_json_addr,air780e_send_data,(now_len+1)/2);
            Air780E_StartSendCmd(0x2aa3,air780e_status.send_json_addr,(now_len+1)/2);
        break;
        case air780eCONNECT_HTTP:
        /**
         * data: len(2) + paramtype(1) + method(1) + URLlen(2) + url +paramlen(2) + param + maxlen(2) + state address(2) + flag address(2) + result type(2) + result len address(2) + result address(2) + bufer_max_id(2)+ headerlen(2) + header
         * header的格式为json，header支持设置多个 例子：{"Content-Type":"application/json","Content-Length":"128"}
         */
            air780e_send_data[http_head_len] = (now_len - http_head_len - 2)>>8;
            air780e_send_data[http_head_len+1] = (now_len - http_head_len - 2)&0xff;
            air780e_send_data[now_len++] = 0x10;
            air780e_send_data[now_len++] = 0x00;   /*maxlen最大接收长度*/
            air780e_send_data[now_len++] = 0xc0;
            air780e_send_data[now_len++] = 0x00;   /*state addr*/
            air780e_send_data[now_len++] = air780e_status.data_json_addr>>8;
            air780e_send_data[now_len++] = air780e_status.data_json_addr&0xff;   /*HttpFlag;flag addr*/
            air780e_send_data[now_len++] = 0x75;
            air780e_send_data[now_len++] = 0xff;  /*result type addr  还是 reponse time? 未知*/
            air780e_send_data[now_len++] = (air780e_status.data_json_addr+1)>>8;
            air780e_send_data[now_len++] = (air780e_status.data_json_addr+1)&0xff;   /*result len addr*/
            air780e_send_data[now_len++] = (air780e_status.data_json_addr+2)>>8;
            air780e_send_data[now_len++] = (air780e_status.data_json_addr+2)&0xff;   /*result addr  ,接收的json信息,保存在从这个地址开始的区域*/
            air780e_send_data[0] = now_len>>8;
            air780e_send_data[1] = now_len&0xff;
            write_dgus_vp(air780e_status.send_json_addr,air780e_send_data,(now_len+1)/2);
            Air780E_StartSendCmd(0x7aad,air780e_status.send_json_addr,(now_len+1)/2);
        break;
        case air780eCONNECT_MQTT:
            write_dgus_vp(air780e_status.send_json_addr,air780e_send_data,(now_len+1)/2);
            Air780E_StartSendCmd(0x2ac4,air780e_status.send_json_addr,(now_len+1)/2);
        break;
        default:
        break;
    }

}


static void Air780E_ValueScanTask(void)
{
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
    #if sysDGUS_AUTO_UPLOAD_ENABLED || uartTA_PROTOCOL_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED ||uartTA_PROTOCOL_ENABLED */

    read_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    if(dgus_value == 0x0001)
    {
        Air780E_ChangeServerPara(air780eCONNECT_WEBSOCKET);
        write_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    }else if(dgus_value == 0x0002)
    {
        Air780E_ChangeServerPara(air780eCONNECT_HTTP);
        write_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    }else if(dgus_value == 0x0003)
    {
        Air780E_ChangeServerPara(air780eCONNECT_MQTT);
        write_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    }else if(dgus_value == 0x0004)
    {
        Air780E_InitServerPara();
        write_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    }else if(dgus_value == 0x0005)
    {
        Air780E_SendServerData();
        write_dgus_vp(AIR780E_VALUE_SCAN_ADDR, (uint8_t *)&dgus_value, 1);
    }
}


void Air780E_Task(void)
{
    Air780E_ValueScanTask();
    Air780E_ServerConnectTask();
}
