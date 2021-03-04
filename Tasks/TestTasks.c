#include "TestTasks.h"

#include <ValidationHandler/Validation.h>
#include <msp430.h>

#include "FreeRTOS.h"
#include "SimpDB.h"
#include "dht11.h"
#include "myuart.h"
#include "OurTCB.h"
#include "task.h"

#define DEBUG 0
#define INFO 1

extern uint8_t nodeAddr;
extern uint32_t timeCounter;
extern uint32_t statistics[4];
extern int firstTime;
#pragma NOINIT(taskIdRecord)
uint16_t taskIdRecord[4];

/* DHT11 */
extern unsigned char SENSE_TIMER;

void sensingTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[0]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_SENSING);

    int32_t RH;
    Data_t humidityData;
    unsigned char Packet[5];
    unsigned char RH_byte1, RH_byte2;
    while (1) {
        taskIdRecord[0] += 2;
        taskId.id = taskIdRecord[0];

        humidityData = readLocalDB(1, &RH, sizeof(RH));
        taskENTER_CRITICAL();   // The sensing can not be interrupted
        read_Packet(Packet);
        RH_byte1 = Packet[0];
        RH_byte2 = Packet[1];
        taskEXIT_CRITICAL();
        if (DEBUG) {
            if (check_Checksum(Packet)) {
                print2uart("check sum success\n");
            } else {
                print2uart("check sum failed\n");
            }
        }
        RH = RH_byte1;
        writeData(&humidityData, &RH);
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, humidityData);

        // wait for humidity sensor to sense again, must > 1500ms, otherwise checksum will fail
        vTaskDelay(2000);
    }
}

// reader task
void monitorTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[1]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_MONITOR);

    int32_t totalSpread;
    Data_t totalSpreadData;

    while (1) {
        taskId.id = taskIdRecord[1];
        taskIdRecord[1] += 2;

        totalSpreadData = readLocalDB(2, &totalSpread, sizeof(totalSpread));
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, totalSpreadData);
        print2uart_new("Total Spread %d\n", totalSpread);
        // same as sensing rate to make sense
        vTaskDelay(2000);
    }
}

void sprayerTask() {
    // init
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[2]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_SPRAYER);
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1)
            ;
    }

    int32_t spreadAmount;
    Data_t sprayAmountData;
    Data_t humidityData;
    int32_t RH;
    int32_t sprayAmount;
    while (1) {
        taskId.id = taskIdRecord[2];
        taskIdRecord[2] += 2;

        humidityData = readRemoteDB(taskId, &myTaskHandle, 1, 1, (void *)&RH, sizeof(RH));
        if (INFO) {
            print2uart_new("RH: %d\n", RH);
        }

        // some calc
        sprayAmountData = readLocalDB(1, &sprayAmount, sizeof(sprayAmount));
        sprayAmount += 1;
        writeData(&sprayAmountData, &sprayAmount);
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 2, &sprayAmountData, &humidityData);

        print2uart_new("Spraying...\n");
        // wait for spraying
        vTaskDelay(1000);
    }
}

void reportTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[3]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_REPORT);
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1) ;
    }

    Data_t sprayAmountLocalData;
    Data_t sprayAmountRemoteData;
    uint32_t sprayAmountLocal;
    uint32_t sprayAmountRemote;

    while (1) {
        taskId.id = taskIdRecord[3];
        taskIdRecord[3] += 2;

        sprayAmountLocalData =
            readLocalDB(1, &sprayAmountLocal, sizeof(sprayAmountLocal));
        sprayAmountRemoteData =
            readRemoteDB(taskId, &myTaskHandle, 1, 2,
                         (void *)&sprayAmountRemote, sizeof(sprayAmountRemote));
        writeData(&sprayAmountRemoteData, &sprayAmountLocal);
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 2,
                   &sprayAmountRemoteData, &sprayAmountLocalData);
        vTaskDelay(1000);
    }
}

void setupTasks()
{
    if (nodeAddr == 1)
    {
        int32_t RH = 100;
        TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
        Data_t humidityData = createWorkingSpace(&RH, sizeof(RH));
        humidityData.dataId.owner = nodeAddr;
        humidityData.dataId.id = 1;
        commitLocalDB(taskId, &humidityData);

        int totalSpread = 0;
        TaskUUID_t taskId2 = {.nodeAddr = nodeAddr, .id = 2};
        Data_t totalSpreadData = createWorkingSpace(&totalSpread, sizeof(totalSpread));
        totalSpreadData.dataId.owner = nodeAddr;
        totalSpreadData.dataId.id = 2;
        commitLocalDB(taskId2, &totalSpreadData);
    }
    else if (nodeAddr == 2 || nodeAddr == 3 || nodeAddr == 4)
    {
        int spreadAmount = 0;
        TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
        Data_t sprayAmountData =
            createWorkingSpace(&spreadAmount, sizeof(spreadAmount));
        sprayAmountData.dataId.owner = nodeAddr;
        sprayAmountData.dataId.id = 1;
        commitLocalDB(taskId, &sprayAmountData);
    }

    taskIdRecord[0] = 1;
    taskIdRecord[1] = 2;
    taskIdRecord[2] = 1;
    taskIdRecord[3] = 2;
}
