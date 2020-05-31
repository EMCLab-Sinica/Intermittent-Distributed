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
    data_t *xDataObj;
    TaskHandle_t *xFromTask;

} DataTransferLog_t;


void taskRerun();
void taskRerun();
void regTaskStart(void* add, unsigned short pri, unsigned short TCB, void* TCBA, int stopTrack);
void regTaskEnd();
void failureRecovery();
void freePreviousTasks();

/* DataManager Logging */
void createDataTransferLog(TransferType_e transferType, uint8_t dataId,
                           const data_t const *dataObj, const TaskHandle_t const *xFromTask);
DataTransferLog_t * getDataTransferLog(TransferType_e transferType, uint8_t dataId);
void deleteDataTransferLog(TransferType_e transferType, uint8_t dataId);

#endif /* RECOVERYHANDLER_RECOVERY_H_ */
