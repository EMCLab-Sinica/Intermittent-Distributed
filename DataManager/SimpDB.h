#ifndef SIMPDB_H
#define SIMPDB_H
/*
 * SimpDB.h
 *
 *  Created on: 2017�~7��12��
 *      Author: Meenchen
 *  Description: This simple DB is used to manage data and task snapshot(stacks)
 */
#include <DataManager/maps.h>
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <config.h>
#include "Queue.h"

#define VM_WORKING_SIZE 3072 // working space in VM

// used for validation: Task t with Task's TCB = WSRTCB[i], SRBegin[NUMTASK] = min(writer's begin), WSRValid[NUMTASK] = 1
static unsigned long WSRBegin[NUMTASK]; //The "begin time of every commit operation" for an object "read by task i" is saved in WSRBegin[i]
static unsigned short WSRTCB[NUMTASK];
static unsigned char WSRValid[NUMTASK];

// deprecated
struct working{//working space of data for tasks
    void* address;
    int loc;//1 stands for SRAM, 0 stands for�@NVM
    int id;//-1 for create
};

typedef enum DataVersion
{
    consistent,             // consistent version in NVM
    duplicated,             // consistent version in VM
    working,                // working version in VM
    modified                // working version in NVM
} DataVersion_e;

typedef struct Data //two-version data structure
{
    int8_t id;
    int8_t owner;  // ownerAddr of the data
    DataVersion_e version;
    void *ptr; //Should point to VM or NVM(depends on mode)
    uint32_t size;
    uint64_t validationTS;
    uint8_t readTCBNum[MAXREAD]; //store 5 readers' TCB number

} data_t;

typedef struct Database
{
    data_t dataRecord[DB_MAX_OBJ];
    uint8_t dataRecordPos;       // end of array
    uint8_t dataIdAutoIncrement; // auto increment for NVM DB
} database_t;

/* for validation */
extern unsigned long timeCounter;

/* DB functions */
void DBConstructor();
void DBDestructor();

data_t *getDataRecord(uint8_t owner, uint8_t dataId);
data_t *readDB(uint8_t dataId);
data_t readInLocalDB(uint8_t dataId, void* destDataPtr);
data_t readRemoteDB(const TaskHandle_t const *xFromTask, uint8_t remoteAddr,
                    uint8_t dataId, void *destDataPtr, uint8_t size);

data_t createWorkingSpace();
data_t *createVMDBobject(uint8_t size);

int8_t commitLocalDB(data_t *data, uint32_t size);


void * getStackVM(int taskID);
void * getTCBVM(int taskID);
/* functions for validation*/
void registerTCB(int id);
void unresgisterTCB(int id);

/* internal functions */
static unsigned long min(unsigned long a, unsigned long b){
    if (a > b)
        return b;
    else
        return a;
}

#endif // SIMPDB_H
