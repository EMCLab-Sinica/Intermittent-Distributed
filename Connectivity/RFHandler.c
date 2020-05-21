#include "CC1101_MSP430.h"
#include "RFHandler.h"
#include <stdint.h>
#include "myuart.h"
#include <FreeRTOS.h>
#include <task.h>

QueueHandle_t RFReceiverQueue;

uint8_t init_rf_queues()
{
    /* Create a queue capable of containing 5 rf packets. */
    RFReceiverQueue = xQueueCreate(2, MAX_PACKET_LEN);
    if (RFReceiverQueue == NULL)
    {
        print2uart("Error: RF Receiver Queue init failed\n");
        while (1)
            ;
    }
    xQueueReset(RFReceiverQueue);
    return TRUE;
}

void rf_handle_receive()
{
    static uint8_t buf[MAX_PACKET_LEN];
    static packet_header_t *packetHeader;
    print2uart("Receive Hander Online\n");

    while (1)
    {
        xQueueReceive(RFReceiverQueue, (void *)buf, portMAX_DELAY);
        packetHeader = (packet_header_t *)buf;

        switch (packetHeader->packetType)
        {
        case request_data:
        {
            // request_data_pkt_t *packet = (request_data_pkt_t *)buf;
            // print2uart("Request Type: %x \n", packet->header.packetType);
            // print2uart("REQ DATA ID: %x \n", packet->dataId);

            // // data_t data = *getDataRecord(packet->dataId);

            // packet_header_t header = {.packetType = response_data_start};
            // response_data_ctrl_pkt_t start_pkt =
            //     {
            //         .header = header,
            //         .dataSize = data.size,
            //         .version = data.version,
            //         .validationTS = data.validationTS
            //     };
            // // send packet

            // uint8_t dataSize = data.size;
            // uint8_t payloadSize = 0;
            // while (dataSize > 0)
            // {
            //     if (dataSize > MAX_DATA_PAYLOAD_SIZE)
            //     {
            //         payloadSize = MAX_DATA_PAYLOAD_SIZE;
            //     }
            //     else
            //     {
            //         payloadSize = dataSize;
            //     }

            //     header.packetType = response_data_payload;
            //     response_data_payload_pkt_t payload_pkt =
            //         {
            //             .header = header,
            //             .payloadSize = payloadSize
            //         };
            //     memcpy(payload_pkt.payload, data.ptr, payloadSize);
            //     // send packet

            //     dataSize -= payloadSize;
            // }
            // header.packetType;
            // response_data_ctrl_pkt_t end_pkt = {.header = header};
            // // send packet

            break;
        }

        case response_data_start:
        {
            break;
        }

        case response_data_payload:
        {
            break;
        }

        case response_data_end:
        {
            break;
        }
        default:
            print2uart("Unknown Request: %d\n", packetHeader->packetType);
            break;
        }
    }
}

void rf_send_task()
{
    print2uart("RF Sender\n");
    receive();
    // enable RF interrupts
    uint8_t my_addr = 0, rx_addr = 0, tx_retries = 5;

    packet_header_t header = {.packetType = request_data};
    request_data_pkt_t packet = {.header = header, .dataId = 3};

    while (1)
    {
        send_packet(my_addr, rx_addr, (uint8_t *)&packet, sizeof(request_data_pkt_t), tx_retries);
        vTaskDelay(500);
    }
}
