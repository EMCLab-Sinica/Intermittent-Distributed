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
#define MAX_PACKET_LEN 62 // 0x3E
#define PACKET_HEADER_LEN  6 // enum can be 1byte or 2, not sure #TODO: enum size
#define CHUNK_SIZE  53 // 62-6(header)-3(dataId, payloadSize, chunkSize)

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
    VPhase1Request,
    VPhase1Response,
    // Commit Phase 1
    CPhase1Request,
    CPhase1Response,
    // Validation Phase 2, locking object
    VPhase2Request,
    VPhase2Response,
    // Commit Phase 2
    CPhase2Request,
    CPhase2Response

} PacketType_e;

typedef struct PacketHeader
{
    uint8_t pktlen;
    uint8_t rxAddr;
    uint8_t txAddr;
    uint8_t sessionId;
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
    Data_t data;
    uint8_t dataPayload[16]; // max 16bytes

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
typedef struct VPhase1RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    Data_t dataWOContent; // Data_t without real content
} VPhase1RequestPacket_t;

typedef struct VPhase1ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    Data_t dataWOContent;
    TimeInterval_t taskInterval;

} VPhase1ResponsePacket_t;

// FIXME: data number (1) and size (8bytes) limitation in a packet
typedef struct CPhase1RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    Data_t dataWOContent;
    uint8_t dataPayload[8];

} CPhase1RequestPacket_t;

typedef struct CPhase1ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    DataUUID_t dataId;
    bool maybeCommit;

} CPhase1ResponsePacket_t;

typedef struct VPhase2RequestPacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    bool decision;

} VPhase2RequestPacket_t;

typedef struct VPhase2ResponsePacket
{
    PacketHeader_t header;
    TaskUUID_t taskId;
    bool finalValidationPassed;

} VPhase2ResponsePacket_t;

void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen);
uint8_t initRFQueues();
void sendWakeupSignal();

#endif // RFHANDLER_H
