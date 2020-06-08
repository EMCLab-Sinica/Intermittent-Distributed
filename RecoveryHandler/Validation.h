#ifndef VALIDATION_H
#define VALIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include "SimpDB.h"

typedef enum ValidationStage
{
	validationPhase1,
	commitPhase1,
	validationPhase2,
	commitPhase2

} ValidationStage_e;

typedef struct RemoteValidationRecord
{
	TaskUUID_t taskId;
	ValidationStage_e stage;

} RemoteValidationRecord_t;

typedef struct MyValidationRecord
{

} MyValidationRecord_t;

typedef struct TimeInterval
{
	uint64_t vBegin;
	uint64_t vEnd;

} TimeInterval_t;

void taskCommit(uint8_t taskId, uint8_t commitNum, ...);
TimeInterval_t calcValidInterval(DataUUID_t dataId, TimeInterval_t taskInterval);


#endif
