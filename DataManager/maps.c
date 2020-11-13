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
#pragma NOINIT(objectValidIntervalBegin0)
static uint64_t objectValidIntervalBegin0[MAX_DB_OBJ];
#pragma NOINIT(objectValidIntervalEnd0)
static uint64_t objectValidIntervalEnd0[MAX_DB_OBJ];

#pragma NOINIT(map1)
static void* map1[MAX_DB_OBJ];
#pragma NOINIT(objectValidIntervalBegin1)
static uint64_t objectValidIntervalBegin1[MAX_DB_OBJ];
#pragma NOINIT(objectValidIntervalEnd1)
static uint64_t objectValidIntervalEnd1[MAX_DB_OBJ];

/*
 * description: reset all the mapSwitcher and maps
 * parameters: none
 * return: none
 * */
 void init(){
    memset(mapSwitcher, 0, sizeof(uint32_t) * NUMCOMMIT);

    memset(map0, 0, sizeof(void*) * MAX_DB_OBJ);
    memset(objectValidIntervalBegin0, 0, sizeof(uint64_t) * MAX_DB_OBJ);
    memset(objectValidIntervalEnd0, 0, sizeof(uint64_t) * MAX_DB_OBJ);

    memset(map1, 0, sizeof(void*) * MAX_DB_OBJ);
    memset(objectValidIntervalBegin1, 0, sizeof(uint64_t) * MAX_DB_OBJ);
    memset(objectValidIntervalEnd1, 0, sizeof(uint64_t) * MAX_DB_OBJ);
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
void commit(uint32_t objectIndex, void *dataAddress, uint64_t vBegin, uint64_t vEnd)
{
    int prefix = objectIndex / 16, postfix = objectIndex % 16;
    if (CHECK_BIT(mapSwitcher[prefix], postfix) > 0)
    {
        map0[objectIndex] = dataAddress;
        objectValidIntervalBegin0[objectIndex] = vBegin;
        objectValidIntervalEnd0[objectIndex] = vEnd;
    }
    else
    {
        map1[objectIndex] = dataAddress;
        objectValidIntervalBegin1[objectIndex] = vBegin;
        objectValidIntervalEnd1[objectIndex] = vEnd;
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
uint64_t getBegin(uint8_t objectIndex){
    int prefix = objectIndex/16, postfix = objectIndex%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return objectValidIntervalBegin1[objectIndex];
    }
    else{
        dummy = 0;
        return objectValidIntervalBegin0[objectIndex];
    }
}


/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the End interval
 * */
uint64_t  getEnd(uint8_t objectIndex){
    int prefix = objectIndex/16,postfix = objectIndex%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return objectValidIntervalEnd1[objectIndex];
    }
    else{
        dummy = 0;
        return objectValidIntervalEnd0[objectIndex];
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
