/*
 * maps.c
 *
 * Description: Functions to maintain the address maps
 */
#include <DataManager/maps.h>
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
/*
 * description: reset all the mapSwitcher and maps
 * parameters: none
 * return: none
 * */
 void init(){
    memset(mapSwitcher, 0, sizeof(unsigned int) * NUMCOMMIT);

    memset(map0, 0, sizeof(void*) * DB_MAX_OBJ);
    memset(validBegin0, 0, sizeof(unsigned long) * DB_MAX_OBJ);
    memset(validEnd0, 0, sizeof(unsigned long) * DB_MAX_OBJ);

    memset(map1, 0, sizeof(void*) * DB_MAX_OBJ);
    memset(validBegin1, 0, sizeof(unsigned long) * DB_MAX_OBJ);
    memset(validEnd1, 0, sizeof(unsigned long) * DB_MAX_OBJ);
}


/*
 * description: return the address for the commit data, reduce the valid interval
 * parameters: number of the object
 * return: none
 * */
void* access(int numObj){
    int prefix = numObj/16,postfix = numObj%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        return map1[numObj];
    }
    else{
        return map0[numObj];
    }
}

volatile int dummy;// the compiler mess up something which will skip compiling the CHECK_BIT procedure, we need this to make the if/else statement work!
/*
 * description: return the address for the commit data
 * parameters: number of the object
 * return: none
 * */
void* accessData(int numObj){
    int prefix = numObj/16,postfix = numObj%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return map1[numObj];
    }
    else{
        dummy = 0;
        return map0[numObj];
    }
}

/*
 * description: commit the address for certain commit data
 * parameters: number of the object, source address
 * return: none
 * */
void commit(int numObj, void* commitaddress, unsigned long vBegin, unsigned long vEnd){
    int prefix = numObj/16,postfix = numObj%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        map0[numObj] = commitaddress;
        validBegin0[numObj] = vBegin;
        validEnd0[numObj] = vEnd;
    }
    else{
        map1[numObj] = commitaddress;
        validBegin1[numObj] = vBegin;
        validEnd1[numObj] = vEnd;
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
unsigned long getBegin(int numObj){
    int prefix = numObj/16, postfix = numObj%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return validBegin1[numObj];
    }
    else{
        dummy = 0;
        return validBegin0[numObj];
    }
}


/*
 * description: commit the address for certain commit data
 * parameters: number of the object
 * return: value of the End interval
 * */
unsigned long getEnd(int numObj){
    int prefix = numObj/16,postfix = numObj%16;
    if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0){
        dummy = 1;
        return validEnd1[numObj];
    }
    else{
        dummy = 0;
        return validEnd0[numObj];
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
    for(i = 0; i < DB_MAX_OBJ; i++){
        printf("%d: %p\n", i, accessData(i));
    }
    printf("mapSwitcher\n");
    for(i = 0; i < DB_MAX_OBJ; i++){
        int prefix = i/8, postfix = i%8;
        if(CHECK_BIT(mapSwitcher[prefix], postfix) > 0)
            printf("1");
        else
            printf("0");
    }
    printf("\n");
}
