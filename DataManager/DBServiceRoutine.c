
#include "RFHandler.h"
#include "Recovery.h"
#include "DBServiceRoutine.h"
#include "myuart.h"

#define ECB 1
#include <ThirdParty/tiny-AES-c/aes.h>

#define  DEBUG 1
#define INFO 1

#pragma NOINIT(DBServiceRoutinePacketQueue);
QueueHandle_t DBServiceRoutinePacketQueue;
extern int firstTime;

// AES ECB Key
static uint8_t key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };

uint8_t initDBSrvQueues()
{
    if (firstTime != 1)
    {
        DBServiceRoutinePacketQueue = xQueueCreate(5, MAX_PACKET_LEN);
        if (DBServiceRoutinePacketQueue == NULL)
        {
            print2uart("Error: DB Service Routine Queue init failed\n");
        }
    }
    else
    {
        vQueueDelete(DBServiceRoutinePacketQueue);
        DBServiceRoutinePacketQueue = xQueueCreate(5, MAX_PACKET_LEN);
    }


    return TRUE;
}

void DBServiceRoutine()
{
    static uint8_t packetBuf[MAX_PACKET_LEN];
    static PacketHeader_t *packetHeader;
    static struct AES_ctx aes_ctx;
    static uint8_t dataPadded[MAX_DB_OBJ_SIZE];

    AES_init_ctx(&aes_ctx, key);

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
            // Encryption, pad the data to 16 bytes
            memset(dataPadded, 0, MAX_DB_OBJ_SIZE);
            memcpy(dataPadded, data->ptr, data->size);
            AES_ECB_encrypt(&aes_ctx, dataPadded);

            // Send
            ResponseDataPacket_t resPacket = {.header.packetType = ResponseData, .taskId=packet->taskId};
            DataTransPacket_t *resData = &(resPacket.data);
            resData->dataId = data->dataId;
            resData->version = data->version;
            resData->size = data->size;
            memcpy(&(resPacket.data.content), dataPadded, MAX_DB_OBJ_SIZE);

            RFSendPacket(packet->taskId.nodeAddr, (uint8_t *)&resPacket, sizeof(resPacket));

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

            // decrypt
            memset(dataPadded, 0, MAX_DB_OBJ_SIZE);
            memcpy(dataPadded, packet->data.content, MAX_DB_OBJ_SIZE);
            AES_ECB_decrypt(&aes_ctx, dataPadded);

            memcpy(log->xToDataObj->ptr, dataPadded, packet->data.size);

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
