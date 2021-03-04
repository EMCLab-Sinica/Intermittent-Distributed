#include "FreeRTOS.h"
#include "ValidationHandler/TaskControl.h"
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


#pragma NOINIT(outboundValidationRecords)
OutboundValidationRecord_t outboundValidationRecords[MAX_GLOBAL_TASKS];

#pragma NOINIT(inboundValidationRecords)
InboundValidationRecord_t inboundValidationRecords[MAX_GLOBAL_TASKS];

extern volatile TCB_t * volatile pxCurrentTCB;
extern uint8_t nodeAddr;
extern int firstTime;

QueueHandle_t validationRequestPacketsQueue;
uint8_t ucQueueStorageArea[5 * MAX_PACKET_LEN];
static StaticQueue_t xStaticQueue;

void sendValidationRequest(TaskUUID_t *taskId, ValidateObject_t *dataToCommit);
void sendValidationResponse(TaskUUID_t *taskId, DataUUID_t *dataId, TimeInterval_t *timeInterval);
void sendCommitRequest(uint8_t dataOwner, TaskUUID_t *taskId, uint8_t decision);
void sendCommitResponse(TaskUUID_t *taskId, DataUUID_t *dataId);

void handleValidationRequest(ValidationRequestPacket_t *packet);
void handleValidationResponse(ValidationResponsePacket_t *packet);
void handleCommitRequest(CommitRequestPacket_t *packet);
void handleCommitResponse(CommitResponsePacket_t *packet);
uint8_t resolveTaskDependency(TaskUUID_t taskId, DataUUID_t dataId);

InboundValidationRecord_t *getOrCreateInboundRecord(TaskUUID_t *taskId);
InboundValidationRecord_t *getInboundRecord(TaskUUID_t *taskId);
OutboundValidationRecord_t *getOutboundRecord(TaskUUID_t taskId);
TimeInterval_t calcValidInterval(DataUUID_t dataId, uint32_t readBegin, uint8_t has_write);

void initValidationEssentials()
{
    OutboundValidationRecord_t *outRecord = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        outRecord = outboundValidationRecords + i;
        memset(outRecord, 0, sizeof(OutboundValidationRecord_t));
        for(int i = 0; i < MAX_TASK_READ_OBJ; i++)
        {
            // maximum of data size
            outRecord->RWSet[i].data.ptr = pvPortMalloc(sizeof(int32_t));
            outRecord->RWSet[i].valid = 0;
        }
    }

    InboundValidationRecord_t *inRecord = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        inRecord = inboundValidationRecords + i;
        memset(inRecord, 0, sizeof(InboundValidationRecord_t));
    }
}

void taskCommit(uint8_t tid, TaskHandle_t *fromTask, int32_t commitNum, ...)
{
    taskENTER_CRITICAL();
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
    // clear
    currentLog->writeSetNum = 0;
    memset(currentLog->validationPassed, 0,
           sizeof(uint8_t) * MAX_TASK_READ_OBJ);
    memset(currentLog->commitDone, 0, sizeof(uint8_t) * MAX_TASK_READ_OBJ);

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

            // copy the data object to NVM
            void *ptrToNVM = currentObject->data.ptr;
            currentObject->data = *data;
            currentObject->data.ptr = ptrToNVM;
            memcpy(currentObject->data.ptr, data->ptr, data->size);    // copy the data content
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
    currentLog->stage = validationPhase;
    // atomic operation
    currentLog->validRecord = pdTRUE;
    taskEXIT_CRITICAL();

    // task sleep wait for validation and commit
    ulTaskNotifyTake(pdTRUE,         /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */
}

void inboundValidationHandler()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    uint8_t hasPacket = pdFALSE;
    PacketHeader_t *packetHeader = NULL;

    validationRequestPacketsQueue = xQueueCreateStatic(5, MAX_PACKET_LEN, ucQueueStorageArea, &xStaticQueue);
    if (validationRequestPacketsQueue == NULL) {
        print2uart("Error: DB Service Routine Queue init failed\n");
    }

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
        vTaskDelay(500);
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

                            if (outboundRecord->RWSet[i].data.dataId.owner ==
                                nodeAddr)  // local
                            {
                                // validate on local
                                Data_t *data = &(outboundRecord->RWSet[i].data);
                                TimeInterval_t ti =
                                    calcValidInterval(data->dataId, data->begin,
                                                      data->version == working);
                                outboundRecord->taskValidInterval.vBegin = max(
                                    outboundRecord->taskValidInterval.vBegin,
                                    ti.vBegin);
                                outboundRecord->taskValidInterval.vEnd =
                                    min(outboundRecord->taskValidInterval.vEnd,
                                        ti.vEnd);
                                outboundRecord->validationPassed[i] = 1;
                            } else {
                                sendValidationRequest(&(outboundRecord->taskId), outboundRecord->RWSet + i);
                            }
                        }
                    }
                    if (toNextStage == pdTRUE)
                    {
                        if (outboundRecord->taskValidInterval.vBegin <= outboundRecord->taskValidInterval.vEnd)
                        {
                            outboundRecord->stage = commitPhase;
                        } else
                        {
                            print2uart("abort\n");
                            // The memory of aborted task will be cleaned up on next recovery
                            // send abort the task
                            sendCommitRequest(outboundRecord->RWSet[i] .data.dataId.owner, &(outboundRecord->taskId), 0);
                            outboundRecord->validRecord = false;
                        }
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
                            if (outboundRecord->RWSet[i].data.version != working)    // read only, not commit needed
                            {
                                outboundRecord->commitDone[i] = 1;
                            }
                            else {
                                if (outboundRecord->RWSet[i] .data.dataId.owner == nodeAddr)  // local
                                {
                                    // commit on local
                                    commitLocalDB( outboundRecord->taskId, &(outboundRecord->RWSet[i].data));
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
                    outboundRecord->validRecord = pdFALSE;
                    if (outboundRecord->taskHandle != NULL)
                    {
                        xTaskNotifyGive(*(outboundRecord->taskHandle));
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
    if (validateObject->data.version != working)  // not readSet
    {
        packet.data.version = dataToCommit->version;
        packet.data.size = dataToCommit->size;
        memcpy(packet.data.content, dataToCommit->ptr, dataToCommit->size);
    } else
    {
        packet.data.size = 0;
        memset(packet.data.content, 0, dataToCommit->size);
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

    if (validateData->version == modified) // resolve dependency
    {
        TaskCommitted_t resolve = resolveTaskDependency(packet->taskId, validateData->dataId);
        if (resolve == aborted)
        {
            // send invalid TimeInterval_t, meaning abort
            TimeInterval_t ti = {.vBegin = 1, .vEnd = -1};
            sendValidationResponse(&(packet->taskId), &(packet->data.dataId), &ti);
        }else if (resolve == pending)
        {
            // return and wait
            return;
        }
    }

    if (validateData->version != duplicated) // writeSet
    {
        validateData->version = packet->data.version;
        validateData->size = packet->data.size;
        memcpy(validateData->ptr, packet->data.content, packet->data.size);
    }
    else    // read only
    {
        validateData->version = duplicated;
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

    TimeInterval_t ti = calcValidInterval(packet->data.dataId, packet->data.begin, packet->data.version == working);
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

TimeInterval_t calcValidInterval(DataUUID_t dataId, uint32_t readBegin, uint8_t has_write)
{
    TimeInterval_t taskInterval = {.vBegin = 0, .vEnd = timeCounter};
    Data_t* dataRecord = getDataRecord(dataId, nvmdb);
    uint32_t latestBegin = getDataBegin(dataId);

    if (readBegin < latestBegin) // modified by some other task
    {
        uint32_t firstCommittedBegin = getFirstCommitedBegin(dataId.id, readBegin);
        taskInterval.vEnd = min(taskInterval.vEnd, firstCommittedBegin - 1);
    }

    // if written
    if (has_write)
    {
        if (readBegin < latestBegin) // modified by some other task
        {
            taskInterval.vBegin = max(taskInterval.vBegin, getDataBegin(dataId) + 1);
        }
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

uint8_t resolveTaskDependency(TaskUUID_t taskId, DataUUID_t dataId)
{
    TaskCommitted_t committed = checkCommitted(taskId, dataId.id);
    return committed;
}

void recoverOutboundValidation() {
    OutboundValidationRecord_t *record = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        record = outboundValidationRecords + i;
        if(record->validRecord == pdTRUE)
        {
            record->taskHandle = NULL;
        }

    }
}
