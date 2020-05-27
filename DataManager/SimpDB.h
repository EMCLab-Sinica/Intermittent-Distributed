#ifndef SIMPDB_H
#define SIMPDB_H
/*
 * SimpDB.h
 *
 *  Created on: 2017�~7��12��
 *      Author: Meenchen
 *  Description: This simple DB is used to manage data and task snapshot(stacks)
 */
#include <../DataManager/maps.h>
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <config.h>

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
    consistent,
    working
} DataVersion_e;

typedef struct Data //two-version data structure
{
    int8_t id;
    void *ptr; //Should point to VM or NVM(depends on mode)
    DataVersion_e version;
    uint32_t size;
    uint32_t validationTS;
    uint8_t readTCBNum[MAXREAD]; //store 5 readers' TCB number
} data_t;

typedef struct Database
{
    data_t dataRecord[DB_MAX_OBJ];
    uint8_t dataRecordPos;    // end of array
} database_t;

/* for validation */
extern unsigned long timeCounter;

/* DB functions */
void DBConstructor();

void destructor();
int DBcommit(struct working *work, int size, int num);
void* DBread(uint8_t dataId);
void DBreadIn(void* to,int id);
void DBworking(struct working* wIn, int size, int id);

data_t *VMDBCreate(uint8_t size, uint8_t dataId);
void VMDBDelete(uint8_t dataId);

data_t *getDataRecord(uint8_t dataId);
void readRemoteDB(const TaskHandle_t *xFromTask, data_t *dataObj, uint8_t remoteAddr, uint8_t dataId);

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
