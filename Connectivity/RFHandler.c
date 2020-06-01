#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "Recovery.h"
#include "RFHandler.h"
#include "CC1101_MSP430.h"
#include "config.h"

#include "mylist.h"
#include "myuart.h"

#define  DEBUG 1

QueueHandle_t RFReceiverQueue;
extern MyList_t *dataTransferLogList;
extern uint8_t nodeAddr;

uint8_t initRFQueues()
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

void RFHandleReceive()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    static PacketHeader_t *packetHeader;

    print2uart("Receive Hander Online\n");

    while (1)
    {
        xQueueReceive(RFReceiverQueue, (void *)packetBuf, portMAX_DELAY);
        packetHeader = (PacketHeader_t *)packetBuf;

        switch (packetHeader->packetType)
        {
        case RequestData:
        {
            const DataControlPacket_t *packet = (DataControlPacket_t *)packetBuf;
            uint8_t dataReceiver = (*packet).header.txAddr;
            if (DEBUG)
                print2uart("RequestData: dataId: %x \n", packet->dataId);

            createDataTransferLog(response, packet->dataId, NULL, NULL);

            // send control message: start
            data_t *data = getDataRecord(packet->owner, packet->dataId, all);
            if (data == NULL)
            {
                print2uart("Can not find data with (%d, %d)...\n", packet->owner, packet->dataId);
                break;
            }
            data_t dataCopy;
            memcpy(&dataCopy, data, sizeof(data_t));
            dataCopy.ptr = NULL;

            PacketHeader_t header = {.packetType = ResponseDataStart};
            TransferDataStartPacket_t startPacket = {
                .header = header,
                .data = dataCopy
                };

            RFSendPacket(dataReceiver, (uint8_t *)&startPacket, sizeof(startPacket));

            // start to send data payload
            uint32_t dataSize = data->size;
            uint8_t payloadSize = 0, chunkNum = 0;
            while (dataSize > 0)
            {
                if (dataSize > CHUNK_SIZE)
                {
                    payloadSize = CHUNK_SIZE;
                }
                else
                {
                    payloadSize = dataSize;
                }

                header.packetType = ResponseDataPayload;
                TransferDataPayloadPacket_t payloadPkt =
                    {
                        .header = header,
                        .owner = data->owner,
                        .dataId = data->id,
                        .chunkNum = chunkNum,
                        .payloadSize = payloadSize};
                memcpy(payloadPkt.payload, data->ptr, payloadSize);

                // calculate the real packet size:  control message (header) + the real payload size
                RFSendPacket(dataReceiver, (uint8_t *)&payloadPkt,
                             sizeof(payloadPkt) - CHUNK_SIZE + payloadSize);

                dataSize -= payloadSize;
                chunkNum += 1;
            }

            // send control message: end
            header.packetType = ResponseDataEnd;
            DataControlPacket_t endPacket = {.header = header, .owner = data->owner, .dataId = data->id};
            RFSendPacket(dataReceiver, (uint8_t *)&endPacket, sizeof(endPacket));

            deleteDataTransferLog(response, packet->dataId);
            break;
        }

        case ResponseDataStart:
        {
            TransferDataStartPacket_t *packet = (TransferDataStartPacket_t *)packetBuf;
            if (DEBUG)
                print2uart("ResponseDataStart: dataId: %x \n", packet->data.id);

            // read request log and buffer
            DataTransferLog_t *log = getDataTransferLog(request, packet->data.id);
            data_t *data = log->xDataObj;
            void *localDataBuf = data->ptr;

            *data = packet->data;
            data->ptr = localDataBuf;   // put back;

            break;
        }

        case ResponseDataPayload:
        {
            TransferDataPayloadPacket_t *packet = (TransferDataPayloadPacket_t *)packetBuf;

            if (DEBUG)
                print2uart("ResponseDataPayload: dataId: %x \n", packet->dataId);

            DataTransferLog_t *log = getDataTransferLog(request, packet->dataId);
            data_t *data = log->xDataObj;

            uint8_t offset = packet->chunkNum * CHUNK_SIZE;
            memcpy((uint8_t *)(data->ptr) + offset, packet->payload, packet->payloadSize);
            break;
        }

        case ResponseDataEnd:
        {
            DataControlPacket_t *packet = (DataControlPacket_t *)packetBuf;
            DataTransferLog_t *log = getDataTransferLog(request, packet->dataId);

            if (DEBUG)
                print2uart("End of response of dataId: %d, TaskHandle: %x \n", log->dataId, log->xFromTask);

            if (xTaskNotifyGive(*(log->xFromTask)) == pdPASS)
            {
                print2uart("Wake up task\n");
            } else
            {
                print2uart("Wake up failed\n");
            }


            deleteDataTransferLog(request, packet->dataId);
            break;
        }

        default:
            print2uart("Unknown Request: %d\n", packetHeader->packetType);
            break;
        }
    }
}

void RFSendPacket(uint8_t rxAddr, uint8_t *txBuffer, uint8_t pktlen)
{
    send_packet(nodeAddr, rxAddr, txBuffer, pktlen, 0);
}
