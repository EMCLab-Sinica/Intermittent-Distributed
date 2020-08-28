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
    // Commit Phase 1
    CommitP1Request,
    CommitP1Response,
    // Validation Phase 2, locking object
    ValidationP2Request,
    ValidationP2Response,
    // Commit Phase 2
    CommitP2Request,
    CommitP2Response

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

// Validation Phase 1
typedef struct ValidationP1RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataHeader_t dataHeader; // Data_t without real content
} ValidationP1RequestPacket_t;

typedef struct ValidationP1ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataHeader_t dataHeader;
    TimeInterval_t taskInterval;

} ValidationP1ResponsePacket_t;

// FIXME: data number (1) and size (8bytes) limitation in a packet
typedef struct CommitP1RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataTransPacket_t data;

} CommitP1RequestPacket_t;

typedef struct CommitP1ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;
    bool maybeCommit;

} CommitP1ResponsePacket_t;

typedef struct ValidationP2RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    bool decision;

} ValidationP2RequestPacket_t;

typedef struct ValidationP2ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    uint8_t ownerAddr;
    bool finalValidationPassed;

} ValidationP2ResponsePacket_t;

typedef struct CommitP2RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    bool decision;

} CommitP2RequestPacket_t;

typedef struct CommitP2ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;

} CommitP2ResponsePacket_t;

void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen);
void sendWakeupSignal();

#endif // RFHANDLER_H
