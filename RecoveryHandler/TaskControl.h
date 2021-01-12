#ifndef TASK_CONTROL_H
#define TASK_CONTROL_H

#include "DataManager/SimpDB.h"

typedef struct TimeInterval
{
    uint32_t vBegin;
    uint32_t vEnd;

} TimeInterval_t;

typedef enum ValidationStage
{
    validationPhase,
    commitPhase,
    finish

} ValidationStage_e;

typedef enum DataModType
{
    ro,
    rw
}DataModType_e;

typedef struct ValidateObject
{
    uint8_t valid;
    DataModType_e mode;   // 0: read-only, 1: r/w
    Data_t data;
} ValidateObject_t;

typedef struct OutboundValidationRecord
{
    uint8_t validRecord;
    ValidationStage_e stage;
    TaskUUID_t taskId;
    uint8_t writeSetNum;
    TaskHandle_t *taskHandle;
    ValidateObject_t RWSet[MAX_TASK_READ_OBJ];  // read/write set
    TimeInterval_t taskValidInterval;
    void* taskRecord;

    uint8_t validationPassed[MAX_TASK_READ_OBJ];
    uint8_t commitDone[MAX_TASK_READ_OBJ];

} OutboundValidationRecord_t;

 typedef struct InboundValidationRecord
 {
    uint8_t validRecord;
    TaskUUID_t taskId;
    uint8_t writeSetNum;
    // FIXME: not sharable modified version
    ValidateObject_t RWSet[MAX_TASK_READ_OBJ];  // read/write set
    uint32_t vPhase1DataBegin[MAX_TASK_READ_OBJ];

 } InboundValidationRecord_t;

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

#endif
