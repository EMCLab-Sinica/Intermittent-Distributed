#include "Validation.h"
#include "config.h"
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include "RFHandler.h"
#include <stdbool.h>

#pragma NOINIT(validationRequestPacketsQueue)
QueueHandle_t validationRequestPacketsQueue;

#pragma NOINIT(outboundValidationRecords)
OutboundValidationRecord_t outboundValidationRecords[MAX_GLOBAL_TASKS];

#pragma NOINIT(inboundValidationRecords)
InboundValidationRecord_t inboundValidationRecords[MAX_GLOBAL_TASKS];

extern uint8_t nodeAddr;
extern TaskAccessObjectLog_t taskAccessObjectLog[MAX_GLOBAL_TASKS];

void sendValidationPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit);
void sendValidationPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, Data_t *data, TimeInterval_t *timeInterval);
void sendCommitPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit);
void sendCommitPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, DataUUID_t *dataId, uint8_t maybeCommit);
void sendValidationPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision);
void sendValidationPhase2Response(uint16_t nodeAddr, TaskUUID_t taskId, bool passed);
InboundValidationRecord_t *getOrCreateInboundRecord(TaskUUID_t *taskId);
InboundValidationRecord_t *getInboundRecord(TaskUUID_t *taskId);
OutboundValidationRecord_t *getOutboundRecord(TaskUUID_t taskId);
TimeInterval_t calcValidInterval(TaskUUID_t taskId, DataUUID_t dataId, unsigned long long *dataBeginRecord);

void initValidationEssentials()
{
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        outboundValidationRecords[i].validRecord = pdFALSE;
        outboundValidationRecords[i].writeSetNum = 0;
    }
}

void taskCommit(uint8_t tid, uint8_t commitNum, ...)
{

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
    if (commitNum > 0)
    {
        Data_t *data;
        Data_t *currentWriteSet;
        va_list vl;
        va_start(vl, commitNum);
        for (uint8_t i = 0; i < MAX_TASK_READ_OBJ; i++)
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
    currentLog->stage = validationPhase1;
    currentLog->taskId = taskId;
    currentLog->validRecord = pdTRUE;

    // task sleep wait for validation and commit
    ulTaskNotifyTake(pdTRUE,         /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */
}

void remoteValidationTask()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    bool hasPacket = pdFALSE;
    PacketHeader_t *packetHeader = NULL;

    OutboundValidationRecord_t *outboundRecord = NULL;
    while (1)
    {
        // Inbound Validation
        hasPacket = xQueueReceive(validationRequestPacketsQueue, (void *)packetBuf, 0);
        if (hasPacket)
        {
            packetHeader = (PacketHeader_t *)packetBuf;
            switch (packetHeader->packetType)
            {
                case VPhase1Request:
                {
                    VPhase1RequestPacket_t *packet = (VPhase1RequestPacket_t *)packetBuf;
                    InboundValidationRecord_t *inboundRecord = getOrCreateInboundRecord(&(packet->taskId));
                    if (!inboundRecord->validRecord)    // not found, create new record
                    {
                        inboundRecord->taskId = packet->taskId;
                        inboundRecord->writeSet[inboundRecord->writeSetNum] = packet->dataWOContent;
                        inboundRecord->writeSetNum++;
                        inboundRecord->stage = validationPhase1;
                        inboundRecord->validRecord = pdTRUE;
                    }
                    TimeInterval_t ti = calcValidInterval(packet->taskId, packet->dataWOContent.dataId,
                                                          inboundRecord->vPhase1DataBegin + inboundRecord->writeSetNum - 1);
                    sendValidationPhase1Response(packet->header.txAddr, &(packet->taskId), &(packet->dataWOContent), &ti);
                }
                case VPhase1Response:
                {
                    VPhase1ResponsePacket_t *packet = (VPhase1ResponsePacket_t *)packetBuf;
                    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
                    record->taskValidInterval.vBegin = max(record->taskValidInterval.vBegin, packet->taskInterval.vBegin);
                    record->taskValidInterval.vEnd = min(record->taskValidInterval.vEnd, packet->taskInterval.vEnd);
                    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
                    {
                        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataWOContent.dataId)))
                        {
                            record->validationPhase1VIShrinked[i] = pdTRUE;
                            break;
                        }
                    }
                }

                case CPhase1Request:
                {
                    CPhase1RequestPacket_t *packet = (CPhase1RequestPacket_t *)packetBuf;
                    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
                    for (unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
                    {
                        if (dataIdEqual(&(record->writeSet[i].dataId), &(packet->dataWOContent.dataId)))
                        {
                            memcpy(record->writeSet[i].ptr, packet->dataPayload, packet->dataWOContent.size);
                            break;
                        }
                    }
                    sendCommitPhase1Response(packet->header.txAddr, &(packet->taskId), &(packet->dataWOContent.dataId), 1);
                    break;
                }

                case CPhase1Response:
                {
                    CPhase1ResponsePacket_t *packet = (CPhase1ResponsePacket_t *)packetBuf;
                    OutboundValidationRecord_t *record = getOutboundRecord(packet->taskId);
                    if(packet->maybeCommit == false)
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
                    break;
                }

                case VPhase2Request:
                {
                    bool VPhase2Passed = true;
                    VPhase2RequestPacket_t *packet = (VPhase2RequestPacket_t *)packetBuf;
                    InboundValidationRecord_t *record = getInboundRecord(&(packet->taskId));
                    for(unsigned int i = 0; i < MAX_TASK_READ_OBJ; i++)
                    {
                        if(record->writeSet[i].dataId.owner == nodeAddr)
                        {
                            // FIXME: lock the object
                            // check if modified after validation phase1
                            if(record->vPhase1DataBegin[i] < getDataBegin(record->writeSet[i].dataId))
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

                    break;
                }

                case VPhase2Response:
                {
                    break;
                }
            }
        }

        // Outbound Validation
        for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
        {
            outboundRecord = outboundValidationRecords + i;
            if (!outboundRecord->validRecord)
            {
                continue;
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

// functions for outbound validation
void sendValidationPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit)
{
    VPhase1RequestPacket_t packet;
    packet.taskId = taskId;
    packet.dataWOContent = *dataToCommit;
    packet.dataWOContent.ptr = NULL;
    packet.header.packetType = VPhase1Request;

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, Data_t *data, TimeInterval_t *timeInterval)
{
    VPhase1ResponsePacket_t packet;
    packet.taskId = *taskId;
    packet.dataWOContent = *data;
    packet.taskInterval = *timeInterval;
    packet.header.packetType = VPhase1Response;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase1Request(TaskUUID_t taskId, Data_t *dataToCommit)
{
    CPhase1RequestPacket_t packet;
    packet.taskId = taskId;
    packet.dataWOContent = *dataToCommit;
    packet.dataWOContent.ptr = NULL;
    memcpy(packet.dataPayload, dataToCommit->ptr, dataToCommit->size);
    packet.header.packetType = CPhase1Request;

    RFSendPacket(dataToCommit->dataId.owner, (uint8_t *)&packet, sizeof(packet));
}

void sendCommitPhase1Response(uint8_t nodeAddr, TaskUUID_t *taskId, DataUUID_t *dataId, uint8_t maybeCommit)
{
    CPhase1ResponsePacket_t packet;
    packet.taskId = *taskId;
    packet.dataId = *dataId;
    packet.header.packetType = CPhase1Response;
    packet.maybeCommit = maybeCommit;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Request(uint8_t nodeAddr, TaskUUID_t taskId, bool decision)
{
    VPhase2RequestPacket_t packet;
    packet.taskId = taskId;
    packet.decision = decision;
    packet.header.packetType = VPhase2Request;

    RFSendPacket(nodeAddr, (uint8_t *)&packet, sizeof(packet));
}

void sendValidationPhase2Response(uint16_t nodeAddr, TaskUUID_t taskId, bool passed)
{

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
