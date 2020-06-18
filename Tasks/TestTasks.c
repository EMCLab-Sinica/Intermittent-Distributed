#include "myuart.h"
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TestTasks.h"
#include "Validation.h"

extern uint8_t nodeAddr;

void localAccessTask()
{
    print2uart("LocalAccess\n");
    Data_t localDataObject;
    uint32_t test;
    localDataObject = readLocalDB(1, &test, sizeof(test));
    print2uart("GotData: %d\n", test);

    while(1);
}

void remoteAccessTask()
{
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
    const TaskHandle_t myTaskHandle = xTaskGetHandle("RemoteAccess");
    if (myTaskHandle == NULL)
    {
        print2uart("Error, can not retrive task handle\n");
        while (1) ;
    }

    Data_t remoteDataObject;
    while (1)
    {
        uint32_t test = 0;
        remoteDataObject = readRemoteDB(taskId, &myTaskHandle, 1, 1, (void *)&test, sizeof(test));

        test++;
        print2uart("GotData: %d\n", test);

        // taskCommit(1, 1, &remoteDataObject);
    }
}
