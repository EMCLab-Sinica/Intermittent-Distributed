#ifndef RFHANDLER_H
#define RFHANDLER_H

#include "FreeRTOS.h"
#include "Queue.h"
#include "task.h"
#include "SimpDB.h"
#include "CC1101_MSP430.h"


/* CC1101 Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | payload data [1..60bytes]
*/
#define MAX_PACKET_LEN 62 // 0x3E
#define PACKET_HEADER_LEN  6 // enum can be 1byte or 2, not sure #TODO: enum size
#define CHUNK_SIZE  53 // 62-6(header)-3(dataId, payloadSize, chunkSize)

/* Our Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | request_type [1byte] | payload data [1..bytes]
*/

typedef enum PacketType
{
    RequestData,
    ResponseDataStart,
    ResponseDataPayload,
    ResponseDataEnd,
    SyncCounter

} PacketType_e;

typedef struct PacketHeader
{
    uint8_t pktlen;
    uint8_t rxAddr;
    uint8_t txAddr;
    uint8_t sessionId;
    PacketType_e packetType;

} PacketHeader_t;

typedef struct DataControlPacket
{
    PacketHeader_t header;
    uint8_t owner;
    uint8_t dataId;

} DataControlPacket_t;

typedef struct TransferDataStartPacket
{
    PacketHeader_t header;
    Data_t data;

} TransferDataStartPacket_t;

typedef struct TransferDataPayloadPacket
{
    PacketHeader_t header;
    uint8_t owner;
    uint8_t dataId;
    uint8_t payloadSize;
    uint8_t chunkNum;
    uint8_t payload[CHUNK_SIZE];

} TransferDataPayloadPacket_t;

typedef struct SyncCounterPacket
{
    PacketHeader_t header;
    uint64_t timeCounter;

} SyncCounterPkt_t;


void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen);
uint8_t initRFQueues();
void RFHandleReceive();


#endif // RFHANDLER_H
