/*
 * maps.c
 *
 * Description: Functions to maintain the address maps
 */
#include <DataManager/maps.h>
#include "FreeRTOS.h"
#include "projdefs.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* Protected data for atomicity */
#pragma NOINIT(dataCommitRecord)
static DataCommitRecord_t dataCommitRecord[MAX_DB_OBJ][NUMCOMMIT];
#pragma DATA_SECTION(recordSwitcher, ".map") //each bit indicates address map for a object
static uint8_t recordSwitcher[MAX_DB_OBJ];
#pragma NOINIT(map0)
static void* map0[MAX_DB_OBJ];
#pragma NOINIT(map1)
static void* map1[MAX_DB_OBJ];

/*
 * description: reset all the mapSwitcher and maps
 * parameters: none
 * return: none
 * */
 void init(){
    memset(dataCommitRecord, 0, sizeof(DataCommitRecord_t) * MAX_DB_OBJ * NUMCOMMIT);
    memset(recordSwitcher, 0, sizeof(uint8_t) * MAX_DB_OBJ);
    for (int i = 0; i < MAX_DB_OBJ; i++) {
        map0[i] = NULL;
        map1[i] = NULL;
    }
}


volatile int dummy;// the compiler mess up something which will skip compiling the CHECK_BIT procedure, we need this to make the if/else statement work!
/*
 * description: return the address for the commit data, reduce the valid interval
 * parameters: number of the object
 * return: none
 * */
void* access(uint8_t objectIndex){
    uint8_t switcher = recordSwitcher[objectIndex];
    DataCommitRecord_t *record = &(dataCommitRecord[objectIndex][switcher]);

    if(record->mapSwitcher > 0){
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
    uint8_t switcher = recordSwitcher[objectIndex];
    DataCommitRecord_t *record = &(dataCommitRecord[objectIndex][switcher]);
    if(record->mapSwitcher > 0){
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
void commit(TaskUUID_t taskUUID, uint32_t objectIndex, void *dataAddress, uint32_t vBegin, uint32_t vEnd)
{
    uint8_t switcher = recordSwitcher[objectIndex];
    uint8_t switcher_new = switcher + 1;
    if (switcher >= NUMCOMMIT)
    {
        switcher_new = 0;
    }

    DataCommitRecord_t *record_old = &(dataCommitRecord[objectIndex][switcher]);
    DataCommitRecord_t *record_new = &(dataCommitRecord[objectIndex][switcher_new]);

    if (record_old->mapSwitcher > 0)
    {
        map0[objectIndex] = dataAddress;
        record_new->mapSwitcher = 0;
    }
    else
    {
        map1[objectIndex] = dataAddress;
        record_new->mapSwitcher = 1;
    }
    record_new->taskBegin = vBegin;
    record_new->taskEnd = vEnd;
    record_new->taskUUID = taskUUID;

    //atomic commit
    recordSwitcher[objectIndex] = switcher_new;
}

/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the begin interval
 * */
uint32_t getBegin(uint8_t objectIndex){
    uint8_t switcher = recordSwitcher[objectIndex];
    DataCommitRecord_t *record = &(dataCommitRecord[objectIndex][switcher]);
    return record->taskBegin;
}

/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the End interval
 * */
uint32_t  getEnd(uint8_t objectIndex){
    uint8_t switcher = recordSwitcher[objectIndex];
    DataCommitRecord_t *record = &(dataCommitRecord[objectIndex][switcher]);
    return record->taskEnd;
}

TaskCommitted_t checkCommitted(TaskUUID_t taskUUID, uint32_t objectIndex)
{
    uint8_t switcher = recordSwitcher[objectIndex] + 1; // get oldest
    if(switcher >= NUMCOMMIT)
    {
        switcher = 0;
    }
    // too old, abort and rerun
    if (dataCommitRecord[objectIndex][switcher].taskUUID.id > taskUUID.id)
    {
        return aborted;
    }

    TaskUUID_t commitedTask;
    for (int i = 0; i < NUMCOMMIT; i++) {
        commitedTask = dataCommitRecord[objectIndex][i].taskUUID;
        if (commitedTask.nodeAddr == taskUUID.nodeAddr &&
            commitedTask.id == taskUUID.id) {
            return committed;
        }
    }

    return pending;
}

uint32_t getFirstCommitedBegin(uint8_t objectIndex, uint32_t begin)
{
    DataCommitRecord_t *record = dataCommitRecord[objectIndex];
    for (int i = 0; i < NUMCOMMIT - 1; i++) {
        if ( record[i].taskBegin <= begin && begin < record[i+1].taskBegin)
        {
            return record[i+1].taskBegin;
        }
    }
    // in case of go back to 0
    if ( record[NUMCOMMIT].taskBegin <= begin && begin < record[0].taskBegin)
    {
        return record[0].taskBegin;
    }

    return 0;
}


