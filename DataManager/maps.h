/*
 * maps.h
 *
 *  Description : This header file is used to define the address maps for atomic commit
 *              ** What we need to protect for atomicity
 *                  * Address of consistency version (map0 and map1)
 *                  * Validity time interval of data (validStart and validEnd)
 *              ** call init() to reset data
 */
#include <stdint.h>

#define DB_MAX_OBJ 16
#define NUMCOMMIT 15
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#pragma DATA_SECTION(mapSwitcher, ".map") //each bit indicates address map for a object
static uint32_t mapSwitcher[15];//16bit * 15 = 240 maximum objects


/* Protected data for atomicity */
#pragma NOINIT(map0)
static void* map0[DB_MAX_OBJ];
#pragma NOINIT(validBegin0)
static unsigned long validBegin0[DB_MAX_OBJ];
#pragma NOINIT(validEnd0)
static unsigned long validEnd0[DB_MAX_OBJ];

#pragma NOINIT(map1)
static void* map1[DB_MAX_OBJ];
#pragma NOINIT(validBegin1)
static unsigned long validBegin1[DB_MAX_OBJ];
#pragma NOINIT(validEnd1)
static unsigned long validEnd1[DB_MAX_OBJ];

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





