#include "Validation.h"
#include "config.h"
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include "RFHandler.h"
#include <stdbool.h>
#include "myuart.h"
#include <string.h>

#define DEBUG 1
#define INFO 1

#pragma NOINIT(validationRequestPacketsQueue)
QueueHandle_t validationRequestPacketsQueue;

#pragma NOINIT(outboundValidationRecords)
OutboundValidationRecord_t outboundValidationRecords[MAX_GLOBAL_TASKS];

#pragma NOINIT(inboundValidationRecords)
InboundValidationRecord_t inboundValidationRecords[MAX_GLOBAL_TASKS];

extern uint8_t nodeAddr;
extern int firstTime;
extern TaskAccessObjectLog_t taskAccessObjectLog[MAX_GLOBAL_TASKS];

void sendValidationPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit);
void sendValidationPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, Data_t *data, TimeInterval_t *timeInterval);
void sendCommitPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit);
void sendCommitPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, DataUUID_t *dataId, uint8_t maybeCommit);
void sendValidationPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision);
void sendValidationPhase2Response(uint8_t dstNodeAddr, TaskUUID_t taskId, bool passed);
void sendCommitPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision);
void sendCommitPhase2Response(uint8_t nodeAddr, TaskUUID_t, DataUUID_t dataId);

void handleValidationPhase1Request(ValidationP1RequestPacket_t *packet);
void handleValidationPhase1Response(ValidationP1ResponsePacket_t *packet);
void handleCommitPhase1Request(CommitP1RequestPacket_t *packet);
void handleCommitPhase1Response(CommitP1ResponsePacket_t *packet);
void handleValidationPhase2Request(ValidationP2RequestPacket_t *packet);
void handleValidationPhase2Response(ValidationP2ResponsePacket_t *packet);
void handleCommitPhase2Request(CommitP2RequestPacket_t *packet);
void handleCommitPhase2Response(CommitP2ResponsePacket_t *packet);

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
        outRecord->validRecord = pdFALSE;
        outRecord->writeSetNum = 0;
        memset(outRecord->validationPhase1VIShrinked, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
        memset(outRecord->commitPhase1Replied, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
        memset(outRecord->validationPhase2Passed, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
        memset(outRecord->commitPhase2Done, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
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

    return TRUE;
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
            *currentWriteSet = *data;
            currentWriteSet->ptr = pvPortMalloc(sizeof(data->size));
            memcpy(currentWriteSet->ptr, data->ptr, data->size);
            currentWriteSet->version = modified;

            currentLog->writeSetNum++;
        }
        va_end(vl);
    }
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = tid};
    currentLog->taskId = taskId;
    currentLog->taskHandle = fromTask;
    currentLog->writeSetNum = commitNum;
    currentLog->stage = validationPhase1;
    currentLog->validRecord = pdTRUE;

    if(DEBUG)
    {
        print2uart("Validation: VP1 started\n");
    }

    // task sleep wait for validation and commit
    ulTaskNotifyTake(pdTRUE,         /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */
}

void inboundValidationHandler()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    bool hasPacket = pdFALSE;
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
                case ValidationP1Request:
                {
                    ValidationP1RequestPacket_t *packet = (ValidationP1RequestPacket_t *)packetBuf;
                    handleValidationPhase1Request(packet);
                    break;
                }
                case ValidationP1Response:
                {
                    ValidationP1ResponsePacket_t *packet = (ValidationP1ResponsePacket_t *)packetBuf;
                    handleValidationPhase1Response(packet);
                    break;
                }

                case CommitP1Request:
                {
                    CommitP1RequestPacket_t *packet = (CommitP1RequestPacket_t *)packetBuf;
                    handleCommitPhase1Request(packet);
                    break;
                }

                case CommitP1Response:
                {
                    CommitP1ResponsePacket_t *packet = (CommitP1ResponsePacket_t *)packetBuf;
                    handleCommitPhase1Response(packet);
                    break;
                }

                case ValidationP2Request:
                {
                    ValidationP2RequestPacket_t *packet = (ValidationP2RequestPacket_t *)packetBuf;
                    handleValidationPhase2Request(packet);
                    break;
                }

                case ValidationP2Response:
                {
                    ValidationP2ResponsePacket_t *packet = (ValidationP2ResponsePacket_t *)packetBuf;
                    handleValidationPhase2Response(packet);
                    break;
                }

                case CommitP2Request:
                {
                    CommitP2RequestPacket_t *packet = (CommitP2RequestPacket_t *)packetBuf;
                    handleCommitPhase2Request(packet);
                    break;
                }

                case CommitP2Response:
                {
                    CommitP2ResponsePacket_t *packet = (CommitP2ResponsePacket_t *)packetBuf;
                    handleCommitPhase2Response(packet);
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
        vTaskDelay(600);
        // Outbound Validation
        for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
        {
            outboundRecord = outboundValidationRecords + i;
            if (!outboundRecord->validRecord)
            {
                continue;
            }
            if (DEBUG)
            {
                print2uart("Record Id:%d, Validation: task: %d, stage: %d\n",
                           i, outboundRecord->taskId.id, outboundRecord->stage);
            }

            switch (outboundRecord->stage)
            {
                case validationPhase1:
                {
                    bool toNextStage = true;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if (outboundRecord->validationPhase1VIShrinked[i] == 0)
                        {
                            toNextStage = false;
                            sendValidationPhase1Request(
                                outboundRecord->taskId, outboundRecord->writeSet + i);
                        }
                    }
                    if(toNextStage)
                    {
                        outboundRecord->stage = commitPhase1;
                    }

                    break;
                }

                case commitPhase1:
                {
                    bool toNextStage = true;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->commitPhase1Replied[i] == 0)
                        {
                            toNextStage = false;
                            sendCommitPhase1Request(outboundRecord->taskId, &(outboundRecord->writeSet[i]));
                        }
                    }
                    if(toNextStage)
                    {
                        outboundRecord->stage = validationPhase2;
                    }
                    break;
                }
                case validationPhase2:
                {
                    bool toNextStage = true;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->validationPhase2Passed[i] == 0)
                        {
                            toNextStage = false;
                            sendValidationPhase2Request(outboundRecord->writeSet[i].dataId.owner,
                                                        outboundRecord->taskId, true);
                        }
                    }
                    if (toNextStage == true)
                    {
                        outboundRecord->stage = commitPhase2;
                    }
                    break;
                }
                case commitPhase2:
                {
                    bool toNextStage = true;
                    for (unsigned int i = 0; i < outboundRecord->writeSetNum; i++)
                    {
                        if(outboundRecord->commitPhase2Done[i] == 0)
                        {
                            toNextStage = false;
                            sendCommitPhase2Request(outboundRecord->writeSet[i].dataId.owner,
                                                    outboundRecord->taskId, true);
                        }
                    }

                    if (toNextStage == true)
                    {
                        outboundRecord->stage = finish;
                    }
                    break;
                }

                case finish:
                {
                    xTaskNotifyGive(*outboundRecord->taskHandle);
                    outboundRecord->validRecord = false;
                    outboundRecord->writeSetNum = 0;
                    memset(outboundRecord->validationPhase1VIShrinked, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
                    memset(outboundRecord->commitPhase1Replied, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
                    memset(outboundRecord->validationPhase2Passed, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
                    memset(outboundRecord->commitPhase2Done, 0, sizeof(bool) * MAX_TASK_READ_OBJ);
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
void sendValidationPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit)
{
    ValidationP1RequestPacket_t packet;
    packet.taskId = taskId;
    packet.dataHeader.dataId = dataToCommit->dataId;
    packet.dataHeader.version = dataToCommit->version;
    packet.header.packetType = ValidationP1Request;

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, Data_t *data, TimeInterval_t *timeInterval)
{
    ValidationP1ResponsePacket_t packet;
    packet.taskId = *taskId;
    packet.dataHeader.dataId = data->dataId;
    packet.dataHeader.version = data->version;
    packet.taskInterval = *timeInterval;
    packet.header.packetType = ValidationP1Response;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit)
{
    CommitP1RequestPacket_t packet;
    packet.taskId = taskId;
    memcpy(&(packet.dataRecord), dataToCommit, sizeof(packet.dataRecord));
    memcpy(packet.dataContent, dataToCommit->ptr, dataToCommit->size);
    packet.header.packetType = CommitP1Request;

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, DataUUID_t *dataId, uint8_t maybeCommit)
{
    CommitP1ResponsePacket_t packet;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    packet.header.packetType = CommitP1Response;
    packet.maybeCommit = maybeCommit;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision)
{
    ValidationP2RequestPacket_t packet;
    packet.taskId = taskId;
    packet.decision = decision;
    packet.header.packetType = ValidationP2Request;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Response(uint8_t dstNodeAddr, TaskUUID_t taskId, bool passed)
{
    ValidationP2ResponsePacket_t packet;
    packet.taskId = taskId;
    packet.ownerAddr = nodeAddr;
    packet.finalValidationPassed = passed;
    packet.header.packetType = ValidationP2Response;

    RFSendPacket(dstNodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision)
{
    CommitP2RequestPacket_t packet;
    packet.taskId = taskId;
    packet.decision = decision;
    packet.header.packetType = CommitP2Request;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase2Response(uint8_t nodeAddr, TaskUUID_t taskId, DataUUID_t dataId)
{
    CommitP2ResponsePacket_t packet;
    packet.taskId = taskId;
    packet.dataId = dataId;
    packet.header.packetType = CommitP2Response;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

// functions for inbound validation
void handleValidationPhase1Request(ValidationP1RequestPacket_t *packet)
{
    InboundValidationRecord_t *inboundRecord = getOrCreateInboundRecord(&(packet->taskId));
    if (!inboundRecord->validRecord) // not found, create new record
    {
        inboundRecord->taskId = packet->taskId;
        inboundRecord->stage = validationPhase1;
        Data_t writeSet;
        writeSet.dataId = packet->dataHeader.dataId;
        writeSet.version = packet->dataHeader.version;
        inboundRecord->writeSet[inboundRecord->writeSetNum] = writeSet;
        inboundRecord->writeSetNum++;
        inboundRecord->validRecord = pdTRUE;
    }
    TimeInterval_t ti = calcValidInterval(packet->taskId, packet->dataHeader.dataId,
                                          inboundRecord->vPhase1DataBegin + inboundRecord->writeSetNum - 1);
    sendValidationPhase1Response(packet->header.txAddr, &(packet->taskId), &(packet->dataHeader), &ti);
}

void handleValidationPhase1Response(ValidationP1ResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    record->taskValidInterval.vBegin = max(record->taskValidInterval.vBegin, packet->taskInterval.vBegin);
    record->taskValidInterval.vEnd = min(record->taskValidInterval.vEnd, packet->taskInterval.vEnd);
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataHeader.dataId)))
        {
            record->validationPhase1VIShrinked[i] = pdTRUE;
            break;
        }
    }
}

void handleCommitPhase1Request(CommitP1RequestPacket_t *packet)
{
    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataRecord.dataId)))
        {
            record->writeSet[i].size = packet->dataRecord.size;
            memcpy(record->writeSet[i].ptr, packet->dataContent, packet->dataRecord.size);
            break;
        }
    }
    sendCommitPhase1Response(packet->header.txAddr, &(packet->taskId), &(packet->dataRecord.dataId), 1);
}

void handleCommitPhase1Response(CommitP1ResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    if (packet->maybeCommit == false)
    {
        // TODO: redo validation
        return;
    }
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataId)))
        {
            record->commitPhase1Replied[i] = true;
            break;
        }
    }
}

void handleValidationPhase2Request(ValidationP2RequestPacket_t *packet)
{
    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));

    bool VPhase2Passed = true;
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->writeSet[i].dataId.owner == nodeAddr)
        {
            // FIXME: lock the object
            // check if modified after validation phase1
            if (record->vPhase1DataBegin[i] < getDataBegin(record->writeSet[i].dataId))
            {
                VPhase2Passed = false;
                break;
            }
        }
    }
    if (VPhase2Passed)
    {
        sendValidationPhase2Response(packet->header.txAddr, record->taskId, true);
    }
    else
    {
        sendValidationPhase2Response(packet->header.txAddr, record->taskId, false);
        // unlock
    }
}

void handleValidationPhase2Response(ValidationP2ResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->writeSet[i].dataId.owner == packet->ownerAddr)
        {
            record->validationPhase2Passed[i] = true;
            break;
        }
    }
}

void handleCommitPhase2Request(CommitP2RequestPacket_t *packet)
{
    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
    {
        if (record->writeSet[i].dataId.owner == nodeAddr)
        {
            // TODO: Commit
            sendCommitPhase2Response(packet->header.txAddr, packet->taskId,
                                     record->writeSet[i].dataId);
            break;
        }
    }

}

void handleCommitPhase2Response(CommitP2ResponsePacket_t *packet)
{
    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
    for (uint8_t i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataId)))
        {
            record->commitPhase2Done[i] = true;
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
        if(record->validRecord == 1)
        {
            if(taskIdEqual(&(record->taskId), taskId))
            {
                return record;
            }
        }
    }
    return NULL;

}
