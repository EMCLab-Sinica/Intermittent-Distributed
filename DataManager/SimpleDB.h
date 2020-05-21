#ifndef SIMPLEDB_H
#define SIMPLEDB_H

#include <stdint.h>

/* Defines */
#define MAX_DB_OBJECTS 10
#define MAX_OBJECT_SIZE 32

/* Enums */
typedef enum DataVersion
{
    working,
    consistent
} data_version_e;

typedef enum DataLocation
{
    VM,
    NVM
} data_location_e;

/* Structures */
typedef struct Data
{
    data_version_e version;
    data_location_e location;
    void *ptr;
    uint16_t size;
    int32_t validationTS;
} data_t;

/* Variable Declaration */

/* for validation */
extern unsigned long timeCounter;

/* DB functions */
void DBconstructor();
void DBdestructor();
data_t *getDataRecord(uint8_t dataId);
uint8_t readLocalDB(void *to, uint8_t id);
void readRemoteDB(void *to, uint8_t remoteAddr, uint8_t dataId);
void responseRemoteRead(uint8_t destAddr, uint8_t dataId);

#endif // SIMPLEDB_H
