/*
 * SimpDB.cpp
 *
 *  Created on: 2017�~7��12��
 *      Author: WeiMingChen
 */

#include <DataManager/SimpDB.h>
#include <FreeRTOS.h>
#include <stdio.h>
#include <task.h>
#include "Recovery.h"
#include "RFHandler.h"
#include "mylist.h"
#include "myuart.h"

#define DEBUG 1 // control debug message

#pragma NOINIT(NVMDatabase)
static database_t NVMDatabase;

#pragma DATA_SECTION(dataId, ".map") //id for data labeling
static int dataId;

/* Half of RAM for caching (0x2C00~0x3800) */
#pragma location = 0x2C00 //Space for working at SRAM
static unsigned char VMWorkingSpace[VM_WORKING_SIZE];

#pragma DATA_SECTION(VMWorkingSpacePos, ".map") //RR pointer for working at SRAM
static unsigned int VMWorkingSpacePos;

/* space in VM for working versions*/
static database_t VMDatabase;

extern MyList_t *dataTransferLogList;

/*
 * DBConstructor(): initialize all data structure in the database
 * parameters: none
 * return: none
 * */
const uint8_t testValue = 7;
void DBConstructor(){
    // create logging
    dataTransferLogList = makeList();
    init();
    NVMDatabase.dataRecordPos = 0;
    for(uint8_t i = 0; i < DB_MAX_OBJ; i++){
        NVMDatabase.dataRecord[i].id = -1;
        NVMDatabase.dataRecord[i].ptr = NULL;
        NVMDatabase.dataRecord[i].size = 0;
        for (uint8_t j = 0; j < MAXREAD; j++)
            NVMDatabase.dataRecord[i].readTCBNum[j] = 0;
    }
    dataId = 0;
    VMWorkingSpacePos = 0;
    for(uint8_t i = 0; i < NUMTASK; i++)
        WSRValid[i] = 0;

    // insert for test
    NVMDatabase.dataRecord[0].id = 6;
    NVMDatabase.dataRecord[0].ptr = (void*)&testValue;
    NVMDatabase.dataRecord[0].size = sizeof(testValue);
    NVMDatabase.dataRecordPos = 1;
}


/*
 * DBread(int Did):
 * parameters: id of the data
 * return: the pointer of data, NULL for failure
 * */
void* DBread(uint8_t dataId){
    if (DEBUG)
        print2uart("DBRead: request dataId: %d\n", dataId);

    // read VM DB
    for (uint8_t i = 0; i < VMDatabase.dataRecordPos; i++)
    {
        if (dataId == VMDatabase.dataRecord[i].id)
        {
            if (DEBUG)
                print2uart("DBRead: dataId: %d found in VM DB\n", dataId);

            return VMDatabase.dataRecord[i].ptr;
        }
    }

    // read NVM DB
    for (uint8_t i = 0; i < NVMDatabase.dataRecordPos; i++)
    {
        if (dataId == NVMDatabase.dataRecord[i].id)
        {
            if (DEBUG)
                print2uart("DBRead: dataId: %d found in NVM DB\n", dataId);

            return accessData(i);
        }
    }
    /* Validation: save the reader's TCBNumber for committing tasks */
    //may need to be protected by some mutex

    if (DEBUG)
        print2uart("DBRead: dataId: %d not found\n", dataId);
    return NULL;
}

data_t *getDataRecord(uint8_t dataId)
{
    if (DEBUG)
        print2uart("getDataRecord: request dataId: %d\n", dataId);

    for (uint8_t i = 0; i < VMDatabase.dataRecordPos; i++)
    {
        if (dataId == VMDatabase.dataRecord[i].id)
        {
            if (DEBUG)
                print2uart("getDataRecord: dataId: %d found in VM DB\n", dataId);

            return VMDatabase.dataRecord+i;
        }
    }

    // read NVM DB
    for (uint8_t i = 0; i < NVMDatabase.dataRecordPos; i++)
    {
        if (dataId == NVMDatabase.dataRecord[i].id)
        {
            if (DEBUG)
                print2uart("getDataRecord: dataId: %d found in NVM DB\n", dataId);

            return NVMDatabase.dataRecord+i;
        }
    }
    if (DEBUG)
        print2uart("getDataRecord: dataId: %d not found.\n", dataId);
    return NULL;
}

/*
 * DBreadIn(void* to,int id):
 * parameters: read to where, id of the data
 * return:
 * */
void DBreadIn(void* to,int id){
    // memcpy(to, DBread(id), DB[id].size);
}

/* Deprecated */
/*
 * DBworking(int id, void *source, int size): return a working space for tasks
 * parameters: data structure of working space, size of the required data
 * return: none
 * */
void DBworking(struct working* wIn, int size, int id)
{
    if(VMWorkingSpacePos + size > VM_WORKING_SIZE)
    {
        //reset, assume will not be overlapped
        print2uart("Warning: VM for working version overflowed! resetting\n");
        VMWorkingSpacePos = 0;
    }
    wIn->address = &VMWorkingSpace[VMWorkingSpacePos];
    wIn->loc = 1;
    VMWorkingSpacePos += size;
    wIn->id = id;
    return;
}

data_t *VMDBCreate(uint8_t size, uint8_t dataId)
{
    if (VMDatabase.dataRecordPos >= DB_MAX_OBJ)
    {
        print2uart("Error: VMDatabase full, please enlarge MAX_DB_OBJ\n");
        return NULL
    }
    if (DEBUG)
        print2uart("VMDBCreate: size: %d, dataId: %d\n", size, dataId);

    if (VMWorkingSpacePos + size > VM_WORKING_SIZE)
    {
        //reset, assume will not be overlapped
        print2uart("Warning: VM for working version overflowed! resetting\n");
        VMWorkingSpacePos = 0;
    }

    data_t *newVMData = VMDatabase.dataRecord + VMDatabase.dataRecordPos;
    newVMData->size = size;
    newVMData->ptr = &VMWorkingSpace[VMWorkingSpacePos];
    newVMData->id  = dataId;

    VMWorkingSpacePos += size;
    VMDatabase.dataRecordPos++;

    return newVMData;
}

void VMDBDelete(uint8_t dataId)
{

}

void readRemoteDB(const TaskHandle_t const *xFromTask, data_t *dataObj, uint8_t remoteAddr, uint8_t dataId)
{
    /* logging */
    createDataTransferLog(request, dataId, dataObj, xFromTask);


    // send request
    PacketHeader_t header = {.packetType = RequestData};
    RequestDataPkt_t packet = {.header = header, .dataId = dataId};
    RFSendPacket(0, (uint8_t *)&packet, sizeof(RequestDataPkt_t));

    if (DEBUG)
    {
        print2uart("readRemoteDB: create log dataId: %d, TaskHandle: %x \n", dataId, xFromTask);
        print2uart("readRemoteDB: read remote dataId: %d, wait for notification\n", dataId);
    }

    /* Block indefinitely (without a timeout, so no need to check the function's
        return value) to wait for a notification.  Here the RTOS task notification
        is being used as a binary semaphore, so the notification value is cleared
        to zero on exit.  NOTE!  Real applications should not block indefinitely,
        but instead time out occasionally in order to handle error conditions
        that may prevent the interrupt from sending any more notifications. */
    ulTaskNotifyTake(pdTRUE,         /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */

    if (DEBUG)
        print2uart("readRemoteDB: remote dataId: %d, notified\n", dataId);
}

/*
 * description: start the concurrency control of the current task, this function will register the current TCB to the DB, and initialize the TCB's validity interval
 * parameters: the TCB number
 * return: none
 * */
// void registerTCB(int id){
//     int i;
//     unsigned short TCB = pxCurrentTCB->uxTCBNumber;

//     //initialize the TCB's validity interval
//     pxCurrentTCB->vBegin = 0;
//     pxCurrentTCB->vEnd = 4294967295;
//     //register the current TCB to the DB,
//     for(i = 0; i < NUMTASK; i++){
//         if(!WSRValid[i]){
//             WSRTCB[i] = TCB;
//             WSRBegin[i] = 4294967295;
//             WSRValid[i] = 1;
//             return;
//         }
//     }
//     //should not be here
//     //TODO: error handling
// }


/*
 * description: finish the concurrency control of the current task, unregister the current TCB from the DB
 * parameters: the TCB number
 * return: none
 * */
// void unresgisterTCB(int id)
// {
//     int i;
//     unsigned short TCB = pxCurrentTCB->uxTCBNumber;
//     for(i = 0; i < NUMTASK; i++){
//         if(WSRValid[i]){
//             if(WSRTCB[i] == TCB){
//                 WSRValid[i] = 0;
//                 return;
//             }
//         }
//     }
//     //TODO: error handling
// }


/*
 * description: create/write a data entry
 * parameters: source address of the data, size in terms of bytes
 * return: the id of the data, -1 for failure
 * note: currently support for committing one data object
 * */
// int DBcommit(struct working *work, int size, int num){
//     int creation = 0,workId;
//     void* previous;

//     /* creation or invalid ID */
//     if(work->id < 0){
//         work->id = dataId++;
//         creation = 1;
//     }
//     else//need to free it after commit
//         previous = accessData(work->id);

//     if(work->id >= DB_MAX_OBJ)
//         return -1;

//     workId = work->id;
//     taskENTER_CRITICAL();

//     int i,j;
//     /* Validation */
//     // for read set that have been updated after the read
//     for(j = 0; j < NUMTASK; j++){
//        if(WSRValid[j] > 0 && WSRTCB[j] == pxCurrentTCB->uxTCBNumber){
//            pxCurrentTCB->vEnd = min(pxCurrentTCB->vEnd,WSRBegin[j]-1);
//            WSRBegin[j] = 4294967295;//incase for a task with multiple commits
//            break;
//        }
//     }

//     // for write set:
//     if(creation == 0)
//         pxCurrentTCB->vBegin = max(pxCurrentTCB->vBegin, getBegin(workId)+1);

//     // should be finished no more later than current time
//     pxCurrentTCB->vEnd = min(pxCurrentTCB->vEnd, timeCounter);

//     // validation success or fail
//     if(pxCurrentTCB->vBegin > pxCurrentTCB->vEnd){
//         taskEXIT_CRITICAL();
//         regTaskEnd();
//         taskRerun();
//         /* Free the previous consistent data */
//         if(creation == 0)
//             vPortFree(previous);
//         return -1;
//     }

//     /* validation success, commit all changes*/
//         void* temp = (void*)pvPortMalloc(size);
//         memcpy(temp, work->address, size);
//         commit(workId,temp, pxCurrentTCB->vBegin, pxCurrentTCB->vEnd);

//     /* Free the previous consistent data */
//     if(creation == 0)
//         vPortFree(previous);

//     /* Link the data */
//     DB[workId].size = size;
//     DB[workId].ptr = work->address;

//     /* validation: for those written data read by other tasks*/
//     // all write set's readers can be removed from readTCBNum[] after their valid interval is reduced
//     for(i = 0; i < MAXREAD; i++){
//         if(DB[workId].readTCBNum[i] != 0){
//             //no point to self-restricted
//             if(DB[workId].readTCBNum[i] == pxCurrentTCB->uxTCBNumber){
//                 DB[workId].readTCBNum[i] = 0;
//                 continue;
//             }

//             //search for valid, the task has read the written data
//             for(j = 0; j < NUMTASK; j++){
//                 if(WSRValid[j] == 1 &&  WSRTCB[j] == DB[workId].readTCBNum[i]){
//                     WSRBegin[j] = min(pxCurrentTCB->vBegin,WSRBegin[j]);
//                     break;
//                 }
//             }
//             DB[workId].readTCBNum[i] = 0;// configure for write set's readers
//         }
//     }

//     taskEXIT_CRITICAL();
//     return workId;
// }
