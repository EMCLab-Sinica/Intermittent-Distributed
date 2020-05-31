#include "myuart.h"
#include "SimpDB.h"
#include "FreeRTOS.h"
#include "task.h"
#include "TestTasks.h"

void remoteAccessTask()
{
    print2uart("RemoteAccess\n");
    const TaskHandle_t myTaskHandle = xTaskGetHandle("RemoteAccess");
    if (myTaskHandle == NULL)
    {
        print2uart("Error, can not retrive task handle\n");
        while (1)
            ;
    }

    data_t remoteDataObject;
    while (1)
    {
        vTaskDelay(1000);
        uint32_t test;
        remoteDataObject = readRemoteDB(&myTaskHandle, 1, 1, &test, sizeof(uint32_t));

        print2uart("GotData: %d\n", test);
    }
}
