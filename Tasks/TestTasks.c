#include "myuart.h"
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TestTasks.h"
#include "Validation.h"

extern uint8_t nodeAddr;
extern unsigned long long timeCounter;

void localAccessTask()
{
    Data_t localDataObject;
    uint32_t test;
    localDataObject = readLocalDB(1, &test, sizeof(test));

    while(1)
    {
        for (unsigned int i = 0; i < 10; i++)
        {
            unsigned int j = i * i;
        }
        vTaskDelay(500);
    }
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
    unsigned long long reqTime = 0;
    unsigned long long timeElapsed = 0;
    volatile TickType_t tick0 = 0;
    volatile TickType_t tick1 = 0;
    while (1)
    {
        uint32_t test = 0;
        // tick0 = xTaskGetTickCount();
        reqTime = timeCounter;
        remoteDataObject = readRemoteDB(taskId, &myTaskHandle, 1, 1, (void *)&test, sizeof(test));
        timeElapsed = timeCounter - reqTime;
        print2uart("%l\n", timeElapsed);
        print2uart("got dataId %d: %d\n", remoteDataObject.dataId.id, test);

        test++;
        // print2uart("GotData: %d\n", test);
        vTaskDelay(100);

        taskCommit(taskId.id, &myTaskHandle, 1, &remoteDataObject);
    }
}
