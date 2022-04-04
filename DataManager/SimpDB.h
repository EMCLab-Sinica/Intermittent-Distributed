#ifndef SIMPDB_H
#define SIMPDB_H
#include <DataManager/maps.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdint.h>
#include <config.h>
#include <stdbool.h>

#define VM_WORKING_SIZE 3072 // working space in VM
#define MAX_DB_OBJ_SIZE 4 // 4bytes for each data object

typedef enum DataVersion
{
    consistent,             // consistent version in NVM
    duplicated,             // consistent version in VM
    working,                // working version in VM
    modified                // working version in NVM
} DataVersion_e;

typedef enum DBSearchMode
{
    all,
    vmdb,
    nvmdb
} DBSearchMode_e;

typedef struct DataUUID
{
    uint8_t owner: 4;
    uint8_t id: 4;

} DataUUID_t;

// Data info without real content, used for packet transmission
typedef struct DataHeader
{
    DataUUID_t dataId;
    DataVersion_e version: 8;

} DataHeader_t;

// Data without validation info, used for packet transmission
typedef struct DataTransPacket
{
    DataUUID_t dataId;
    DataVersion_e version;
    uint8_t size;
    uint8_t content[MAX_DB_OBJ_SIZE];
    uint32_t begin;

} DataTransPacket_t;

typedef struct Data
{
    DataUUID_t dataId;
    DataVersion_e version;
    uint8_t size;
    void *ptr; // points to the data location
    TaskUUID_t validationLock;
    uint32_t begin;

} Data_t;

typedef struct Database
{
    Data_t dataRecord[MAX_DB_OBJ];
    uint8_t dataRecordCount;       // end of array
    uint8_t dataIdAutoIncrement; // auto increment for NVM DB

} Database_t;

typedef struct taskAccessLog
{
    DataUUID_t readSet[MAX_TASK_READ_OBJ];
} TaskAccessLog_t;

/* for validation */
extern uint32_t  timeCounter;

/* DB functions */
void NVMDBConstructor();
void VMDBConstructor();
void DBDestructor();

uint32_t getDataBegin(DataUUID_t dataId);
Data_t *getDataRecord(DataUUID_t dataId, DBSearchMode_e mode);
Data_t readLocalDB(uint8_t id, void* destDataPtr, uint8_t size);
Data_t readRemoteDB(TaskUUID_t taskId, const TaskHandle_t const *xFromTask, uint8_t remoteAddr,
                    uint8_t id, void *destDataPtr, uint8_t size);
void writeData(Data_t *data, void *value);

bool writeDataObjectInt(uint8_t task_id, Data_t* data, int value);
Data_t createWorkingSpace(void *dataPtr, uint32_t size);
Data_t *createVMDBobject(uint8_t size);

DataUUID_t commitLocalDB(TaskUUID_t taskUUID, Data_t *data);


void * getStackVM(int taskID);
void * getTCBVM(int taskID);
/* functions for validation*/
void registerTCB(int id);
void unresgisterTCB(int id);

/* internal functions */
static uint32_t min(uint32_t a, uint32_t b)
{
    if (a > b)
        return b;
    else
        return a;
}

static uint32_t max(uint32_t a, uint32_t b)
{
    if (a > b)
        return a;
    else
        return b;
}

static bool dataIdEqual(DataUUID_t *lhs, DataUUID_t *rhs)
{
    return (lhs->owner == rhs->owner) && (lhs->id == rhs->id);
}

static bool taskIdEqual(TaskUUID_t *lhs, TaskUUID_t *rhs)
{
    return (lhs->nodeAddr == rhs->nodeAddr) && (lhs->id == rhs->id);
}

#endif // SIMPDB_H
