
#include "RFHandler.h"
#include "Recovery.h"
#include "DBServiceRoutine.h"
#include "myuart.h"

#define  DEBUG 0
#define INFO 0

#pragma NOINIT(DBServiceRoutinePacketQueue);
QueueHandle_t DBServiceRoutinePacketQueue;

void DBServiceRoutine()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    static PacketHeader_t *packetHeader;

    while (1)
    {
        xQueueReceive(DBServiceRoutinePacketQueue, (void *)packetBuf, portMAX_DELAY);
        packetHeader = (PacketHeader_t *)packetBuf;

        switch (packetHeader->packetType)
        {
        case RequestData:
        {
            const RequestDataPacket_t *packet = (RequestDataPacket_t *)packetBuf;
            if (INFO)
                print2uart("RequestData: (%d, %d), from task (%d, %d)\n",
                           packet->dataId.owner, packet->dataId.id, packet->taskId.nodeAddr, packet->taskId.id);

            Data_t *data = getDataRecord(packet->dataId, all);
            if (data == NULL)
            {
                print2uart("Can not find data with (%d, %d)...\n", packet->dataId.owner, packet->dataId.id);
                break;
            }
            ResponseDataPacket_t resPacket = {.header.packetType = ResponseData};
            resPacket.data = *data;
            resPacket.taskId = packet->taskId;
            memcpy(&(resPacket.dataPayload), data->ptr, data->size);

            RFSendPacket(packet->header.txAddr, (uint8_t *)&resPacket, sizeof(resPacket));

            break;
        }

        case ResponseData:
        {
            ResponseDataPacket_t *packet = (ResponseDataPacket_t *)packetBuf;
            if (DEBUG)
                print2uart("ResponseData: dataId: %d \n", packet->data.dataId.id);

            // read request log and buffer
            DataRequestLog_t *log = getDataRequestLog(packet->taskId, packet->data.dataId);
            if(log == NULL)
            {
                if(DEBUG)
                    print2uart("Log not found, task (%d, %d), data (%d, %d)\n",
                               packet->taskId.nodeAddr, packet->taskId.id, packet->data.dataId.owner, packet->data.dataId);
                break;
            }

            memcpy(log->xToDataObj->ptr, packet->dataPayload, packet->data.size);

            if (xTaskNotifyGive(*(log->xFromTask)) != pdPASS)
            {
                print2uart("Wake up failed\n");
            }

            // deactive log entry
            log->valid = false;

            break;
        }

        default:
            print2uart("Unknown Request: %d\n", packetHeader->packetType);
            break;
        }
    }
}
