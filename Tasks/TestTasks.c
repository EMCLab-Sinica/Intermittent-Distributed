#include "TestTasks.h"

#include <msp430.h>

#include "FreeRTOS.h"
#include "SimpDB.h"
#include "Validation.h"
#include "dht11.h"
#include "myuart.h"
#include "task.h"

#define DEBUG 1

extern uint8_t nodeAddr;
extern uint64_t timeCounter;
extern uint32_t statistics[2];

/* DHT11 */
extern unsigned char SENSE_TIMER;

void sensingTask() {
    unsigned char RH_byte1, RH_byte2, T_byte1, T_byte2, checksum;
    unsigned char Packet[5];
    Data_t temperature = createWorkingSpace(&T_byte1, sizeof(T_byte1));
    Data_t humidity = createWorkingSpace(&RH_byte1, sizeof(RH_byte1));

    while (1) {
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

        temperature.dataId = commitLocalDB(&temperature, sizeof(T_byte1));
        humidity.dataId = commitLocalDB(&humidity, sizeof(RH_byte1));

        vTaskDelay(2000);
    }
}

void fanTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
    const TaskHandle_t myTaskHandle = xTaskGetHandle(TASKNAME_FAN);
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1) ;
    }

    Data_t tempObject;
    Data_t humidityObject;
    while (1) {
        uint32_t temp = 0;
        uint32_t humidity = 0;
        tempObject = readRemoteDB(taskId, &myTaskHandle, 1, 1,
                                        (void *)&temp, sizeof(temp));
        humidityObject = readRemoteDB(taskId, &myTaskHandle, 1, 2,
                                        (void *)&humidity, sizeof(humidity));
        //statistics[1] += (uint32_t)timeElapsed;
        if (DEBUG) {
        }
        print2uart_new("T: %d ", temp);
        print2uart_new("RH: %d\n", humidity);

        //taskCommit(taskId.id, &myTaskHandle, 1, &remoteDataObject);
        //statistics[0]++;
        vTaskDelay(2000);
    }
}

void sprayerTask() {}

void localAccessTask() {
    Data_t localDataObject;
    uint32_t test;
    localDataObject = readLocalDB(1, &test, sizeof(test));

    while (1) {
        for (unsigned int i = 0; i < 10; i++) {
            unsigned int j = i * i;
        }
        vTaskDelay(500);
    }
}

void syncTimeHelperTask() {
    while (1) {
        print2uart("%l\n", timeCounter);
        vTaskDelay(1000);
    }
}
