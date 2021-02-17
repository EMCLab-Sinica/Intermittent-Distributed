#include "TestTasks.h"

#include <RecoveryHandler/Validation.h>
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
#pragma NOINIT(taskIdRecord)
uint16_t taskIdRecord[4];

/* DHT11 */
extern unsigned char SENSE_TIMER;

void sensingTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[0]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_SENSING);

    int32_t RH;
    Data_t humidityData = createWorkingSpace(&RH, sizeof(RH));
    humidityData.dataId.id = 1;
    commitLocalDB(taskId, &humidityData);

    while (1) {
        taskId.id = taskIdRecord[0];
        taskIdRecord[0] += 2;

        humidityData = readLocalDB(1, &RH, sizeof(RH));
        RH = 70;
        writeData(&humidityData, &RH);
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, humidityData);
        vTaskDelay(2000);
    }
}

// reader task
void monitorTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = taskIdRecord[1]};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_MONITOR);

    int32_t totalSpread;
    Data_t totalSpreadData = createWorkingSpace(&totalSpread, sizeof(totalSpread));
    totalSpread = 0;
    totalSpreadData.dataId.id = 2;
    commitLocalDB(taskId, &totalSpreadData);

    while (1) {
        taskId.id = taskIdRecord[1];
        taskIdRecord[1] += 2;

        totalSpreadData = readLocalDB(2, &totalSpread, sizeof(totalSpread));
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, totalSpreadData);
        print2uart_new("Total Spread %d\n", totalSpread);
        vTaskDelay(1000);
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
    Data_t sprayAmountData = createWorkingSpace(&spreadAmount, sizeof(spreadAmount));
    spreadAmount = 0;
    sprayAmountData.dataId.id = 1;
    commitLocalDB(taskId, &sprayAmountData);

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
    taskIdRecord[0] = 1;
    taskIdRecord[1] = 2;
    taskIdRecord[2] = 1;
    taskIdRecord[3] = 2;
}
