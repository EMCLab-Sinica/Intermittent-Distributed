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


/* map functions */
void init();
void* access(uint8_t objectIndex);
void accessCache(uint8_t objectIndex);
void* accessData(uint8_t objectIndex);
void commit(uint32_t objectIndex, void *dataAddress, uint32_t vBegin, uint32_t vEnd);
void dumpAll();
uint32_t getBegin(uint8_t objectIndex);
uint32_t getEnd(uint8_t objectIndex);

