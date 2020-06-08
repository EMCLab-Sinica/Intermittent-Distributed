/*
 * maps.h
 *
 *  Description : This header file is used to define the address maps for atomic commit
 *              ** What we need to protect for atomicity
 *                  * Address of consistency version (map0 and map1)
 *                  * Validity time interval of data (validStart and objectValidIntervalEnd)
 *              ** call init() to reset data
 */
#include <stdint.h>

#define MAX_DB_OBJ 16
#define NUMCOMMIT 15
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

/* internal functions */
static unsigned long max(unsigned long a, unsigned long b){
    if (a > b)
        return a;
    else
        return b;
}

/* map functions */
void init();
void* access(int objectIndex);
void accessCache(int objectIndex);
void* accessData(int objectIndex);
void commit(uint32_t objectIndex, void *dataAddress, uint64_t vBegin, uint64_t vEnd);
void dumpAll();
unsigned long getBegin(int objectIndex);
unsigned long getEnd(int objectIndex);





