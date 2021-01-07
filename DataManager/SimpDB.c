/*
 * SimpDB.cpp
 */

#include "SimpDB.h"

#include <FreeRTOS.h>
#include <stdio.h>
#include <string.h>  // for memset
#include <task.h>

#include "RFHandler.h"
#include "Recovery.h"
#include "Validation.h"
#include "config.h"
#include "myuart.h"

#define DEBUG 0  // control debug message
#define INFO 1   // control debug message

#pragma NOINIT(NVMDatabase)
static Database_t NVMDatabase;

/* Half of RAM for caching (0x2C00~0x3800) */
#pragma location = 0x2C00  // Space for working at SRAM
static unsigned char VMWorkingSpace[VM_WORKING_SIZE];

#pragma DATA_SECTION(VMWorkingSpacePos, \
                     ".map")  // RR pointer for working at SRAM
static unsigned int VMWorkingSpacePos;

/* space in VM for working versions*/
static Database_t VMDatabase;

extern DataRequestLog_t dataRequestLogs[MAX_GLOBAL_TASKS];
extern uint8_t nodeAddr;
/*
 * NVMDBConstructor(): initialize all data structure in the database
 * parameters: none
 * return: none
 * */
void NVMDBConstructor() {
    init();
    NVMDatabase.dataIdAutoIncrement = 1;  // dataId start from 1
    NVMDatabase.dataRecordCount = 0;
    DataUUID_t initId = {.owner = 0, .id = 0};
    for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
        memset(&(NVMDatabase.dataRecord[i].dataId), 0, sizeof(DataUUID_t));
        NVMDatabase.dataRecord[i].ptr = NULL;
        NVMDatabase.dataRecord[i].size = 0;
        memset(&(NVMDatabase.dataRecord[i].validationLock), 0,
               sizeof(TaskUUID_t));
        //memset(NVMDatabase.dataRecord[i].readers, 0,
        //       sizeof(TaskUUID_t) * MAX_READERS);
    }

    VMWorkingSpacePos = 0;
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++) {
        // taskAccessObjectLog[i].validLog = pdFALSE;
    }

    // insert for test
    /*
    if (nodeAddr == 1)
    {
        int32_t testValue = 7;
        Data_t myData = createWorkingSpace(&testValue, sizeof(testValue));
        commitLocalDB(&myData, sizeof(testValue));
    }
    */
}

void VMDBConstructor() {
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++) {
        dataRequestLogs[i].valid = false;
    }

    VMDatabase.dataIdAutoIncrement = 1;  // dataId start from 1
    VMDatabase.dataRecordCount = 0;
    DataUUID_t initId = {.owner = 0, .id = 0};
    TaskUUID_t nullTask = {.nodeAddr = 0, .id = 0};
    for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
        VMDatabase.dataRecord[i].dataId = initId;
        VMDatabase.dataRecord[i].ptr = NULL;
        VMDatabase.dataRecord[i].size = 0;
        VMDatabase.dataRecord[i].validationLock = nullTask;
    }
}

uint32_t getDataBegin(DataUUID_t dataId) {
    Data_t *data;
    for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
        data = NVMDatabase.dataRecord + i;
        if (dataIdEqual(&(data->dataId), &dataId)) {
            return getBegin(i);
        }
    }
    return UINT32_MAX;
}

Data_t *getDataRecord(DataUUID_t dataId, DBSearchMode_e mode) {
    Data_t *data = NULL;
    // read NVM DB
    if (mode == nvmdb || mode == all) {
        for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
            data = NVMDatabase.dataRecord + i;
            if (dataIdEqual(&(data->dataId), &dataId)) {
                if (DEBUG)
                    print2uart(
                        "getDataRecord: (owner, dataId): (%d, %d) found in "
                        "NVMDB index %d\n",
                        dataId.owner, dataId.id, i);

                // update data location (two version atomic commit)
                data->ptr = accessData(i);
                return data;
            }
        }
        data = NULL;
    }

    if (mode == vmdb || mode == all) {
        for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
            data = VMDatabase.dataRecord + i;
            if (dataIdEqual(&(data->dataId), &dataId)) {
                if (DEBUG)
                    print2uart(
                        "getDataRecord: (owner, dataId): (%d, %d) found in "
                        "VMDB\n",
                        dataId.owner, dataId.id);

                return data;
            }
        }
    }

    if (DEBUG)
        print2uart("getDataRecord: (owner, dataId): (%d, %d) not found\n",
                   dataId.owner, dataId.id);

    return NULL;
}

/*
 * description: create/write a data entry to NVM DB
 * parameters:
 * return:
 * note: currently support for committing one data object
 * */

Data_t readLocalDB(uint8_t id, void *destDataPtr, uint8_t size) {
    DataUUID_t dataId = {.owner = nodeAddr, .id = id};
    Data_t dataWorking;
    Data_t *data = getDataRecord(dataId, all);

    if (data == NULL) {
        if (DEBUG)
            print2uart("readLocalDB: Can not find data with id=%d\n",
                       dataId.id);
        destDataPtr = NULL;
        // reset the DataUUID
        DataUUID_t resetId = {.owner = 0, .id = 0};
        dataWorking.dataId = resetId;
        return dataWorking;
    }

    memcpy(destDataPtr, data->ptr, size);
    dataWorking = *data;
    dataWorking.ptr = destDataPtr;
    dataWorking.version = working;
    if (size < data->size) {
        dataWorking.size = size;
    }

    return dataWorking;
}

Data_t readRemoteDB(TaskUUID_t taskId, const TaskHandle_t const *xFromTask,
                    uint8_t remoteAddr, uint8_t id, void *destDataPtr,
                    uint8_t size) {
    Data_t *dataBuffer;
    DataUUID_t dataId = {.owner = remoteAddr, .id = id};
    // see if we already have the duplicated copy of the data object
    dataBuffer = getDataRecord(dataId, vmdb);
    if (dataBuffer == NULL) {
        dataBuffer = createVMDBobject(size);
        dataBuffer->dataId = dataId;
        dataBuffer->version = duplicated;
        dataBuffer->size = size;
    }
    /* logging */
    DataRequestLog_t *log =
        createDataRequestLog(taskId, dataId, dataBuffer, xFromTask);
    if (log == NULL) {
        printf("DataRequestLog create failed!");
        while (1)
            ;
    }

    // send request
    RequestDataPacket_t packet = {
        .header.packetType = RequestData, .taskId = taskId, .dataId = dataId};
    if (INFO) {
        print2uart("task (%d, %d), read remote (%d, %d)\n", taskId.nodeAddr, taskId.id, remoteAddr, id);
    }
    uint32_t ulNotificationValue = 0;
    do {
        // RFSendPacket(0, (uint8_t *)&packet, sizeof(packet));
        RFSendPacket(remoteAddr, (uint8_t *)&packet, sizeof(packet));
        ulNotificationValue = ulTaskNotifyTake(
            pdFALSE, /* Clear the notification value before exiting. */
            pdMS_TO_TICKS(800));
    } while (ulNotificationValue != 1);

    if (DEBUG) print2uart("readRemoteDB: remote dataId: %d, notified\n", id);

    log->valid = false;

    // create working version for the task
    Data_t dataRead = *dataBuffer;
    memcpy(destDataPtr, dataBuffer->ptr, dataBuffer->size);
    dataRead.ptr = destDataPtr;
    dataRead.version = working;
    return dataRead;
}

/*
 * DBworking(int id, void *source, int size): return a working space for tasks
 * parameters: data structure of working space, size of the required data
 * return: none
 * */
Data_t createWorkingSpace(void *dataPtr, uint32_t size) {
    Data_t data;
    DataUUID_t dataId = {.owner = nodeAddr, .id = 0};

    data.dataId = dataId;
    data.version = working;
    data.ptr = dataPtr;
    data.size = size;
    data.validationLock = (TaskUUID_t){.nodeAddr = 0, .id = 0};
    return data;
}

/*
 * Create a data object in Database located in VM
 * parameters: size of the data object
 * return: none
 * */
Data_t *createVMDBobject(uint8_t size) {
    if (VMDatabase.dataRecordCount >= MAX_DB_OBJ) {
        print2uart("Error: VMDatabase full, please enlarge MAX_DB_OBJ\n");
        return NULL;
    }
    if (DEBUG) print2uart("VMDBCreate: size: %d\n", size);

    if (VMWorkingSpacePos + size > VM_WORKING_SIZE) {
        // reset, assume will not be overlapped
        print2uart("Warning: VM for working version overflowed! resetting\n");
        VMWorkingSpacePos = 0;
    }

    int8_t freeSlot = -1;
    // find a free slot
    for (uint8_t i = 0; i < MAX_DB_OBJ; i++) {
        if (VMDatabase.dataRecord[i].dataId.id == 0) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot == -1) {
        VMDatabase.dataRecordCount = MAX_DB_OBJ;
        print2uart("Error: VMDatabase full, please enlarge MAX_DB_OBJ\n");
        return NULL;
    }

    Data_t *newVMData = &VMDatabase.dataRecord[freeSlot];
    newVMData->size = size;
    newVMData->ptr = VMWorkingSpace + VMWorkingSpacePos;

    VMWorkingSpacePos += size;
    VMDatabase.dataRecordCount++;

    return newVMData;
}

DataUUID_t commitLocalDB(Data_t *data, size_t size) {
    if (data->version != working &&
        data->version !=
            modified)  // only working or modified version can be committed
    {
        print2uart("Can only commit working or modified version\n");
        while (1)
            ;
    }

    if (data->dataId.id < 1 || data->dataId.id > MAX_DB_OBJ) {
        print2uart("Invalid DataId, failed to commit\n");
        while (1)
            ;
    }

    void *oldMallocDataAddress = NULL;
    int32_t objectIndex = data->dataId.id;

    taskENTER_CRITICAL();

    void *NVMSpace = (void *)pvPortMalloc(data->size);
    memcpy(NVMSpace, data->ptr, data->size);
    commit(objectIndex, NVMSpace, 0, 0);

    /* Free the previous consistent data */
    if (oldMallocDataAddress) vPortFree(oldMallocDataAddress);

    /* Link the data */
    NVMDatabase.dataRecord[objectIndex] = *data;
    NVMDatabase.dataRecord[objectIndex].ptr = NVMSpace;
    NVMDatabase.dataRecord[objectIndex].size = size;
    if (data->dataId.owner == nodeAddr) {
        NVMDatabase.dataRecord[objectIndex].version = consistent;
    } else {
        NVMDatabase.dataRecord[objectIndex].version = modified;
    }
    taskEXIT_CRITICAL();

    return data->dataId;
}

