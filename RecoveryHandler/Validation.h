#ifndef VALIDATION_H
#define VALIDATION_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include "SimpDB.h"
#include "TaskControl.h"

 void taskCommit(uint8_t tid, TaskHandle_t *fromTask, uint8_t commitNum, ...);
 void initValidationEssentials();
uint8_t initValidationQueues();

void outboundValidationHandler();
void inboundValidationHandler();

#endif
