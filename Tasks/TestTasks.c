#include "TestTasks.h"

#include <RecoveryHandler/Validation.h>
#include <msp430.h>

#include "FreeRTOS.h"
#include "SimpDB.h"
#include "dht11.h"
#include "myuart.h"
#include "task.h"

#define DEBUG 0
#define INFO 1

extern uint8_t nodeAddr;
extern uint64_t timeCounter;
extern uint32_t statistics[4];

/* DHT11 */
extern unsigned char SENSE_TIMER;

void sensingTask() {
    unsigned char RH_byte1, RH_byte2, T_byte1, T_byte2, checksum;
    unsigned char Packet[5];
    Data_t temperature = createWorkingSpace(&T_byte1, sizeof(T_byte1));
    Data_t humidity = createWorkingSpace(&RH_byte1, sizeof(RH_byte1));
    temperature.dataId.id = 1;
    humidity.dataId.id = 2;

    while (1) {
        T_byte1 = 27;
        RH_byte2 = 70;
        /*
        taskENTER_CRITICAL();
        read_Packet(Packet);
        RH_byte1 = Packet[0];
        RH_byte2 = Packet[1];
        T_byte1 = Packet[2];
        T_byte2 = Packet[3];
        checksum = Packet[4];
        taskEXIT_CRITICAL();

        if (DEBUG) {
            if (check_Checksum(Packet)) {
                print2uart("check sum success\n");
            } else {
                print2uart("check sum failed\n");
            }
            print2uart_new("DHT11: %d, %d\n", T_byte1, RH_byte1);
        }
        */

        temperature.dataId = commitLocalDB(&temperature, sizeof(T_byte1));
        humidity.dataId = commitLocalDB(&humidity, sizeof(RH_byte1));

        // print2uart("%d\n", ++statistics[0]);
        vTaskDelay(2000);
    }
}

void fanTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_FAN);
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1)
            ;
    }

    Data_t tempData;
    Data_t humidityData;
    Data_t fanSpeedData;
    uint32_t temp = 0;
    uint32_t humidity = 0;
    uint32_t fanSpeed = 0;
    while (1) {

        tempData = readRemoteDB(taskId, &myTaskHandle, 1, 1, (void *)&temp, sizeof(temp));
        humidityData = readRemoteDB(taskId, &myTaskHandle, 1, 2, (void *)&humidity, sizeof(humidity));
        fanSpeedData = readRemoteDB(taskId, &myTaskHandle, 4, 1, (void *)&fanSpeed, sizeof(fanSpeed));
        // statistics[1] += (uint32_t)timeElapsed;
        if (INFO) {
            print2uart_new("T: %d ", temp);
            print2uart_new("RH: %d\n", humidity);
        }

        fanSpeed = 10;
        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, &fanSpeedData);
        print2uart_new("%d\n", ++statistics[1]);
        vTaskDelay(1000);
    }
}

void sprayerTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_SPRAYER);
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1)
            ;
    }

    uint32_t humidity = 0;
    int32_t sprayAmount = 0;
    Data_t humidityData;
    Data_t sprayAmountData;
    while (1) {
        humidityData = readRemoteDB(taskId, &myTaskHandle, 1, 2,
                                    (void *)&humidity, sizeof(humidity));
        sprayAmountData = readRemoteDB(taskId, &myTaskHandle, 4, 2, (void *)&sprayAmount,
                         sizeof(sprayAmount));
        // statistics[1] += (uint32_t)timeElapsed;
        if (INFO) {
            print2uart_new("RH: %d\n", humidity);
        }
        sprayAmount = humidity + 10;

        taskCommit(taskId.id, (TaskHandle_t *)&myTaskHandle, 1, &sprayAmountData);
        // statistics[0]++;
        print2uart_new("%d\n", ++statistics[2]);
         vTaskDelay(1000);
    }
}

void monitorTask() {
    int32_t fanSpeed = 0;
    int32_t sprayAmount = 0;
    Data_t fanSpeedData = createWorkingSpace(&fanSpeed, sizeof(fanSpeed));
    Data_t sprayAmountData =
        createWorkingSpace(&sprayAmount, sizeof(sprayAmount));
    fanSpeedData.dataId.id = 1;
    sprayAmountData.dataId.id = 2;
    commitLocalDB(&fanSpeedData, sizeof(fanSpeed));
    commitLocalDB(&sprayAmountData, sizeof(sprayAmount));

    while (1) {
        readLocalDB(1, (void *)&fanSpeed, sizeof(fanSpeed));
        readLocalDB(2, (void *)&sprayAmount, sizeof(sprayAmount));
        print2uart_new("%d\n", ++statistics[3]);
        vTaskDelay(1000);
    }
}

void syncTimeHelperTask() {
    while (1) {
        print2uart("%l\n", timeCounter);
        vTaskDelay(1000);
    }
}
