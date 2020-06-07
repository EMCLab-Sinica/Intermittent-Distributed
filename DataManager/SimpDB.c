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
static Database_t NVMDatabase;

#pragma DATA_SECTION(dataId, ".map") //id for data labeling
static int dataId;

/* Half of RAM for caching (0x2C00~0x3800) */
#pragma location = 0x2C00 //Space for working at SRAM
static unsigned char VMWorkingSpace[VM_WORKING_SIZE];

#pragma DATA_SECTION(VMWorkingSpacePos, ".map") //RR pointer for working at SRAM
static unsigned int VMWorkingSpacePos;

/* space in VM for working versions*/
static Database_t VMDatabase;

extern MyList_t *dataTransferLogList;
extern uint8_t nodeAddr;

/*
 * NVMDBConstructor(): initialize all data structure in the database
 * parameters: none
 * return: none
 * */
void NVMDBConstructor(){
    // create logging
    dataTransferLogList = makeList();
    init();
    NVMDatabase.dataIdAutoIncrement = 1; // dataId start from 1
    NVMDatabase.dataRecordCount = 0;
    for(uint8_t i = 0; i < DB_MAX_OBJ; i++){
        NVMDatabase.dataRecord[i].id = -1;
        NVMDatabase.dataRecord[i].ptr = NULL;
        NVMDatabase.dataRecord[i].size = 0;
    }

    VMWorkingSpacePos = 0;
    for(uint8_t i = 0; i < NUMTASK; i++)
        WSRValid[i] = 0;

    // insert for test
    if (nodeAddr == 1)
    {
        int32_t testValue = 7;
        Data_t myData = createWorkingSpace(&testValue, sizeof(testValue));
        commitLocalDB(&myData, sizeof(testValue));
    }
}

void VMDBConstructor(){
    VMDatabase.dataIdAutoIncrement = 1; // dataId start from 1
    NVMDatabase.dataRecordCount = 0;
    for(uint8_t i = 0; i < DB_MAX_OBJ; i++){
        VMDatabase.dataRecord[i].id = -1;
        VMDatabase.dataRecord[i].ptr = NULL;
        VMDatabase.dataRecord[i].size = 0;
    }
}

Data_t *getDataRecord(uint8_t owner, uint8_t dataId, DBSearchMode_e mode)
{
    Data_t *data = NULL;
    // read NVM DB
    if (mode == nvmdb || mode == all)
    {
        for (uint8_t i = 0; i < DB_MAX_OBJ; i++)
        {
            data = NVMDatabase.dataRecord + i;
            if (data->owner == owner && data->id == dataId)
            {
                if (DEBUG)
                    print2uart("getDataRecord: (owner, dataId): (%d, %d) found in NVMDB index %d\n", owner, dataId, i);
                
                // update data location (two version atomic commit)
                data->ptr = accessData(i);
                return data;
            }
        }
        data = NULL;
    }

    if (mode == vmdb || mode == all)
    {
        for (uint8_t i = 0; i < DB_MAX_OBJ; i++)
        {
            data = VMDatabase.dataRecord + i;
            if (data->owner == owner && data->id == dataId)
            {
                if (DEBUG)
                    print2uart("getDataRecord: (owner, dataId): (%d, %d) found in VMDB\n", owner, dataId);

                return data;
            }
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

Data_t readLocalDB(uint8_t dataId, void* destDataPtr, uint8_t size)
{
    Data_t dataWorking;
    Data_t * data = getDataRecord(nodeAddr, dataId, all);

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

Data_t readRemoteDB(const TaskHandle_t const *xFromTask, uint8_t owner,
                    uint8_t dataId, void *destDataPtr, uint8_t size)
{
    Data_t *duplicatedDataObj;
    // see if we already have the duplicated copy of the data object
    duplicatedDataObj = getDataRecord(owner, dataId, vmdb);
    if (duplicatedDataObj == NULL)
    {
        duplicatedDataObj = createVMDBobject(size);
    }

    /* logging */
    createDataTransferLog(request, owner, dataId, duplicatedDataObj, xFromTask);

    // send request
    PacketHeader_t header = {.packetType = RequestData};
    DataControlPacket_t packet = {.header = header, .owner = owner, .dataId = dataId};
    RFSendPacket(0, (uint8_t *)&packet, sizeof(packet));

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

    deleteDataTransferLog(request, owner, dataId);

    Data_t dataRead;
    memcpy(&dataRead, duplicatedDataObj, sizeof(Data_t));
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
Data_t createWorkingSpace(void *dataPtr, uint32_t size)
{
    Data_t data;

    data.id = -1;
    data.owner = nodeAddr;
    data.version = working;
    data.validationTS = 0;
    data.ptr = dataPtr;
    data.size = size;
    return data;
}

/*
 * Create a data object in Database located in VM
 * parameters: size of the data object
 * return: none
 * */
Data_t *createVMDBobject(uint8_t size)
{
    if (VMDatabase.dataRecordCount >= DB_MAX_OBJ)
    {
        print2uart("Error: VMDatabase full, please enlarge DB_MAX_OBJ\n");
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

    int8_t freeSlot = -1;
    // find a free slot
    for(uint8_t i = 0; i < DB_MAX_OBJ; i++)
    {
        if(VMDatabase.dataRecord[i].id < 0)
        {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot == -1)
    {
        VMDatabase.dataRecordCount = DB_MAX_OBJ;
        print2uart("Error: VMDatabase full, please enlarge DB_MAX_OBJ\n");
        return NULL;
    }

    Data_t *newVMData = &VMDatabase.dataRecord[freeSlot];
    newVMData->size = size;
    newVMData->ptr = VMWorkingSpace + VMWorkingSpacePos;

    VMWorkingSpacePos += size;
    VMDatabase.dataRecordCount++;

    return newVMData;
}

int32_t commitLocalDB(Data_t *data, size_t size)
{
    if (data->version != working && data->version != modified) // only working or modified version can be committed
    {
        print2uart("Can only commit working or modified version\n");
        return -1;
    }

    void* oldMallocDataAddress = NULL;
    int32_t objectIndex = -1;

    if (data->id <= 0) // creation
    {
        data->owner = nodeAddr;
        data->size = size;
        data->id = NVMDatabase.dataIdAutoIncrement++;
        for (uint8_t i = 0; i < DB_MAX_OBJ; i++)
        {
            if (NVMDatabase.dataRecord[i].id < 0)
            {
                objectIndex = i;
                break;
            }
            if (objectIndex < 0)
            {
                print2uart("Error: NVMDatabase full, please enlarge DB_MAX_OBJ\n");
                return -1;
            }
        }
    }
    else // find db record
    {

        Data_t *DBData = NULL;
        for (uint8_t i = 0; i < NVMDatabase.dataRecordCount; i++)
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

    // incase failed at previous stage
    NVMDatabase.dataRecordCount++;
    return (int32_t)data->id;
}
