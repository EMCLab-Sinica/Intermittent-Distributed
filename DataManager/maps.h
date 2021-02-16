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

#define MAX_DB_OBJ 4
#define NUMCOMMIT 15
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

typedef enum TaskCommitted
{
    committed,
    aborted,
    pending
} TaskCommitted_t;

typedef struct TaskUUID // Task Universal Unique Identifier
{
    uint8_t nodeAddr;
    uint8_t id;

} TaskUUID_t;

typedef struct DataCommitRecord
{
    TaskUUID_t taskUUID;
    uint8_t mapSwitcher;
    uint32_t taskBegin;
    uint32_t taskEnd;
} DataCommitRecord_t;

/* map functions */
void init();
void* access(uint8_t objectIndex);
void accessCache(uint8_t objectIndex);
void* accessData(uint8_t objectIndex);
void commit(TaskUUID_t taskUUID, uint32_t objectIndex, void *dataAddress, uint32_t vBegin, uint32_t vEnd);
void dumpAll();
uint32_t getBegin(uint8_t objectIndex);
uint32_t getEnd(uint8_t objectIndex);
uint32_t getFirstCommitedBegin(uint8_t objectIndex, uint32_t begin);
TaskCommitted_t checkCommitted(TaskUUID_t taskUUID, uint32_t objectIndex);

