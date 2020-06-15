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
#include "mylist.h"

/*
 * Task control block.  A task control block (TCB) is allocated for each task,
 * and stores task state information, including a pointer to the task's context
 * (the task's run time environment, including register values)
 */
typedef struct tskTaskControlBlock 			/* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
	volatile StackType_t	*pxTopOfStack;	/*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */

	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xMPUSettings;		/*< The MPU settings are defined as part of the port layer.  THIS MUST BE THE SECOND MEMBER OF THE TCB STRUCT. */
	#endif

	ListItem_t			xStateListItem;	/*< The list that the state list item of a task is reference from denotes the state of that task (Ready, Blocked, Suspended ). */
	ListItem_t			xEventListItem;		/*< Used to reference a task from an event list. */
	UBaseType_t			uxPriority;			/*< The priority of the task.  0 is the lowest priority. */
	StackType_t			*pxStack;			/*< Points to the start of the stack. */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< Descriptive name given to the task when created.  Facilitates debugging only. */ /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

	/*------------------------------  Extend to support validation: Start ------------------------------*/
	#if ( configINTERMITTENT_DISTRIBUTED == 1)
		uint64_t  vBegin;
		uint64_t  vEnd;
		/*------------------------------  Extend to support validation: End ------------------------------*/
		/*------------------------------  Extend to support dynamic stack: Start -------------------------*/
		void * AddressOfVMStack;
		void * AddressOffset;
		int StackInNVM;
		int initial;
		/*------------------------------  Extend to support dynamic stack: End ------------------------------*/
		/*------------------------------  Extend to support dynamic function: Start -------------------------*/
		void * AddressOfNVMFunction;
		void * AddressOfVMFunction;
		void * CodeOffset;
		int SizeOfFunction;
		int CodeInNVM;
	#endif
	/*------------------------------  Extend to support dynamic function: End ------------------------------*/

	#if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
		StackType_t		*pxEndOfStack;		/*< Points to the highest valid address for the stack. */
	#endif

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxCriticalNesting;	/*< Holds the critical section nesting depth for ports that do not maintain their own count in the port layer. */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxTCBNumber;		/*< Stores a number that increments each time a TCB is created.  It allows debuggers to determine when a task has been deleted and then recreated. */
		UBaseType_t		uxTaskNumber;		/*< Stores a number specifically for use by third party trace code. */
	#endif

	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxBasePriority;		/*< The priority last assigned to the task - used by the priority inheritance mechanism. */
		UBaseType_t		uxMutexesHeld;
	#endif

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
		TaskHookFunction_t pxTaskTag;
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
		void			*pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
	#endif

	#if( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulRunTimeCounter;	/*< Stores the amount of time the task has spent in the Running state. */
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		/* Allocate a Newlib reent structure that is specific to this task.
		Note Newlib support has been included by popular demand, but is not
		used by the FreeRTOS maintainers themselves.  FreeRTOS is not
		responsible for resulting newlib operation.  User must be familiar with
		newlib and must provide system-wide implementations of the necessary
		stubs. Be warned that (at the time of writing) the current newlib design
		implements a system-wide malloc() that must be provided with locks.

		See the third party link http://www.nadler.com/embedded/newlibAndFreeRTOS.html
		for additional information. */
		struct	_reent xNewLib_reent;
	#endif

	#if( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t ulNotifiedValue;
		volatile uint8_t ucNotifyState;
	#endif

	/* See the comments in FreeRTOS.h with the definition of
	tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
	#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 Macro has been consolidated for readability reasons. */
		uint8_t	ucStaticallyAllocated; 		/*< Set to pdTRUE if the task is a statically allocated to ensure no attempt is made to free the memory. */
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDelayAborted;
	#endif

	#if( configUSE_POSIX_ERRNO == 1 )
		int iTaskErrno;
	#endif

} tskTCB;

/* The old tskTCB name is maintained above then typedefed to the new TCB_t name
below to enable the use of older kernel aware debuggers. */
typedef tskTCB TCB_t;

/* Used to check whether memory address is valid */
#define heapBITS_PER_BYTE       ( ( size_t ) 8 )
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock;   /*<< The next free block in the list. */
    size_t xBlockSize;                      /*<< The size of the free block. */
} BlockLink_t;

static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
const size_t xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );

/* Used for rerunning unfinished tasks */
#pragma NOINIT(taskRecord)
TaskRecord_t taskRecord[MAX_GLOBAL_TASKS];

/* Logs for all the components */
#pragma NOINIT(dataTransferLogList)
MyList_t *dataTransferLogList;

extern tskTCB * volatile pxCurrentTCB;
extern unsigned char volatile stopTrack;

/*
 * taskRerun(): rerun the current task invoking this function
 * parameters: none
 * return: none
 * note: Memory allocated by the task code is not automatically freed, and should be freed before the task is deleted
 * */
void taskRerun(){
    xTaskCreate( pxCurrentTCB->AddressOfNVMFunction, pxCurrentTCB->pcTaskName, configMINIMAL_STACK_SIZE, NULL, pxCurrentTCB->uxPriority, NULL);
    vTaskDelete(NULL);//delete the current TCB
}

/*
 * regTaskEnd(): mark the assigned task as started
 * parameters: none
 * return: none
 * */
void regTaskStart(void *pxNewTCB, void *taskAddress, uint32_t stackSize, uint8_t stopTrack)
{
    TCB_t *TCB = (TCB_t *)pxNewTCB;

    int i;
    for(i = 0; i < MAX_GLOBAL_TASKS; i++){
        //find a invalid
        if(taskRecord[i].unfinished != 1){
            TaskRecord_t *record = taskRecord+i;
            strcpy(record->taskName, TCB->pcTaskName);
            record->address = taskAddress;
            record->priority = TCB->uxPriority;
            record->TCBNum = TCB->uxTCBNumber;
            record->TCB = TCB;
            record->schedulerTask = stopTrack;
            record->stackSize =  stackSize;
            record->unfinished = 1;
            break;
        }
    }
}

/*
 * regTaskEnd(): mark the current as ended
 * parameters: none
 * return: none
 * */
void regTaskEnd(){
    int i;
    for(i = 0; i < MAX_GLOBAL_TASKS; i++){
        //find the slot
        if(taskRecord[i].unfinished == 1 && taskRecord[i].TCBNum == pxCurrentTCB->uxTCBNumber){
            taskRecord[i].unfinished = 0;
            return;
        }
    }
}

/*
 * prvcheckAdd(): check if the pointer is actually allocated and can be freed
 * parameters: none
 * return: 1 for yes
 * */
int prvcheckAdd(void * pv){
    tskTCB * pxTCB = pv;
    uint8_t *puc = ( uint8_t * ) pxTCB->pxStack;
    BlockLink_t *pxLink;

    /* This casting is to keep the compiler from issuing warnings. */
    pxLink = ( void * ) (puc - xHeapStructSize);

    /* Check the block is actually allocated. */
    volatile uint64_t  allocbit = pxLink->xBlockSize & xBlockAllocatedBit;
    if(allocbit == 0)
        return 0;
    if(pxLink->pxNextFreeBlock != NULL)
        return 0;

    return 1;
}

/*
 * freePreviousTasks(): free all unfinished tasks stacks after power failure, this only is used by default approach
 * parameters: none
 * return: none
 * */
void freePreviousTasks(){
    int i;
    for(i = 0; i < MAX_GLOBAL_TASKS; i++){
        //find all unfinished tasks
        if(taskRecord[i].unfinished == 1){
            //see if the address is balid
            if(prvcheckAdd(taskRecord[i].TCB) == 1){
                dprint2uart("Delete: %d\r\n", taskRecord[i].TCBNum);
                //Since all tasks information, e.g., list of ready queue, is saved in VM, we only needs to consider the stack and free the stack and TCB
                TCB_t *tcb = taskRecord[i].TCB;
                vPortFree(tcb->pxStack);
                vPortFree(tcb);
                taskRecord[i].unfinished = 1;
            }
        }
    }
}


/*
 * failureRecovery(): recover all unfinished tasks after power failure
 * parameters: none
 * return: none
 * */
void failureRecovery(){
    TaskRecord_t *task = NULL;
    for(int i = 0; i < MAX_GLOBAL_TASKS; i++){
        task = taskRecord+i;
        //find all unfinished tasks
        if(task->unfinished == 1){
            //see if the address is valid
            if(prvcheckAdd(task->TCB) == 1){
                dprint2uart("Recovery: Delete: %d\r\n", task->TCBNum);
                //Since all tasks information, e.g., list of ready queue, is saved in VM, we only needs to consider the stack and free the stack and TCB
                tskTCB* tcb = task->TCB;
                vPortFree(tcb->pxStack);
                vPortFree(tcb);
            }
            task->unfinished = 0;
            if(!(task->schedulerTask)){
                xTaskCreate(task->address, task->taskName, task->stackSize, NULL, task->priority, NULL);
                dprint2uart("Recovery Create: %d\r\n", task->TCBNum);
            }
            dprint2uart("Rend: xPortGetFreeHeapSize = %l\r\n", xPortGetFreeHeapSize());
        }
    }
    /* Start the scheduler. */
    vTaskStartScheduler();
}

/* DataManager Logging */
void createDataTransferLog(
    TransferType_e transferType, DataUUID_t dataId,
    const Data_t *dataObj, const TaskHandle_t *xFromTask)
{
    DataTransferLog_t *newDataTransferLog = pvPortMalloc(sizeof(DataTransferLog_t));

    newDataTransferLog->dataId = dataId;
    newDataTransferLog->type = transferType;
    if ( transferType == request )
    {
        newDataTransferLog->xDataObj = (Data_t *)dataObj;
        newDataTransferLog->xFromTask = (TaskHandle_t *)xFromTask;
    }

    listInsertEnd(newDataTransferLog, dataTransferLogList);
}

DataTransferLog_t *getDataTransferLog(TransferType_e transferType, DataUUID_t dataId)
{
    DataTransferLog_t *log;
    MyListNode_t *iterator = dataTransferLogList->head;
    while (iterator != NULL)
    {
        log = iterator->data;
        if (dataIdEqual(&(log->dataId), &dataId) && log->type == transferType)
        {
            break;
        }
        iterator = iterator->next;
    }

    return log;
}

void deleteDataTransferLog(TransferType_e transferType, DataUUID_t dataId)
{
    DataTransferLog_t *log;
    MyListNode_t *current = dataTransferLogList->head;
    MyListNode_t *previous = current;
    while (current != NULL)
    {
        log = current->data;
        if (dataIdEqual(&(log->dataId), &dataId) && log->type == transferType)
        {
            previous->next = current->next;
            if (current == dataTransferLogList->head)
                dataTransferLogList->head = current->next;
            vPortFree(current->data);       // free the data
            vPortFree(current);             // free the node
            return;
        }
        previous = current;
        current = current->next;
    }
}
