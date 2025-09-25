
#include <string.h>
#include "t5losconfig.h"
#include "sys.h"

#define DB_TEXT_MAX         26
#define DB_MAX_RECORD       0x100
#define DB_RECORD_SIZE      0x10
#define DB_START_ADDR       0x2000
#define DB_END_ADDR         (DB_START_ADDR + DB_MAX_RECORD * DB_RECORD_SIZE - 1)

typedef struct 
{
    uint32_t uuid;
    uint16_t number;
    char _data[DB_TEXT_MAX];
} database_t;


extern database_t database;

void DatabaseClear(void);


void DatabaseAddRecord(uint32_t uuid, uint16_t number, char *_data);

int DatabaseFindRecordByUUID(uint32_t uuid, database_t *out_record);


int DatabaseFindRecordByNumber(uint16_t number, database_t *out_record);


void DatabaseDeleteRecord(uint32_t uuid);


int DatabaseUpdateRecord(uint32_t uuid, uint16_t number, const char *_data);

void DatabaseScanTask(void);