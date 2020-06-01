/*
 * Recover.h
 *
 *  Created on: 2018�~2��12��
 *      Author: Meenchen
 */
#ifndef RECOVERYHANDLER_RECOVERY_H_
#define RECOVERYHANDLER_RECOVERY_H_

#include <config.h>
#include "mylist.h"
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"


/* DataManager Logging */
typedef enum TransferType
{
    request,
    response 
} TransferType_e;

typedef struct DataTransferLog
{
    TransferType_e type;
    uint8_t dataId;
    Data_t *xDataObj;
    TaskHandle_t *xFromTask;

} DataTransferLog_t;

typedef struct TaskRecord
{
    uint8_t unfinished; // 1: running, others for invalid
    uint8_t priority;
    uint16_t TCBNum;
    uint8_t schedulerTask; // if it is schduler's task, we don't need to recreate it because the shceduler does
    char taskName[configMAX_TASK_NAME_LEN + 1];
    void *address;      // Function address of tasks
    void *TCB;          // TCB address of tasks
    uint16_t stackSize;

} TaskRecord_t;

void taskRerun();
void taskRerun();
void regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, uint8_t stopTrack);
void regTaskEnd();
void failureRecovery();
void freePreviousTasks();

/* DataManager Logging */
void createDataTransferLog(TransferType_e transferType, uint8_t dataId,
                           const Data_t const *dataObj, const TaskHandle_t const *xFromTask);
DataTransferLog_t * getDataTransferLog(TransferType_e transferType, uint8_t dataId);
void deleteDataTransferLog(TransferType_e transferType, uint8_t dataId);

#endif /* RECOVERYHANDLER_RECOVERY_H_ */
