
#include <stdio.h>
#include "SimpleDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "RFHandler.h"
#include "maps.h"
#include "list.h"

#pragma NOINIT(DBSpace) //space for maintaining structure of data
static uint8_t DBSpace[MAX_DB_OBJECTS * sizeof(data_t)];

#pragma NOINIT(DB) //data structures for all data
static data_t *DB;

// #pragma DATA_SECTION(dataId, ".map") //id for data labeling
// static int dataId;

/* Half of RAM for caching (0x2C00~0x3800) */
// #pragma location = 0x2C00 //Space for working at SRAM
// static unsigned char SRAMData[DWORKSIZE];

// #pragma DATA_SECTION(DWORKpoint, ".map") //RR pointer for working at SRAM
// static unsigned int DWORKpoint;

/*
 * DBconstructor(): initialize all data structure in the database
 * parameters: none
 * return: none
 * */
void DBconstructor()
{
    init();
    DB = (data_t *)DBSpace;
    for (uint8_t i = 0; i < MAX_DB_OBJECTS; i++)
    {
        DB[i].ptr = NULL;
        DB[i].size = 0;
        DB[i].validationTS = -1;
        DB[i].version = working;
        DB[i].location = VM;
    }
}

/*
 * destructor(): free all allocated data
 * parameters: none
 * return: none
 * */
void destructor(){
    for (uint8_t i = 0; i < MAX_DB_OBJECTS; i++)
    {
        // free
    }
}

data_t *getDataRecord(uint8_t dataId)
{
    if (dataId > MAX_DB_OBJECTS || dataId < 0 || DB[dataId].size <= 0)
    {
        return NULL;
    }

    return DB + dataId;
}

uint8_t readLocalDB(void * to, uint8_t dataId)
{
    data_t *data = getDataRecord(dataId);
    if (!data)
    {
        return pdFALSE;
    }

    memcpy(to, data->ptr, data->size);

    return pdTRUE;
}

/*
 * readRemoteDB(uint8_t remote_addr, uint8_t id):
 * parameters: id of the data
 * return: the pointer of data, NULL for failure
 * */

void readRemoteDB(void *to, uint8_t remote_addr, uint8_t id)
{
    // send request
    // wait for notification
    // waked up by RFHandler
}

void responseRemoteRead(uint8_t dest_addr, uint8_t data_id)
{
    // read object
    // send back message
}
