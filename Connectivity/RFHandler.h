#ifndef RFHANDLER_H
#define RFHANDLER_H

#include "FreeRTOS.h"
#include "Queue.h"
#include "SimpleDB.h"


/* CC1101 Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | payload data [1..60bytes]
*/
#define MAX_PACKET_LEN 62 // 0x3E
#define PACKET_HEADER_LEN  6 // enum can be 1byte or 2, not sure #TODO: enum size
#define MAX_DATA_PAYLOAD_SIZE 52

/* Our Packet Format
pkt_len [1byte] | rx_addr [1byte] | tx_addr [1byte] | request_type [1byte] | payload data [1..bytes]
*/

typedef enum PacketType
{
    request_data,
    response_data_start,
    response_data_payload,
    response_data_end

} packet_type_e;

typedef struct PacketHeader
{
    uint8_t pktlen;
    uint8_t rxAddr;
    uint8_t txAddr;
    uint8_t sessionId;
    packet_type_e packetType;

} packet_header_t;

typedef struct RequestDataPacket
{
    packet_header_t header;
    uint8_t dataId;

} request_data_pkt_t;

typedef struct ResponseDataControlPacket
{
    packet_header_t header;
    uint8_t dataId;
    uint8_t dataSize;
    data_version_e version;
    uint8_t validationTS;

} response_data_ctrl_pkt_t;

typedef struct ResponseDataPayloadPacket
{
    packet_header_t header;
    uint8_t payloadSize;
    uint8_t payload[MAX_DATA_PAYLOAD_SIZE];

} response_data_payload_pkt_t;


void rf_receive_task();
void rf_send_task();
uint8_t init_rf_queues();
void rf_handle_receive();


#endif // RFHANDLER_H
