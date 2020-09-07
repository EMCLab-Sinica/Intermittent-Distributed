/*
 * Recovery.c
 *
 *  Created on: 2018�~2��12��
 *      Author: Meenchen
 */

#include <RecoveryHandler/Recovery.h>
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>
#include "task.h"
#include "Tools/myuart.h"
#include "driverlib.h"
#include "RFHandler.h"
#include "TaskControl.h"

#define DEBUG 1

/* Used to check whether memory address is valid */
#define heapBITS_PER_BYTE ((size_t)8)
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock; /*<< The next free block in the list. */
    size_t xBlockSize;                    /*<< The size of the free block. */
} BlockLink_t;

static const size_t xHeapStructSize = (sizeof(BlockLink_t) + ((size_t)(portBYTE_ALIGNMENT - 1))) & ~((size_t)portBYTE_ALIGNMENT_MASK);
const size_t xBlockAllocatedBit = ((size_t)1) << ((sizeof(size_t) * heapBITS_PER_BYTE) - 1);

/* Used for rerunning unfinished tasks */
#pragma NOINIT(taskRecord)
TaskRecord_t taskRecord[MAX_GLOBAL_TASKS];

/* Logs for all the components */
DataRequestLog_t dataRequestLogs[MAX_GLOBAL_TASKS];

extern tskTCB *volatile pxCurrentTCB;
extern unsigned int volatile stopTrack;

/*
 * taskRerun(): rerun the current task invoking this function
 * parameters: none
 * return: none
 * note: Memory allocated by the task code is not automatically freed, and should be freed before the task is deleted
 * */
void taskRerun()
{
    xTaskCreate(pxCurrentTCB->AddressOfNVMFunction, pxCurrentTCB->pcTaskName, configMINIMAL_STACK_SIZE, NULL, pxCurrentTCB->uxPriority, NULL);
    vTaskDelete(NULL); //delete the current TCB
}

void initRecoveryEssential()
{
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        taskRecord[i].taskStatus = invalid;
    }
}

/*
 * regTaskEnd(): mark the assigned task as started
 * parameters: none
 * return: none
 * */
TaskRecord_t* regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, unsigned int stopTrack)
{
    TCB_t *TCB = (TCB_t *)pxNewTCB;

    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        //find a invalid
        if (taskRecord[i].taskStatus == invalid)
        {
            TaskRecord_t *record = taskRecord + i;
            strcpy(record->taskName, TCB->pcTaskName);
            record->address = taskAddress;
            record->priority = TCB->uxPriority;
            record->TCBNum = TCB->uxTCBNumber;
            record->TCB = TCB;
            record->schedulerTask = stopTrack;
            record->stackSize = stackSize;
            record->taskStatus = running;
            return record;
        }
    }
    return NULL;
}

/*
 * regTaskEnd(): mark the current as ended
 * parameters: none
 * return: none
 * */
void regTaskEnd()
{
    int i;
    for (i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        if (taskRecord[i].taskStatus == finished && taskRecord[i].TCBNum == pxCurrentTCB->uxTCBNumber)
        {
            taskRecord[i].taskStatus= invalid;
            return;
        }
    }
}

/*
 * prvcheckAdd(): check if the pointer is actually allocated and can be freed
 * parameters: none
 * return: 1 for yes
 * */
int prvcheckAdd(void *pv)
{
    tskTCB *pxTCB = pv;
    uint8_t *puc = (uint8_t *)pxTCB->pxStack;
    BlockLink_t *pxLink;

    /* This casting is to keep the compiler from issuing warnings. */
    pxLink = (void *)(puc - xHeapStructSize);

    /* Check the block is actually allocated. */
    volatile uint64_t allocbit = pxLink->xBlockSize & xBlockAllocatedBit;
    if (allocbit == 0)
        return 0;
    if (pxLink->pxNextFreeBlock != NULL)
        return 0;

    return 1;
}

/*
 * freePreviousTasks(): free all unfinished tasks stacks after power failure, this only is used by default approach
 * parameters: none
 * return: none
 * */
void freePreviousTasks()
{
    int i;
    for (i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        //find all running tasks
        if (taskRecord[i].taskStatus == running)
        {
            //see if the address is valid
            if (prvcheckAdd(taskRecord[i].TCB) == 1)
            {
                dprint2uart("Delete: %d\r\n", taskRecord[i].TCBNum);
                //Since all tasks information, e.g., list of ready queue, is saved in VM, we only needs to consider the stack and free the stack and TCB
                TCB_t *tcb = taskRecord[i].TCB;
                vPortFree(tcb->pxStack);
                vPortFree(tcb);
            }
        }
    }
}

/*
 * failureRecovery(): recover all running tasks after power failure
 * parameters: none
 * return: none
 * */
void failureRecovery()
{
    TaskRecord_t *task = NULL;
    for (int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        task = taskRecord + i;
        //find all running tasks
        if (task->taskStatus == running)
        {
            //see if the address is valid
            if (prvcheckAdd(task->TCB) == 1)
            {
                dprint2uart("Recovery: Delete: %d\r\n", task->TCBNum);
                //Since all tasks information, e.g., list of ready queue, is saved in VM, we only needs to consider the stack and free the stack and TCB
                tskTCB *tcb = task->TCB;
                vPortFree(tcb->pxStack);
                vPortFree(tcb);
            }
            task->taskStatus = invalid;
            if (!(task->schedulerTask)) // consider recreating task
            {
                if (task->validationRecord != NULL)
                {
                    if (task->validationRecord->validRecord == true)
                    {
                        // the task content is gone, but its modified version is under validation.
                        task->taskStatus = validating;
                        // recreate after previous task finished validation
                        continue;
                    }
                }
                xTaskCreate(task->address, task->taskName, task->stackSize, NULL, task->priority, NULL);
                dprint2uart("Recovery Create: %d\r\n", task->TCBNum);
            }

            dprint2uart("Rend: xPortGetFreeHeapSize = %d\r\n", xPortGetFreeHeapSize());
        }
    }

}

/* DataManager Logging */
DataRequestLog_t* createDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId,
                          const Data_t *xToDataObj, const TaskHandle_t *xFromTask)
{
    DataRequestLog_t *log = NULL;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        log = dataRequestLogs + i;
        if (log->valid == false)
        {
            log->xToDataObj = (Data_t *)xToDataObj;
            log->xFromTask = (TaskHandle_t *)xFromTask;
            log->dataId = dataId;
            log->taskId = taskId;
            log->valid = true;
            break;
        }
        if(DEBUG)
            print2uart("CreateDataRequestLog Failed\n");
    }
    return log;
}

DataRequestLog_t *getDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId)
{
    DataRequestLog_t *log = NULL;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        log = dataRequestLogs + i;
        if (dataIdEqual(&(log->dataId), &dataId) && taskIdEqual(&(log->taskId), &taskId))
        {
            break;
        }
    }
    return log;
}

void deleteDataRequestLog(TaskUUID_t taskId, DataUUID_t dataId)
{
    DataRequestLog_t *log;
    for (unsigned int i = 0; i < MAX_GLOBAL_TASKS; i++)
    {
        log = dataRequestLogs + i;
        if (dataIdEqual(&(log->dataId), &dataId) && taskIdEqual(&(log->taskId), &taskId))
        {
            log->valid = false;
            break;
        }
    }

    return;
}
