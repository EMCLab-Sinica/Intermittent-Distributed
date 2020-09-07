/*
 * Recover.h
 *
 *  Created on: 2018�~2��12��
 *      Author: Meenchen
 */
#ifndef RECOVERYHANDLER_RECOVERY_H_
#define RECOVERYHANDLER_RECOVERY_H_

#include <config.h>
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdbool.h"
#include "Validation.h"

/* DataManager Logging */
typedef struct DataRequestLog
{
    bool valid;
    DataUUID_t dataId;
    TaskUUID_t taskId;
    Data_t *xToDataObj;
    TaskHandle_t *xFromTask;

} DataRequestLog_t;

typedef enum TaskStatus
{
    invalid,
    running,
    validating,
    finished
} TaskStatus_e;

typedef struct TaskRecord
{
    TaskStatus_e taskStatus;
    uint8_t priority;
    uint16_t TCBNum;
    uint8_t schedulerTask; // if it is schduler's task, we don't need to recreate it because the shceduler does
    char taskName[configMAX_TASK_NAME_LEN + 1];
    void *address;      // Function address of tasks
    void *TCB;          // TCB address of tasks
    uint16_t stackSize;
    OutboundValidationRecord_t *validationRecord;

} TaskRecord_t;

void taskRerun();
void taskRerun();
TaskRecord_t* regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, unsigned int stopTrack);
void regTaskEnd();
void failureRecovery();
void freePreviousTasks();

void initRecoveryEssential();

/* DataManager Logging */
DataRequestLog_t *createDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId, const Data_t *dataObj, const TaskHandle_t *xFromTask);
DataRequestLog_t *getDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);
void deleteDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);


#endif /* RECOVERYHANDLER_RECOVERY_H_ */
