#include "FreeRTOS.h"
#include "RecoveryHandler/TaskControl.h"
#include "SimpDB.h"
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
extern TaskAccessLog_t accessLog[MAX_LOCAL_TASKS];

void sendValidationRequest(TaskUUID_t *taskId, ValidateObject_t *dataToCommit);
void sendValidationResponse(TaskUUID_t *taskId, DataUUID_t *dataId, TimeInterval_t *timeInterval);
void sendCommitRequest(uint8_t dataOwner, TaskUUID_t *taskId, uint8_t decision);
void sendCommitResponse(TaskUUID_t *taskId, DataUUID_t *dataId);

void handleValidationRequest(ValidationRequestPacket_t *packet);
void handleValidationResponse(ValidationResponsePacket_t *packet);
void handleCommitRequest(CommitRequestPacket_t *packet);
void handleCommitResponse(CommitResponsePacket_t *packet);

InboundValidationRecord_t *getOrCreateInboundRecord(TaskUUID_t *taskId);
InboundValidationRecord_t *getInboundRecord(TaskUUID_t *taskId);
OutboundValidationRecord_t *getOutboundRecord(TaskUUID_t taskId);
TimeInterval_t calcValidInterval(TaskUUID_t taskId, DataUUID_t dataId);

void initValidationEssentials()
{
    OutboundValidationRecord_t *outRecord = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        outRecord = outboundValidationRecords + i;
        for(int i = 0; i < MAX_TASK_READ_OBJ; i++)
        {
            // maximum of data size
            outRecord->RWSet[i].data.ptr = pvPortMalloc(sizeof(int32_t));
            outRecord->RWSet[i].valid = 0;
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

void taskCommit(uint8_t tid, TaskHandle_t *fromTask, int32_t commitNum, ...)
{
    if(DEBUG)
        print2uart("Task %d, request to commit\n", tid);

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
    ValidateObject_t *currentObject;
    if (commitNum > 0)
    {
        Data_t *data;
        va_list vl;
        va_start(vl, commitNum);
        for (uint8_t i = 0; i < commitNum; i++)
        {
            data = va_arg(vl, Data_t *);
            currentObject = currentLog->RWSet + i;
            void *ptrToNVM = currentObject->data.ptr;
            currentObject->data = *data;   // copy the data object to NVM
            currentObject->data.ptr = ptrToNVM;
            memcpy(currentObject->data.ptr, data->ptr, data->size);    // copy the data content
            currentObject->data.version = modified;
            currentLog->writeSetNum++;
            if(DEBUG)
            {
                print2uart("Commit data (%d, %d) added\n", data->dataId.owner, data->dataId.id);
            }
            // remove from readSet
            for (int i = 0; i < MAX_TASK_READ_OBJ; i++)
            {
                if (dataIdEqual(&(accessLog[tid].readSet[i]), &(data->dataId)))
                {
                    accessLog[tid].readSet[i].id = 0;
                }
            }
        }
        va_end(vl);
    }

    // save readSet to log
    for (int i = commitNum; i < MAX_TASK_READ_OBJ; i++)
    {
        if (accessLog[tid].readSet[i].id != 0)
        {
            DataUUID_t dataId = accessLog[tid].readSet[i];
            currentObject = currentLog->RWSet + i;
            currentObject->data.dataId = dataId;
            currentObject->mode = ro;
            currentObject->valid = 1;
            currentLog->writeSetNum++;
            if(DEBUG)
            {
                print2uart("Read data(%d, %d) added for\n", dataId.owner, dataId.id);
            }
        }
    }

    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = tid};
    currentLog->taskId = taskId;
    currentLog->taskHandle = fromTask;
    currentLog->stage = validationPhase;
    // atomic operation
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
                    ValidationRequestPacket_t *packet = (ValidationRequestPacket_t *)packetBuf;
                    handleValidationRequest(packet);
                    break;
                }
                case ValidationResponse:
                {
                    ValidationResponsePacket_t *packet = (ValidationResponsePacket_t *)packetBuf;
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

                default:
                {
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
                print2uart("Record :%d, Validation: task: %d, stage: %d\n",
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

                            if (outboundRecord->RWSet[i].data.dataId.owner == nodeAddr) // local
                            {
                                // validate on local
                                TimeInterval_t ti = calcValidInterval(outboundRecord->taskId, outboundRecord->RWSet[i].data.dataId);
                                outboundRecord->taskValidInterval.vBegin = max(outboundRecord->taskValidInterval.vBegin, ti.vBegin);
                                outboundRecord->taskValidInterval.vEnd = min(outboundRecord->taskValidInterval.vEnd, ti.vEnd);
                                outboundRecord->validationPassed[i] = 1;
                            }
                            else
                            {
                                sendValidationRequest(&(outboundRecord->taskId), outboundRecord->RWSet + i);
                            }

                        }
                    }
                    if (toNextStage == pdTRUE)
                    {
                        outboundRecord->stage = commitPhase;
                        /*
                        if (outboundRecord->taskValidInterval.vBegin <= outboundRecord->taskValidInterval.vEnd)
                        {
                            outboundRecord->stage = commitPhase;
                        } else
                        {
                            print2uart("abort\n");
                            print2uart("%u\n",outboundRecord->taskValidInterval.vBegin );
                            print2uart("%u\n", outboundRecord->taskValidInterval.vEnd);
                            TaskRecord_t* task = (TaskRecord_t*)(outboundRecord->taskRecord);
                            vTaskDelete(task->TCB); //delete the current task
                            task->taskStatus = invalid;
                            xTaskCreate(task->address, task->taskName, task->stackSize, NULL, task->priority, NULL);
                            outboundRecord->validRecord = false;
                        }
                        */
                        break;
                    }
                }
                case commitPhase:
                {
                    uint8_t toNextStage = pdTRUE;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->commitDone[i] == 0)
                        {
                            toNextStage = pdFALSE;
                            if (outboundRecord->RWSet[i].mode == ro)    // read only, not commit needed
                            {
                                outboundRecord->commitDone[i] = 1;
                            }
                            else {
                                if (outboundRecord->RWSet[i] .data.dataId.owner == nodeAddr)  // local
                                {
                                    // commit on local
                                    commitLocalDB(&(outboundRecord->RWSet[i].data), outboundRecord->RWSet[i].data.size);
                                    outboundRecord->commitDone[i] = 1;
                                } else {
                                    sendCommitRequest(outboundRecord->RWSet[i]
                                                          .data.dataId.owner,
                                                      &(outboundRecord->taskId),
                                                      pdTRUE);
                                }
                            }
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
void sendValidationRequest(TaskUUID_t* taskId, ValidateObject_t *validateObject)
{
    ValidationRequestPacket_t packet;
    packet.header.packetType = ValidationRequest;
    packet.taskId = *taskId;
    Data_t* dataToCommit = &(validateObject->data);
    packet.data.dataId = dataToCommit->dataId;
    if (validateObject->mode == rw)  // not readSet
    {
        packet.mode = rw;
        packet.data.version = dataToCommit->version;
        packet.data.size = dataToCommit->size;
        memcpy(packet.data.content, dataToCommit->ptr, dataToCommit->size);
    } else
    {
        packet.mode = ro;
        packet.data.size = 0;
        memcpy(packet.data.content, 0, dataToCommit->size);
    }

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationResponse(TaskUUID_t *taskId, DataUUID_t *dataId, TimeInterval_t *timeInterval)
{
    ValidationResponsePacket_t packet;
    packet.header.packetType = ValidationResponse;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    packet.taskInterval = *timeInterval;

    RFSendPacket(taskId->nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitRequest(uint8_t dataOwner, TaskUUID_t *taskId, uint8_t decision)
{
    CommitRequestPacket_t packet;
    packet.header.packetType = CommitRequest;
    packet.taskId = *taskId;
    packet.decision = decision;

    RFSendPacket(dataOwner, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitResponse(TaskUUID_t *taskId, DataUUID_t *dataId)
{
    CommitResponsePacket_t packet;
    packet.header.packetType = CommitResponse;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    RFSendPacket(taskId->nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void handleValidationRequest(ValidationRequestPacket_t *packet)
{
    // find a new place
    InboundValidationRecord_t *record= getOrCreateInboundRecord(&(packet->taskId));
    record->taskId = packet->taskId;
    record->validRecord = pdTRUE;

    ValidateObject_t *entry = record->RWSet + packet->data.dataId.id;
    entry->valid = 0;
    Data_t *validateData = &(entry->data);
    validateData->dataId = packet->data.dataId;
    if (packet->mode == rw) // writeSet
    {
        validateData->version = packet->data.version;
        validateData->size = packet->data.size;
        memcpy(validateData->ptr, packet->data.content, packet->data.size);
    }
    else    // read only
    {
        validateData->version = consistent;
        validateData->size = 0;
        validateData->ptr = NULL;
    }

    Data_t *dataRecord = getDataRecord(packet->data.dataId, nvmdb);
    if (dataRecord->validationLock.nodeAddr == 0)    // nullTask, not locked
    {
        if (DEBUG)
        {
            print2uart("Not locked, get lock\n");
        }
        dataRecord->validationLock = record->taskId;    // lock
        entry->valid = 1;
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

    TimeInterval_t ti = calcValidInterval(packet->taskId, packet->data.dataId);

    sendValidationResponse(&(packet->taskId), &(packet->data.dataId), &ti);
}

void handleValidationResponse(ValidationResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    record->taskValidInterval.vBegin = max(record->taskValidInterval.vBegin, packet->taskInterval.vBegin);
    record->taskValidInterval.vEnd = min(record->taskValidInterval.vEnd, packet->taskInterval.vEnd);
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (dataIdEqual(&(record->RWSet[i].data.dataId), &(packet->dataId)))
        {
            record->validationPassed[i] = pdTRUE;
            break;
        }
    }
}

void handleCommitRequest(CommitRequestPacket_t *packet)
{
    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->RWSet[i].data.dataId.owner == nodeAddr)
        {
            Data_t *writeData = &(record->RWSet[i].data);
            Data_t* dataRecord = getDataRecord(writeData->dataId, nvmdb);
            // TODO: Commit
            // unlock
            dataRecord->validationLock = (TaskUUID_t){.nodeAddr=0, .id=0};
            sendCommitResponse(&(packet->taskId), &(writeData->dataId));
            //break;
        }
    }

}

void handleCommitResponse(CommitResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (dataIdEqual(&(record->RWSet[i].data.dataId), &(packet->dataId)))
        {
            record->commitDone[i] = pdTRUE;
            break;
        }
    }
}

TimeInterval_t calcValidInterval(TaskUUID_t taskId, DataUUID_t dataId)
{
    // need if modified
    // reader task
    TimeInterval_t taskInterval = {.vBegin = 0, .vEnd = timeCounter};
    Data_t* dataRecord = getDataRecord(dataId, nvmdb);
    // FIXME: here
    if (dataRecord->accessLog.WARBegins[dataRecord->accessLog.pos] > 1 )// modified by some other task
    {
        taskInterval.vEnd = min(taskInterval.vEnd, getDataBegin(dataId) - 1);
    }

    // write set
    unsigned long long dataBegin = getDataBegin(dataId);
    if (1)  // last modified
    {
        taskInterval.vBegin = max(taskInterval.vBegin, getDataBegin(dataId) + 1);
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
