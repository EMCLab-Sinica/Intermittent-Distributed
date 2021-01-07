#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "queue.h"
#include "Recovery.h"
#include "RFHandler.h"
#include "CC1101_MSP430.h"
#include "config.h"

#include "myuart.h"

#define  DEBUG 1


extern uint8_t nodeAddr;
extern int firstTime;
extern uint64_t timeCounter;


void syncTime(uint8_t* timeSynced)
{
    DeviceWakeUpPacket_t packet = {.header.packetType = DeviceWakeUp, .addr = nodeAddr,};
    do
    {
        RFSendPacket(0, (uint8_t *)&packet, sizeof(packet));
        __delay_cycles(200000 * CYCLE_PER_US);
    } while(*timeSynced == 0);
}

void sendSyncTimeResponse(uint8_t rxAddr)
{
    SyncCounterPacket_t packet = {.header.packetType = SyncCounter, .timeCounter = timeCounter};
    RFSendPacket(rxAddr, (uint8_t *)&packet, sizeof(packet));
}

void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen)
{
    send_packet(rxAddr, txBuffer, pktlen, 0);
}

void RFSendPacketNoRTOS(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen)
{
    send_packet_no_rtos(rxAddr, txBuffer, pktlen, 0);
}
