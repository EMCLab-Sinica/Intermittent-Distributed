#include "myuart.h"
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TestTasks.h"

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
    print2uart("RemoteAccess\n");
    const TaskHandle_t myTaskHandle = xTaskGetHandle("RemoteAccess");
    if (myTaskHandle == NULL)
    {
        print2uart("Error, can not retrive task handle\n");
        while (1) ;
    }

    Data_t remoteDataObject;
    while (1)
    {
        vTaskDelay(1000);
        uint32_t test = 0;
        remoteDataObject = readRemoteDB(&myTaskHandle, 1, 1, (void *)&test, sizeof(test));

        test++;
        // commit(TASK_ID, commitNum, remoteDataObject,...,);

        print2uart("GotData: %d\n", test);
    }
}
