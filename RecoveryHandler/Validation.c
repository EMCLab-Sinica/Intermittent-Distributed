#include "FreeRTOS.h"
#include "RecoveryHandler/TaskControl.h"
#include "task.h"
#include "Validation.h"
#include "Recovery.h"
#include "config.h"
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include "RFHandler.h"
#include "myuart.h"
#include <stdint.h>
#include <string.h>
#include "OurTCB.h"


#define DEBUG 0
#define INFO 1

#pragma NOINIT(validationRequestPacketsQueue)
QueueHandle_t validationRequestPacketsQueue;

#pragma NOINIT(outboundValidationRecords)
OutboundValidationRecord_t outboundValidationRecords[MAX_GLOBAL_TASKS];

#pragma NOINIT(inboundValidationRecords)
InboundValidationRecord_t inboundValidationRecords[MAX_GLOBAL_TASKS];

extern volatile TCB_t * volatile pxCurrentTCB;
extern uint8_t nodeAddr;
extern int firstTime;
extern TaskAccessObjectLog_t taskAccessObjectLog[MAX_GLOBAL_TASKS];

void sendValidationRequest(TaskUUID_t *taskId, Data_t *dataToCommit);
void sendValidationResponse(TaskUUID_t *taskId, DataUUID_t *dataId, TimeInterval_t *timeInterval, uint8_t canCommit);
void sendCommitPhaseRequest(uint8_t dataOwner, TaskUUID_t *taskId, uint8_t decision);
void sendCommitPhaseResponse(TaskUUID_t *taskId, DataUUID_t *dataId);

void handleValidationRequest(ValidationP1RequestPacket_t *packet);
void handleValidationResponse(ValidationP1ResponsePacket_t *packet);
void handleCommitRequest(CommitRequestPacket_t *packet);
void handleCommitResponse(CommitResponsePacket_t *packet);

InboundValidationRecord_t *getOrCreateInboundRecord(TaskUUID_t *taskId);
InboundValidationRecord_t *getInboundRecord(TaskUUID_t *taskId);
OutboundValidationRecord_t *getOutboundRecord(TaskUUID_t taskId);
TimeInterval_t calcValidInterval(TaskUUID_t taskId, DataUUID_t dataId, unsigned long long *dataBeginRecord);

void initValidationEssentials()
{
    OutboundValidationRecord_t *outRecord = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        outRecord = outboundValidationRecords + i;
        for(int i = 0; i < MAX_TASK_READ_OBJ; i++)
        {
            // maximum of data size
            outRecord->writeSet[i].ptr = pvPortMalloc(sizeof(int32_t));
        }
        outRecord->validRecord = pdFALSE;
        outRecord->writeSetNum = 0;
        memset(outRecord->validationPassed, 0, sizeof(uint8_t) * MAX_TASK_READ_OBJ);
        memset(outRecord->commitDone, 0, sizeof(uint8_t) * MAX_TASK_READ_OBJ);
    }

    InboundValidationRecord_t *inRecord = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        inRecord = inboundValidationRecords + i;
        inRecord->validRecord = pdFALSE;
        inRecord->writeSetNum = 0;
        memset(inRecord->vPhase1DataBegin, 0, sizeof(unsigned long long) * MAX_TASK_READ_OBJ);
    }
}
uint8_t initValidationQueues()
{
    if (firstTime == 1)
    {
        vQueueDelete(validationRequestPacketsQueue);
    }

    validationRequestPacketsQueue = xQueueCreate(5, MAX_PACKET_LEN);
    if (validationRequestPacketsQueue == NULL)
    {
        print2uart("Error: DB Service Routine Queue init failed\n");
    }

    return pdTRUE;
}

void taskCommit(uint8_t tid, TaskHandle_t *fromTask, uint8_t commitNum, ...)
{

    if(DEBUG)
    {
        print2uart("Task %d, request to commit\n", tid);
    }

    // find a place
    OutboundValidationRecord_t *currentLog = NULL;
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (outboundValidationRecords[i].validRecord == pdFALSE)
        {
            currentLog = outboundValidationRecords + i;
            break;
        }
    }
    if (currentLog == NULL)
    {
        print2uart("No place for OutboundValidationRecord\n");
        while(1);
    }

    // save the log to taskRecord
    TaskRecord_t * taskRecord = pxCurrentTCB->taskRecord;
    taskRecord->validationRecord = currentLog;
    currentLog->taskRecord = (void*)taskRecord;

    // save write set to log
    if (commitNum > MAX_TASK_READ_OBJ)
    {
        print2uart("Error, maximum task read data exceeded\n");
        while(1);
    }
    if (commitNum > 0)
    {
        Data_t *data;
        Data_t *currentWriteSet;
        va_list vl;
        va_start(vl, commitNum);
        for (uint8_t i = 0; i < commitNum; i++)
        {
            data = va_arg(vl, Data_t *);
            currentWriteSet = currentLog->writeSet + i;
            void *ptrToNVM = currentWriteSet->ptr;
            *currentWriteSet = *data;   // copy the data object to NVM
            currentWriteSet->ptr = ptrToNVM;
            memcpy(currentWriteSet->ptr, data->ptr, data->size);    // copy the data content
            currentWriteSet->version = modified;
            currentLog->writeSetNum++;
            if(DEBUG)
            {
                print2uart("Commit data (%d, %d) added\n", data->dataId.owner, data->dataId.id);
            }
        }
        va_end(vl);
    }
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = tid};
    currentLog->taskId = taskId;
    currentLog->taskHandle = fromTask;
    currentLog->writeSetNum = commitNum;
    currentLog->stage = validationPhase;
    currentLog->validRecord = pdTRUE;

    // task sleep wait for validation and commit
    ulTaskNotifyTake(pdTRUE,         /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */
}

void inboundValidationHandler()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    uint8_t hasPacket = pdFALSE;
    PacketHeader_t *packetHeader = NULL;

    while(1)
    {
        // Inbound Validation
        hasPacket = xQueueReceive(validationRequestPacketsQueue, (void *)packetBuf, portMAX_DELAY);
        if (hasPacket)
        {
            packetHeader = (PacketHeader_t *)packetBuf;
            if (DEBUG)
            {
                print2uart("Validation PacketType: %d\n", packetHeader->packetType);
            }
            switch (packetHeader->packetType)
            {
                case ValidationRequest:
                {
                    ValidationP1RequestPacket_t *packet = (ValidationP1RequestPacket_t *)packetBuf;
                    handleValidationRequest(packet);
                    break;
                }
                case ValidationResponse:
                {
                    ValidationP1ResponsePacket_t *packet = (ValidationP1ResponsePacket_t *)packetBuf;
                    handleValidationResponse(packet);
                    break;
                }

                case CommitRequest:
                {
                    CommitRequestPacket_t *packet = (CommitRequestPacket_t *)packetBuf;
                    handleCommitRequest(packet);
                    break;
                }

                case CommitResponse:
                {
                    CommitResponsePacket_t *packet = (CommitResponsePacket_t *)packetBuf;
                    handleCommitResponse(packet);
                    break;
                }
            }
        }
    }
}

void outboundValidationHandler()
{
    OutboundValidationRecord_t *outboundRecord = NULL;
    while (1)
    {
        vTaskDelay(300);
        // Outbound Validation
        for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
        {
            outboundRecord = outboundValidationRecords + i;
            if (!outboundRecord->validRecord)
            {
                continue;
            }
            if (INFO)
            {
                print2uart("Record Id:%d, Validation: task: %d, stage: %d\n",
                           i, outboundRecord->taskId.id, outboundRecord->stage);
            }

            switch (outboundRecord->stage)
            {
                case validationPhase:
                {
                    uint8_t toNextStage = pdTRUE;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->validationPassed[i] == 0)
                        {
                            toNextStage = pdFALSE;
                            sendValidationPhase2Request(outboundRecord->writeSet[i].dataId.owner,
                                                        &(outboundRecord->taskId), pdTRUE);
                        }
                    }
                    if (toNextStage == pdTRUE)
                    {
                        outboundRecord->stage = commitPhase;
                    }
                    break;
                }
                case commitPhase:
                {
                    uint8_t toNextStage = pdTRUE;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->commitDone[i] == 0)
                        {
                            toNextStage = pdFALSE;
                            sendCommitPhaseRequest(outboundRecord->writeSet[i].dataId.owner,
                                                    &(outboundRecord->taskId), pdTRUE);
                        }
                    }

                    if (toNextStage == pdTRUE)
                    {
                        outboundRecord->stage = finish;
                    }
                    break;
                }

                case finish:
                {
                    xTaskNotifyGive(*outboundRecord->taskHandle);
                    outboundRecord->validRecord = pdFALSE;
                    outboundRecord->writeSetNum = 0;
                    memset(outboundRecord->validationPassed, 0, sizeof(uint8_t) * MAX_TASK_READ_OBJ);
                    memset(outboundRecord->commitDone, 0, sizeof(uint8_t) * MAX_TASK_READ_OBJ);
                    // recreate the task if needed
                    TaskRecord_t* task = (TaskRecord_t*)(outboundRecord->taskRecord);
                    if (task->taskStatus == validating)
                    {
                        task->taskStatus = invalid;
                        xTaskCreate(task->address, task->taskName, task->stackSize, NULL, task->priority, NULL);
                        dprint2uart("Validation Create: %d\r\n", task->TCBNum);
                    }
                }

                default:
                {
                    break;
                }
            }
        }
    }
}

// functions for outbound validation
void sendValidationPhase1Request(TaskUUID_t* taskId, Data_t *dataToCommit)
{
    ValidationP1RequestPacket_t packet;
    packet.header.packetType = ValidationP1Request;
    packet.taskId = *taskId;
    packet.data.dataId = dataToCommit->dataId;
    packet.data.version = dataToCommit->version;
    packet.data.size = dataToCommit->size;
    memcpy(packet.data.content, dataToCommit->ptr, dataToCommit->size);

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase1Response(TaskUUID_t *taskId, DataUUID_t *dataId, TimeInterval_t *timeInterval,
        uint8_t maybeCommit)
{
    ValidationP1ResponsePacket_t packet;
    packet.header.packetType = ValidationP1Response;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    packet.taskInterval = *timeInterval;
    packet.maybeCommit = maybeCommit;

    RFSendPacket(taskId->nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Request(uint8_t dataOwner, TaskUUID_t* taskId, uint8_t decision)
{
    ValidationP2RequestPacket_t packet;
    packet.header.packetType = ValidationP2Request;
    packet.taskId = *taskId;
    packet.decision = decision;

    RFSendPacket(dataOwner, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Response(TaskUUID_t *taskId, uint8_t passed)
{
    ValidationP2ResponsePacket_t packet;
    packet.header.packetType = ValidationP2Response;
    packet.taskId = *taskId;
    packet.ownerAddr = nodeAddr;
    packet.finalValidationPassed = passed;

    RFSendPacket(taskId->nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhaseRequest(uint8_t dataOwner, TaskUUID_t *taskId, uint8_t decision)
{
    CommitRequestPacket_t packet;
    packet.header.packetType = CommitRequest;
    packet.taskId = *taskId;
    packet.decision = decision;

    RFSendPacket(dataOwner, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhaseResponse(TaskUUID_t *taskId, DataUUID_t *dataId)
{
    CommitResponsePacket_t packet;
    packet.header.packetType = CommitResponse;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    RFSendPacket(taskId->nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void handleValidationPhase1Request(ValidationP1RequestPacket_t *packet)
{
    // find a new place
    InboundValidationRecord_t *record= getOrCreateInboundRecord(&(packet->taskId));
    record->taskId = packet->taskId;
    Data_t *writeData = record->writeSet + record->writeSetNum;
    writeData->dataId = packet->data.dataId;
    writeData->version = packet->data.version;
    writeData->size = packet->data.size;
    memcpy(writeData->ptr, packet->data.content, packet->data.size);
    // currently we only allow one data object access on per remote device
    record->validRecord = pdTRUE;

    Data_t *dataRecord = getDataRecord(packet->data.dataId, nvmdb);
    if (dataRecord->validationLock.nodeAddr == 0)    // nullTask, not locked
    {
        if (DEBUG)
        {
            print2uart("Not locked, get lock\n");
        }
        dataRecord->validationLock = record->taskId;    // lock
    }
    else if (!taskIdEqual(&(dataRecord->validationLock), &(record->taskId)))
    {
        // other task is doing validation, let it wait
        if (DEBUG)
        {
            print2uart("other is doing validation, %d %d\n", dataRecord->validationLock.nodeAddr, dataRecord->validationLock.id);
        }
        return;
    }

    TimeInterval_t ti = calcValidInterval(packet->taskId, packet->data.dataId,
                                          record->vPhase1DataBegin + record->writeSetNum - 1);
    sendValidationPhase1Response(&(packet->taskId), &(packet->data.dataId), &ti, pdTRUE);
}



void handleValidationPhase2Response(ValidationP2ResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->writeSet[i].dataId.owner == packet->ownerAddr)
        {
            record->validationPassed[i] = pdTRUE;
            break;
        }
    }
}

void handleCommitPhaseRequest(CommitRequestPacket_t *packet)
{
    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->writeSet[i].dataId.owner == nodeAddr)
        {
            Data_t *writeData = record->writeSet + i;
            Data_t* dataRecord = getDataRecord(writeData->dataId, nvmdb);
            // TODO: Commit
            // unlock
            dataRecord->validationLock = (TaskUUID_t){.nodeAddr=0, .id=0};
            sendCommitPhaseResponse(&(packet->taskId), &(writeData->dataId));
            //break;
        }
    }

}

void handleCommitPhaseResponse(CommitResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataId)))
        {
            record->commitDone[i] = pdTRUE;
            break;
        }
    }
}

TimeInterval_t calcValidInterval(TaskUUID_t taskId, DataUUID_t dataId, unsigned long long *dataBeginRecord)
{
    TimeInterval_t taskInterval = {.vBegin = 0, .vEnd = UINT64_MAX};
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (taskAccessObjectLog[i].validLog == pdTRUE &&
            taskIdEqual(&(taskAccessObjectLog[i].taskId), &taskId))
        {
            taskInterval.vEnd = min(taskInterval.vEnd, taskAccessObjectLog[i].writeSetReadBegin - 1);
        }
        break;
    }

    // write set
    unsigned long long dataBegin = getDataBegin(dataId);
    taskInterval.vBegin = max(taskInterval.vBegin, dataBegin + 1);

    // og down validation log
    if (dataBeginRecord != NULL)
    {
        *dataBeginRecord = dataBegin;
    }

    return taskInterval;
}

InboundValidationRecord_t *getOrCreateInboundRecord(TaskUUID_t *taskId)
{
    InboundValidationRecord_t *firstNull = NULL;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if(inboundValidationRecords[i].validRecord)
        {
            if (taskIdEqual(&(inboundValidationRecords[i].taskId), taskId))
            {
                return inboundValidationRecords + i;
            }
        }
        else
        {
            if(firstNull == NULL)
            {
                firstNull = inboundValidationRecords + i;
            }
        }

    }
    return firstNull;
}

OutboundValidationRecord_t *getOutboundRecord(TaskUUID_t taskId)
{
    OutboundValidationRecord_t *record = NULL;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        record = outboundValidationRecords + i;
        if(record->validRecord == 1)
        {
            if(taskIdEqual(&(record->taskId), &taskId))
            {
                return record;
            }
        }
    }

    return NULL;
}

InboundValidationRecord_t *getInboundRecord(TaskUUID_t *taskId)
{
    InboundValidationRecord_t *record = NULL;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        record = inboundValidationRecords + i;
        if(record->validRecord == pdTRUE)
        {
            if(taskIdEqual(&(record->taskId), taskId))
            {
                return record;
            }
        }
    }
    return NULL;

}
