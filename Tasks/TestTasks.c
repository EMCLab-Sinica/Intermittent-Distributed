#include <msp430.h>
#include "TestTasks.h"
#include "FreeRTOS.h"
#include "SimpDB.h"
#include "Validation.h"
#include "dht11.h"
#include "myuart.h"
#include "task.h"

#define DEBUG 0

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
        read_Packet(Packet);
        RH_byte1 = Packet[0];
        RH_byte2 = Packet[1];
        T_byte1 = Packet[2];
        T_byte2 = Packet[3];
        checksum = Packet[4];

        if (DEBUG) {
            if (check_Checksum(Packet)) {
                print2uart("check sum success\n");
            } else {
                print2uart("check sum failed\n");
            }
            print2uart_new("DHT11: %d, %d\n", T_byte1, RH_byte1);
        }

       commitLocalDB(&temperature , sizeof(T_byte1));
       commitLocalDB(&humidity, sizeof(RH_byte1));

        vTaskDelay(2000);
    }
}

void fanTask() {
}

void sprayerTask() {
}

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

void remoteAccessTask() {
    TaskUUID_t taskId = {.nodeAddr = nodeAddr, .id = 1};
    const TaskHandle_t myTaskHandle = xTaskGetHandle("RemoteAccess");
    if (myTaskHandle == NULL) {
        print2uart("Error, can not retrive task handle\n");
        while (1)
            ;
    }

    Data_t remoteDataObject;
    unsigned long long reqTime = 0;
    unsigned long long timeElapsed = 0;
    volatile TickType_t tick0 = 0;
    volatile TickType_t tick1 = 0;
    while (1) {
        uint32_t test = 0;
        // tick0 = xTaskGetTickCount();
        reqTime = timeCounter;
        remoteDataObject = readRemoteDB(taskId, &myTaskHandle, 1, 1,
                                        (void *)&test, sizeof(test));
        timeElapsed = timeCounter - reqTime;
        statistics[1] += (uint32_t)timeElapsed;
        if (DEBUG) {
            print2uart("%l\n", timeElapsed);
            print2uart("got dataId %d: %d\n", remoteDataObject.dataId.id, test);
        }

        test++;
        // print2uart("GotData: %d\n", test);
        vTaskDelay(100);

        taskCommit(taskId.id, &myTaskHandle, 1, &remoteDataObject);
        statistics[0]++;
    }
}

void syncTimeHelperTask() {
    while (1) {
        print2uart("%l\n", timeCounter);
        vTaskDelay(1000);
    }
}
