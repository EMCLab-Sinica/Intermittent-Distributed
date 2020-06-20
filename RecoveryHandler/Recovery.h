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
#include "stdbool.h"


/* DataManager Logging */
typedef struct DataRequestLog
{
    bool valid;
    DataUUID_t dataId;
    TaskUUID_t taskId;
    Data_t *xToDataObj;
    TaskHandle_t *xFromTask;

} DataRequestLog_t;

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
void regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, unsigned int stopTrack);
void regTaskEnd();
void failureRecovery();
void freePreviousTasks();

void initRecoveryEssential();

/* DataManager Logging */
DataRequestLog_t *createDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId, const Data_t *dataObj, const TaskHandle_t *xFromTask);
DataRequestLog_t *getDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);
void deleteDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);


#endif /* RECOVERYHANDLER_RECOVERY_H_ */
