#ifndef RFHANDLER_H
#define RFHANDLER_H

#include "FreeRTOS.h"
#include "Queue.h"
#include "task.h"
#include "SimpDB.h"
#include "CC1101_MSP430.h"
#include "Validation.h"

/* CC1101 Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | payload data [1..60bytes]
*/
#define MAX_PACKET_LEN 62   // 0x3E
#define PACKET_HEADER_LEN 6 // enum can be 1byte or 2, not sure #TODO: enum size
#define CHUNK_SIZE 53       // 62-6(header)-3(dataId, payloadSize, chunkSize)

/* Our Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | request_type [1byte] | payload data [1..bytes]
*/

typedef enum PacketType
{
    RequestData,
    ResponseData,
    SyncCounter,
    DeviceWakeUp,

    // Validation Phase 1
    ValidationP1Request,
    ValidationP1Response,
    // Validation Phase 2, locking object
    ValidationP2Request,
    ValidationP2Response,
    // Commit Phase
    CommitRequest,
    CommitResponse

} PacketType_e;

typedef struct PacketHeader
{
    uint8_t pktlen;
    uint8_t rxAddr;
    PacketType_e packetType;

} PacketHeader_t;

typedef struct RequestDataPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;

} RequestDataPacket_t;

typedef struct ResponseDataPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataTransPacket_t data;

} ResponseDataPacket_t;

typedef struct SyncCounterPacket
{
    PacketHeader_t header;
    uint64_t timeCounter;

} SyncCounterPkt_t;

typedef struct DeviceWakeUpPacket
{
    PacketHeader_t header;

} DeviceWakeUpPacket_t;

// Validation Phase 1: send data and request VI
typedef struct ValidationP1RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataTransPacket_t data;

} ValidationP1RequestPacket_t;

typedef struct ValidationP1ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;
    TimeInterval_t taskInterval;
    uint8_t maybeCommit;

} ValidationP1ResponsePacket_t;

typedef struct ValidationP2RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    uint8_t decision;

} ValidationP2RequestPacket_t;

typedef struct ValidationP2ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    uint8_t ownerAddr;
    uint8_t finalValidationPassed;

} ValidationP2ResponsePacket_t;

typedef struct CommitRequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;
    uint8_t decision;

} CommitRequestPacket_t;

typedef struct CommitResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;

} CommitResponsePacket_t;

void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen);
void sendWakeupSignal();

#endif // RFHANDLER_H
