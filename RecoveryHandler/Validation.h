#ifndef VALIDATION_H
#define VALIDATION_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <stdbool.h>
#include "SimpDB.h"
#include "mylist.h"

typedef struct TimeInterval
{
    uint64_t vBegin;
    uint64_t vEnd;

} TimeInterval_t;

typedef enum ValidationStage
{
    validationPhase1,
    commitPhase1,
    validationPhase2,
    commitPhase2

} ValidationStage_e;

typedef struct OutboundValidationRecord
{
    bool validRecord;
    ValidationStage_e stage;
    TaskUUID_t taskId;
    uint8_t writeSetNum;
    TaskHandle_t taskHandle;
    Data_t writeSet[MAX_TASK_READ_OBJ];
    TimeInterval_t taskValidInterval;

    bool validationPhase1VIShrinked[MAX_TASK_READ_OBJ]; // VI = valid interval
    bool commitPhase1Replied[MAX_TASK_READ_OBJ];
    bool validationPhase2Passed[MAX_TASK_READ_OBJ];
    bool commitPhase2Done[MAX_TASK_READ_OBJ];

} OutboundValidationRecord_t;

 typedef struct InboundValidationRecord
 {
    bool validRecord;
    ValidationStage_e stage;
    TaskUUID_t taskId;
    uint8_t writeSetNum;
    // FIXME: not sharable modified version
    Data_t writeSet[MAX_TASK_READ_OBJ];
    uint64_t vPhase1DataBegin[MAX_TASK_READ_OBJ];

 } InboundValidationRecord_t;

void taskCommit(uint8_t taskId, uint8_t commitNum, ...);

void remoteValidationTask();

#endif
