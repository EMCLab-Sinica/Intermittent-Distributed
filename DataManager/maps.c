/*
 * maps.c
 *
 * Description: Functions to maintain the address maps
 */
#include <DataManager/maps.h>
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#pragma DATA_SECTION(mapSwitcher, ".map") //each bit indicates address map for a object
static uint32_t mapSwitcher[NUMCOMMIT];//16bit * 15 = 240 maximum objects
/* Protected data for atomicity */
#pragma NOINIT(map0)
static void* map0[MAX_DB_OBJ];
#pragma NOINIT(taskCommitLog0)
static TaskCommitLog_t taskCommitLog0[MAX_DB_OBJ];

#pragma NOINIT(map1)
static void* map1[MAX_DB_OBJ];
#pragma NOINIT(taskCommitLog1)
static TaskCommitLog_t taskCommitLog1[MAX_DB_OBJ];

/*
 * description: reset all the mapSwitcher and maps
 * parameters: none
 * return: none
 * */
 void init(){
    memset(mapSwitcher, 0, sizeof(uint32_t) * NUMCOMMIT);

    memset(map0, 0, sizeof(void*) * MAX_DB_OBJ);
    memset(map1, 0, sizeof(void*) * MAX_DB_OBJ);
    memset(taskCommitLog0, 0, sizeof(TaskCommitLog_t) * MAX_DB_OBJ);
    memset(taskCommitLog1, 0, sizeof(TaskCommitLog_t) * MAX_DB_OBJ);
}


volatile int dummy;// the compiler mess up something which will skip compiling the CHECK_BIT procedure, we need this to make the if/else statement work!
/*
 * description: return the address for the commit data, reduce the valid interval
 * parameters: number of the object
 * return: none
 * */
void* access(uint8_t objectIndex){
    uint32_t prefix = objectIndex / 16, postfix = objectIndex % 16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return map1[objectIndex];
    }
    else{
        dummy = 0;
        return map0[objectIndex];
    }
}

/*
 * description: return the data address of the data object
 * parameters: number of the object
 * return: none
 * */
void* accessData(uint8_t objectIndex){
    uint32_t prefix = objectIndex / 16, postfix = objectIndex % 16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return map1[objectIndex];
    }
    else{
        dummy = 0;
        return map0[objectIndex];
    }
}

/*
 * description: commit the address for certain commit data
 * parameters: number of the object, source address
 * return: none
 * */
void commit(uint32_t objectIndex, void *dataAddress, uint32_t vBegin, uint32_t vEnd)
{
    int prefix = objectIndex / 16, postfix = objectIndex % 16;
    if (CHECK_BIT(mapSwitcher[prefix], postfix) > 0)
    {
        map0[objectIndex] = dataAddress;
        TaskCommitLog_t * log = taskCommitLog0 + objectIndex;
        log->TaskBegins[log->pos++] = vBegin;
    }
    else
    {
        map1[objectIndex] = dataAddress;
        TaskCommitLog_t * log = taskCommitLog1 + objectIndex;
        log->TaskBegins[log->pos++] = vBegin;
    }

    //atomic commit
    mapSwitcher[prefix] ^= 1 << (postfix);
    //TODO: we need to use some trick to the stack pointer to use pushm for multiple section
}

/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the begin interval
 * */
uint32_t getBegin(uint8_t objectIndex){
    int prefix = objectIndex/16, postfix = objectIndex%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        TaskCommitLog_t * log = taskCommitLog1 + objectIndex;
        return log->TaskBegins[log->pos];
    }
    else{
        dummy = 0;
        TaskCommitLog_t * log = taskCommitLog0 + objectIndex;
        return log->TaskBegins[log->pos];
    }
}


/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the End interval
 * */
uint32_t  getEnd(uint8_t objectIndex){
    int prefix = objectIndex/16,postfix = objectIndex%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        TaskCommitLog_t * log = taskCommitLog1 + objectIndex;
        return log->TaskEnds[log->pos];
    }
    else{
        dummy = 0;
        TaskCommitLog_t * log = taskCommitLog0 + objectIndex;
        return log->TaskEnds[log->pos];
    }
}


/*
 * description: use for debug, dump all info.
 * parameters: none
 * return: none
 * */
void dumpAll(){
    int i;
    printf("address maps\n");
    for(i = 0; i < MAX_DB_OBJ; i++){
        printf("%d: %p\n", i, accessData(i));
    }
    printf("mapSwitcher\n");
    for(i = 0; i < MAX_DB_OBJ; i++){
        int prefix = i/8, postfix = i%8;
        if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0)
            printf("1");
        else
            printf("0");
    }
    printf("\n");
}
