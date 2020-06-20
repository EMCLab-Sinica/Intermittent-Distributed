#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "queue.h"
#include "Recovery.h"
#include "RFHandler.h"
#include "CC1101_MSP430.h"
#include "config.h"

#include "mylist.h"
#include "myuart.h"

#define  DEBUG 1


extern uint8_t nodeAddr;
extern int firstTime;

extern QueueHandle_t DBServiceRoutinePacketQueue;

uint8_t initRFQueues()
{
    if (firstTime != 1)
    {
        DBServiceRoutinePacketQueue = xQueueCreate(5, MAX_PACKET_LEN);
        if (DBServiceRoutinePacketQueue == NULL)
        {
            print2uart("Error: DB Service Routine Queue init failed\n");
        }
    }
    else
    {
        vQueueDelete(DBServiceRoutinePacketQueue);
        DBServiceRoutinePacketQueue = xQueueCreate(5, MAX_PACKET_LEN);
    }


    return TRUE;
}

void sendWakeupSignal()
{
    DeviceWakeUpPacket_t packet = {.header.packetType = DeviceWakeUp};
    RFSendPacket(0, (uint8_t *)&packet, sizeof(packet));
    // delay 5ms incase of RF traffic jamming
    __delay_cycles(5000 * CYCLE_PER_US);
}



void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen)
{
    send_packet(nodeAddr, rxAddr, txBuffer, pktlen, 0);
}
