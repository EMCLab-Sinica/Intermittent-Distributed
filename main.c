
/*
 *  main.c
 *
 */
#define TestDB

/* Scheduler include files. */
#include <Connectivity/RFHandler.h>
#include <DataManager/DBServiceRoutine.h>
#include <FreeRTOS.h>
#include <Peripheral/dht11.h>
#include <ValidationHandler/Recovery.h>
#include <ValidationHandler/Validation.h>
#include <Tasks/TestTasks.h>
#include <Tools/dvfs.h>
#include <Tools/myuart.h>
#include <config.h>
#include <task.h>
#include <string.h>

#include "CC1101_MSP430.h"
#include "timers.h"

/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
functionality in an interrupt. */
#include "driverlib.h"
#include "main.h"

#define DEBUG 1
#define INFO 0

#pragma NOINIT(ucHeap)
uint8_t ucHeap[configTOTAL_HEAP_SIZE];

#pragma NOINIT(statistics)
uint32_t statistics[4];

extern InboundValidationRecord_t inboundValidationRecords[MAX_GLOBAL_TASKS];
/*-----------------------------------------------------------*/
/*
 * Configure the hardware as necessary.
 */
static void prvSetupHardware(void);

unsigned short SemphTCB;
uint8_t nodeAddr = NODEADDR;
volatile uint8_t timeSynced = 0;

extern QueueHandle_t DBServiceRoutinePacketQueue;
extern QueueHandle_t validationRequestPacketsQueue;
extern unsigned int volatile stopTrack;
extern uint16_t taskIdRecord[4];

TaskHandle_t RecoverySrvTaskHandle = NULL;

/*-----------------------------------------------------------*/

int main(void) {
    prvSetupHardware();
    if (firstTime != 1) {
        pvInitHeapVar();
    }

    initRF(&nodeAddr);
    enableRFInterrupt();
    DeviceWakeUpPacket_t packet = {
        .header.packetType = DeviceWakeUp,
        .addr = nodeAddr,
    };
    do {
        RFSendPacketNoRTOS(SYNCTIME_NODE, (uint8_t *)&packet, sizeof(packet));
        __delay_cycles(9600000); // 600 * 1000 * US_PER_SEC

    } while (timeSynced == 0);
    DISABLE_GDO2_INT(); // disable RF Interrupt for setup

    VMDBConstructor();
    /* Create System Service Tasks */
    stopTrack = 1;
    xTaskCreate(DBServiceRoutine, "DBServ", 600, NULL, 0, NULL);
    xTaskCreate(inboundValidationHandler, "inboundV", 600, NULL, 0, NULL);
    xTaskCreate(outboundValidationHandler, "outboundV", 600, NULL, 0, NULL);
    stopTrack = 0;

    if (firstTime != 1) {
        memset(&statistics, 0, sizeof(uint32_t) * 4);
        if (DEBUG) {
            print2uart("Node id: %d FirstTime\n", nodeAddr);
        }
        initRecoveryEssential();
        NVMDBConstructor();
        initValidationEssentials();

        setupTasks();
        switch (nodeAddr) {
            case 1:
            {
                xTaskCreate(sensingTask, TASKNAME_SENSING, 400, NULL, 0, NULL);
                xTaskCreate(monitorTask, TASKNAME_MONITOR, 400, NULL, 0, NULL);
                break;
            }
            case 2:
            {
                xTaskCreate(sprayerTask, TASKNAME_SPRAYER, 400, NULL, 0, NULL);
                xTaskCreate(reportTask, TASKNAME_REPORT, 400, NULL, 0, NULL);
                break;
            }
            case 3:
            {
                xTaskCreate(sprayerTask, TASKNAME_SPRAYER, 400, NULL, 0, NULL);
                xTaskCreate(reportTask, TASKNAME_REPORT, 400, NULL, 0, NULL);
                break;
            }
            case 4:
            {
                xTaskCreate(sprayerTask, TASKNAME_SPRAYER, 400, NULL, 0, NULL);
                xTaskCreate(reportTask, TASKNAME_REPORT, 400, NULL, 0, NULL);
                break;
            }
        }
        firstTime = 1;  // need to consider recovery after this point

        enableRFInterrupt();
        vTaskStartScheduler();

    } else {
        if (DEBUG) {
            print2uart("Node id: %d Recovery\n", nodeAddr);
        }
        failureRecovery();
        enableRFInterrupt();
        /* Start the scheduler. */
        vTaskStartScheduler();
    }

    /* Should not reach here */
    for (;;)
        ;
}

static void prvSetupHardware(void) {
    /* Stop Watchdog timer. */
    WDT_A_hold(__MSP430_BASEADDRESS_WDT_A__);

    /* Set all GPIO pins to output and low. */
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin( GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15);
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15);

    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);  // Red LED off
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN1);  // Green LED off

    /* Configure P2.0 - UCA0TXD and P2.1 - UCA0RXD. */
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0);
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN1, GPIO_SECONDARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN0, GPIO_SECONDARY_MODULE_FUNCTION);

    /* Set PJ.4 and PJ.5 for LFXT. */
    GPIO_setAsPeripheralModuleFunctionInputPin( GPIO_PORT_PJ, GPIO_PIN4 + GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION);

    // Configure button S1 (P5.6) interrupt and S2 P(5.5)
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_selectInterruptEdge(GPIO_PORT_P5, GPIO_PIN6, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_selectInterruptEdge(GPIO_PORT_P5, GPIO_PIN5, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);

    /* Set DCO frequency to 16 MHz. */
    setFrequency(8);

    /* Set external clock frequency to 32.768 KHz. */
    CS_setExternalClockSource(32768, 0);

    /* Set ACLK = LFXT. */
    CS_initClockSignal(CS_ACLK, CS_LFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set SMCLK = DCO with frequency divider of 1, SMCLK = 16MHz */
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set MCLK = DCO with frequency divider of 1. */
    CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Start XT1 with no time out. */
    CS_turnOnLFXT(CS_LFXT_DRIVE_0);

    /* Disable the GPIO power-on default high-impedance mode. */
    PMM_unlockLPM5();

    /* Initialize Uart */
    uartinit();
    /* Init Timer for DHT-11 */
    if (NODEADDR == DHT11_NODE) {
        setup_MSP430_DHT11();
    }
}
/*-----------------------------------------------------------*/

int _system_pre_init(void) {
    /* Stop Watchdog timer. */
    WDT_A_hold(__MSP430_BASEADDRESS_WDT_A__);

    /* Return 1 for segments to be initialised. */
    return 1;
}

/*
 * port 5 interrupt service routine to handle s1 and s2 button press
 *
 */
#pragma vector = PORT5_VECTOR
__interrupt void Port_5(void) {
    GPIO_disableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_disableInterrupt(GPIO_PORT_P5, GPIO_PIN5);

    /* Button pushed, do something if you need to */
    if ((P5IFG & 1 << 5) == 0)  // P5.5
    {
    }

    if ((P5IFG & 1 << 6) == 0)  // P5.6
    {
        // reset
        memset(statistics, 0, sizeof(uint32_t) * 4);
    }

    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN5);
}

/*
 * port 8 interrupt service routine to handle RF packet recv
 *
 */
#pragma vector = PORT8_VECTOR
__interrupt void Port_8(void) {
    static uint8_t buf[MAX_PACKET_LEN];
    static uint8_t my_addr, pktlen, src_addr;
    static BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    static BaseType_t xSendQueueResult;
    if (GDO2_INT_FLAG_IS_SET()) {
        DISABLE_GDO2_INT();

        get_packet(buf, &pktlen, &my_addr, &src_addr);
        static PacketHeader_t *packetHeader = (PacketHeader_t *)buf;

        if (timeSynced == 0) {
            if (packetHeader->packetType == SyncCounter) {
                SyncCounterPacket_t *packet = (SyncCounterPacket_t *)buf;
                // 106 is the compensation to propagation time
                // the RTT time of an RF packet is 212 ticks
                // which is observed in our experiment.
                timeCounter = packet->timeCounter + 106;
                timeSynced = 1;
            }
        }
        else
        {
            if (packetHeader->packetType <= ResponseData) {
                xSendQueueResult = xQueueSendToBackFromISR(
                    DBServiceRoutinePacketQueue, buf, &xHigherPriorityTaskWoken);
                if (xSendQueueResult != pdTRUE) {
                    print2uart("Send to queue failed\n");
                }
            }

            else if (packetHeader->packetType >= ValidationRequest) {
                xSendQueueResult = xQueueSendToBackFromISR( validationRequestPacketsQueue, buf, NULL);
                if (xSendQueueResult != pdTRUE) {
                    print2uart("Send to validation queue failed\n");
                }
            }
        }

        CLEAR_GDO2_INT_FLAG();
        ENABLE_GDO2_INT();
    }
}

/* Temperature and voltage
 * ADC12 Interrupt Service Routine
 * Exits LPM3 when Temperature/Voltage data is ready
 */
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void) {
    switch (__even_in_range(ADC12IV, 76)) {
        case ADC12IV_NONE:
            break;  // Vector  0:  No interrupt
        case ADC12IV_ADC12OVIFG:
            break;  // Vector  2:  ADC12MEMx Overflow
        case ADC12IV_ADC12TOVIFG:
            break;  // Vector  4:  Conversion time overflow
        case ADC12IV_ADC12HIIFG:
            break;  // Vector  6:  ADC12HI
        case ADC12IV_ADC12LOIFG:
            break;  // Vector  8:  ADC12LO
        case ADC12IV_ADC12INIFG:
            break;                                 // Vector 10:  ADC12IN
        case ADC12IV_ADC12IFG0:                    // Vector 12:  ADC12MEM0
            ADC12IFGR0 &= ~ADC12IFG0;              // Clear interrupt flag
            __bic_SR_register_on_exit(LPM3_bits);  // Exit active CPU
            break;
        case ADC12IV_ADC12IFG1:  // Vector 14:  ADC12MEM1
            break;
        case ADC12IV_ADC12IFG2:
            break;  // Vector 16:  ADC12MEM2
        case ADC12IV_ADC12IFG3:
            break;  // Vector 18:  ADC12MEM3
        case ADC12IV_ADC12IFG4:
            break;  // Vector 20:  ADC12MEM4
        case ADC12IV_ADC12IFG5:
            break;  // Vector 22:  ADC12MEM5
        case ADC12IV_ADC12IFG6:
            break;  // Vector 24:  ADC12MEM6
        case ADC12IV_ADC12IFG7:
            break;  // Vector 26:  ADC12MEM7
        case ADC12IV_ADC12IFG8:
            break;  // Vector 28:  ADC12MEM8
        case ADC12IV_ADC12IFG9:
            break;  // Vector 30:  ADC12MEM9
        case ADC12IV_ADC12IFG10:
            break;  // Vector 32:  ADC12MEM10
        case ADC12IV_ADC12IFG11:
            break;  // Vector 34:  ADC12MEM11
        case ADC12IV_ADC12IFG12:
            break;  // Vector 36:  ADC12MEM12
        case ADC12IV_ADC12IFG13:
            break;  // Vector 38:  ADC12MEM13
        case ADC12IV_ADC12IFG14:
            break;  // Vector 40:  ADC12MEM14
        case ADC12IV_ADC12IFG15:
            break;  // Vector 42:  ADC12MEM15
        case ADC12IV_ADC12IFG16:
            break;  // Vector 44:  ADC12MEM16
        case ADC12IV_ADC12IFG17:
            break;  // Vector 46:  ADC12MEM17
        case ADC12IV_ADC12IFG18:
            break;  // Vector 48:  ADC12MEM18
        case ADC12IV_ADC12IFG19:
            break;  // Vector 50:  ADC12MEM19
        case ADC12IV_ADC12IFG20:
            break;  // Vector 52:  ADC12MEM20
        case ADC12IV_ADC12IFG21:
            break;  // Vector 54:  ADC12MEM21
        case ADC12IV_ADC12IFG22:
            break;  // Vector 56:  ADC12MEM22
        case ADC12IV_ADC12IFG23:
            break;  // Vector 58:  ADC12MEM23
        case ADC12IV_ADC12IFG24:
            break;  // Vector 60:  ADC12MEM24
        case ADC12IV_ADC12IFG25:
            break;  // Vector 62:  ADC12MEM25
        case ADC12IV_ADC12IFG26:
            break;  // Vector 64:  ADC12MEM26
        case ADC12IV_ADC12IFG27:
            break;  // Vector 66:  ADC12MEM27
        case ADC12IV_ADC12IFG28:
            break;  // Vector 68:  ADC12MEM28
        case ADC12IV_ADC12IFG29:
            break;  // Vector 70:  ADC12MEM29
        case ADC12IV_ADC12IFG30:
            break;  // Vector 72:  ADC12MEM30
        case ADC12IV_ADC12IFG31:
            break;  // Vector 74:  ADC12MEM31
        case ADC12IV_ADC12RDYIFG:
            break;  // Vector 76:  ADC12RDY
        default:
            break;
    }
}
