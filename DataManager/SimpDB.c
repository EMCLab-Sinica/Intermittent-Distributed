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
#include "config.h"

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
extern uint8_t nodeAddr;

/*
 * DBConstructor(): initialize all data structure in the database
 * parameters: none
 * return: none
 * */
void DBConstructor(){
    // create logging
    dataTransferLogList = makeList();
    init();
    NVMDatabase.dataRecordPos = 0;
    NVMDatabase.dataIdAutoIncrement = 1; // dataId start from 1
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
    print2uart("DBCons: nodeAddr %d\n", nodeAddr);
    if (nodeAddr == 1)
    {
        uint32_t *testValue = pvPortMalloc(sizeof(uint32_t));
        *testValue = 7;
        print2uart("init owner = 1, data = 1\n");
        NVMDatabase.dataRecord[0].owner = 1;
        NVMDatabase.dataRecord[0].id = 1;
        NVMDatabase.dataRecord[0].ptr = (void *)testValue;
        NVMDatabase.dataRecord[0].size = sizeof(*testValue);
        NVMDatabase.dataRecordPos = 1;
    }
}

data_t *getDataRecord(uint8_t owner, uint8_t dataId)
{
    if (DEBUG)
        print2uart("getDataRecord: request (owner, dataId): (%d, %d)\n", owner, dataId);

    data_t *data = NULL;

    for (uint8_t i = 0; i < VMDatabase.dataRecordPos; i++)
    {
        data = VMDatabase.dataRecord+i;
        if (data->owner == owner && data->id == dataId)
        {
            if (DEBUG)
                print2uart("getDataRecord: (owner, dataId): (%d, %d) found in VMDB\n", owner, dataId);

            return data;
        }
    }

    data = NULL;
    // read NVM DB
    for (uint8_t i = 0; i < NVMDatabase.dataRecordPos; i++)
    {
        data = NVMDatabase.dataRecord+i;
        if (data->owner == owner && data->id == dataId)
        {
            if (DEBUG)
                print2uart("getDataRecord: (owner, dataId): (%d, %d) found in NVMDB\n", owner, dataId);

            return data;
        }
    }
    if (DEBUG)
        print2uart("getDataRecord: (owner, dataId): (%d, %d) not found\n", owner, dataId);

    return NULL;
}

/*
 * description: create/write a data entry to NVM DB
 * parameters:
 * return:
 * note: currently support for committing one data object
 * */

data_t readLocalDB(uint8_t dataId, void* destDataPtr, uint8_t size)
{
    data_t dataWorking;
    data_t * data = getDataRecord(nodeAddr, dataId);

    if (data == NULL)
    {
        if (DEBUG)
            print2uart("readLocalDB: Can not find data with id=%d\n", dataId);
        destDataPtr = NULL;
        dataWorking.id = -1;
        return  dataWorking;
    }

    memcpy(destDataPtr, data->ptr, size);
    dataWorking = *data;
    dataWorking.ptr = destDataPtr;
    dataWorking.version = working;
    if (size < data->size)
    {
        dataWorking.size = size;
    }

    return dataWorking;
}

data_t readRemoteDB(const TaskHandle_t const *xFromTask, uint8_t owner,
                    uint8_t dataId, void *destDataPtr, uint8_t size)
{
    data_t *duplicatedDataObj = createVMDBobject(size);
    /* logging */
    createDataTransferLog(request, dataId, duplicatedDataObj, xFromTask);

    // send request
    PacketHeader_t header = {.packetType = RequestData};
    RequestDataPkt_t packet = {.header = header, .owner = owner, .dataId = dataId};
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

    deleteDataTransferLog(request, dataId);

    data_t dataRead;
    memcpy(&dataRead, duplicatedDataObj, sizeof(data_t));
    memcpy(destDataPtr, duplicatedDataObj->ptr, duplicatedDataObj->size);
    dataRead.ptr = destDataPtr;
    dataRead.version = working;
    return dataRead;
}

/*
 * DBworking(int id, void *source, int size): return a working space for tasks
 * parameters: data structure of working space, size of the required data
 * return: none
 * */
data_t createWorkingSpace()
{
    data_t data;

    data.id = -1;
    data.owner = nodeAddr;
    data.version = working;
    data.validationTS = 0;
    return data;
}

/*
 * Create a data object in Database located in VM
 * parameters: size of the data object
 * return: none
 * */
data_t *createVMDBobject(uint8_t size)
{
    if (VMDatabase.dataRecordPos >= DB_MAX_OBJ)
    {
        print2uart("Error: VMDatabase full, please enlarge MAX_DB_OBJ\n");
        return NULL;
    }
    if (DEBUG)
        print2uart("VMDBCreate: size: %d\n", size);

    if (VMWorkingSpacePos + size > VM_WORKING_SIZE)
    {
        //reset, assume will not be overlapped
        print2uart("Warning: VM for working version overflowed! resetting\n");
        VMWorkingSpacePos = 0;
    }

    data_t *newVMData = &VMDatabase.dataRecord[VMDatabase.dataRecordPos];
    newVMData->size = size;
    newVMData->ptr = VMWorkingSpace + VMWorkingSpacePos;

    VMWorkingSpacePos += size;
    VMDatabase.dataRecordPos++;

    return newVMData;
}

int8_t commitLocalDB(data_t *data, uint32_t size)
{
    if (data->version != working || data->version != modified) // only working or modified version can be committed
        return -1;

    void* oldMallocDataAddress = NULL;
    uint32_t objectIndex = 0;

    if (data->id <= 0) // creation
    {
        data->owner = nodeAddr;
        data->size = size;
        data->id = VMDatabase.dataIdAutoIncrement++;
        objectIndex = NVMDatabase.dataRecordPos++;
    }
    else    // find db record
    {

        data_t *DBData = NULL;
        for (uint32_t i = 0; i < NVMDatabase.dataRecordPos; i++)
        {
            DBData = NVMDatabase.dataRecord + i;
            if (DBData->owner == data->owner && DBData->id == data->id)
            {
                objectIndex = i;
                break;
            }
        }

        oldMallocDataAddress = accessData(objectIndex);

        if(DEBUG)
            print2uart("commitLocalDB: commit failed, can not find data: (%d, %d)\n",
                       data->owner, data->id);
        return -1;
    }

    taskENTER_CRITICAL();

    void *NVMSpace = (void *)pvPortMalloc(data->size);
    memcpy(NVMSpace, data->ptr, data->size);
    commit(objectIndex, NVMSpace, 0, 0);

    /* Free the previous consistent data */
    if (oldMallocDataAddress)
        vPortFree(oldMallocDataAddress);

    /* Link the data */
    NVMDatabase.dataRecord[objectIndex] = *data;
    NVMDatabase.dataRecord[objectIndex].ptr = NVMSpace;
    NVMDatabase.dataRecord[objectIndex].size = size;
    if (data->owner == nodeAddr)
    {
        NVMDatabase.dataRecord[objectIndex].version = consistent;
    }
    else
    {
        NVMDatabase.dataRecord[objectIndex].version = modified;
    }


    taskEXIT_CRITICAL();
    return data->id;
}
