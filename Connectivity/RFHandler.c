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
