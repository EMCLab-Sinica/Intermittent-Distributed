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
        while(1);
    }
    while (1)
    {
        vTaskDelay(1000);
        uint8_t dataId = 6;
        data_t *remoteDataObject = VMDBCreate(16, dataId);
        if (remoteDataObject == NULL)
        {
            print2uart("Error, database full\n");
            while (1) ;
        }
        readRemoteDB(&myTaskHandle, remoteDataObject, 0, dataId);
        print2uart("GotData\n");
    }
}
