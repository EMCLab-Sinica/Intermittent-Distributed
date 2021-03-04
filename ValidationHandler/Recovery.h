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
#include "TaskControl.h"

void taskRerun();
void taskRerun();
TaskRecord_t* regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, unsigned int stopTrack);
void regTaskEnd();
void failureRecovery();
void freePreviousTasks();

void initRecoveryEssential();

void RecoveryServiceRoutine();

/* DataManager Logging */
DataRequestLog_t *createDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId, const Data_t *dataObj, const TaskHandle_t *xFromTask);
DataRequestLog_t *getDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);
void deleteDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId);


#endif /* RECOVERYHANDLER_RECOVERY_H_ */
