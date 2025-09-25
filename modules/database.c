#include <string.h>
#include "database.h"

database_t database;

void DatabaseClear(void)
{
		int i;
        memset((uint8_t *)&database, 0, sizeof(database_t));
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        write_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
    }
    DgusToFlash(flashMAIN_BLOCK_ORDER, 0, DB_START_ADDR, DB_MAX_RECORD * DB_RECORD_SIZE);
}

void DatabaseAddRecord(uint32_t uuid, uint16_t number, char *_data)
{
		int i;
        database_t tmpdata;
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        memset((uint8_t *)&tmpdata, 0, sizeof(database_t));
        read_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&tmpdata, DB_RECORD_SIZE);
        if (tmpdata.uuid == 0)
        {
            database.uuid = uuid;
            database.number = number;
            memcpy(database._data, _data, DB_TEXT_MAX - 1);
            write_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
            DgusToFlash(flashMAIN_BLOCK_ORDER, 0, DB_START_ADDR + i * DB_RECORD_SIZE, DB_RECORD_SIZE);
            return;
        }
    }
}


int DatabaseFindRecordByUUID(uint32_t uuid, database_t *out_record)
{
		int i;
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        memset((uint8_t *)&database, 0, sizeof(database_t));
        read_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
        if (database.uuid == uuid)
        {
            if (out_record != NULL)
            {
                memcpy(out_record, &database, sizeof(database_t));
            }
            return 1; 
        }
    }
    return 0; 
}


int DatabaseFindRecordByNumber(uint16_t number, database_t *out_record)
{
		int i;
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        memset((uint8_t *)&database, 0, sizeof(database_t));
        read_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
        if (database.number == number)
        {
            if (out_record != NULL)
            {
                memcpy(out_record, &database, sizeof(database_t));
            }
            return 1; 
        }
    }
    return 0; 
}



void DatabaseDeleteRecord(uint32_t uuid)
{
		int i;
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        memset((uint8_t *)&database, 0, sizeof(database_t));
        read_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
        if (database.uuid == uuid)
        {
            memset((uint8_t *)&database, 0, sizeof(database_t));
            write_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
            DgusToFlash(flashMAIN_BLOCK_ORDER, 0, DB_START_ADDR, DB_MAX_RECORD * DB_RECORD_SIZE);
            return;
        }
    }
}



int DatabaseUpdateRecord(uint32_t uuid, uint16_t number, const char *_data)
{
		int i;
    for (i = 0; i < DB_MAX_RECORD; i++)
    {
        memset((uint8_t *)&database, 0, sizeof(database_t));
        read_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
        if (database.uuid == uuid)
        {
            database.number = number;
            if (_data != NULL)
            {
                memset(database._data, 0, sizeof(database._data));
                strncpy(database._data, _data, DB_TEXT_MAX - 1);
            }
            write_dgus_vp(DB_START_ADDR + i * DB_RECORD_SIZE, (uint8_t *)&database, DB_RECORD_SIZE);
            DgusToFlash(flashMAIN_BLOCK_ORDER, 0, DB_START_ADDR, DB_MAX_RECORD * DB_RECORD_SIZE);
            return 1; 
        }
    }
    return 0; 
}


void DatabaseScanTask(void)
{
    #define DATABASE_SCAN_ADDRESS     0x1000
    const uint16_t uint16_port_zero = 0;
    uint16_t dgus_value;
  
    #if sysDGUS_AUTO_UPLOAD_ENABLED
    DgusAutoUpload();
    #endif /* sysDGUS_AUTO_UPLOAD_ENABLED */

    read_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&dgus_value, 1);
    if(dgus_value == 0x5a01)
	{
        DatabaseClear();
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a02)
    {
        database.uuid = 0x03;
        database.number = 12345;
        memcpy(database._data, "Hello, World!\0\0", DB_TEXT_MAX - 1);
        DatabaseAddRecord(database.uuid, database.number, database._data);
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a03)
    {
        database.uuid = 0x04;
        database.number = 54321;
        memcpy(database._data, "zheshiuuid=4deshuju\0\0", DB_TEXT_MAX - 1);
        DatabaseAddRecord(database.uuid, database.number, database._data);
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a04)
    {
        database.uuid = 0x03;
        if(DatabaseFindRecordByUUID(database.uuid, &database))
        {
            database.number += 1;
            DatabaseUpdateRecord(database.uuid, database.number, NULL);
        }
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a05)
    {
        database.uuid = 0x04;
        if(DatabaseFindRecordByUUID(database.uuid, &database))
        {
            database.number -= 1;
            DatabaseUpdateRecord(database.uuid, database.number, NULL);
        }
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a06)
    {
        database.uuid = 0x03;
        DatabaseDeleteRecord(database.uuid);
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }else if(dgus_value == 0x5a07)
    {
        database.uuid = 0x04;
        DatabaseDeleteRecord(database.uuid);
        write_dgus_vp(DATABASE_SCAN_ADDRESS, (uint8_t *)&uint16_port_zero, 1);
    }
}

