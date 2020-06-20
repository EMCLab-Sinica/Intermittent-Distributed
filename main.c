
/*
 *  main.c
 *
 *  Author: Meenchen
 */
#define TestDB

/* Scheduler include files. */
#include <FreeRTOS.h>
#include <task.h>
#include "timers.h"

#include "CC1101_MSP430.h"

/* My tools*/
#include <RecoveryHandler/Recovery.h>
#include <Tools/myuart.h>
#include "RFHandler.h"
#include <Tasks/TestTasks.h>
#include "config.h"
#include "Validation.h"
#include "DBServiceRoutine.h"
#include "RFHandler.h"

/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
functionality in an interrupt. */
#include "driverlib.h"
#include "main.h"

/*-----------------------------------------------------------*/
/*
 * Configure the hardware as necessary.
 */
static void prvSetupHardware(void);
static void setupTimerTasks(void);

unsigned short SemphTCB;
uint8_t nodeAddr = 2;
extern QueueHandle_t DBServiceRoutinePacketQueue;
extern unsigned int volatile stopTrack;

TaskHandle_t DBSrvTaskHandle = NULL;

/*-----------------------------------------------------------*/

int main(void)
{
    /* Configure the hardware ready to run the demo. */
    prvSetupHardware();
    print2uart("Node id: %d\n", nodeAddr);

    initRF(&nodeAddr);

    if (firstTime != 1)
    {
        print2uart("First time\n");
        timeCounter = 0;
        /* Do not call pvPortMalloc before pvInitHeapVar(), it will fail due to the heap is not initialized.
         */
        pvInitHeapVar();
        initRecoveryEssential();
        NVMDBConstructor();
        VMDBConstructor();
        /* Initialize RF*/
        initRFQueues();
        enableRFInterrupt();

        stopTrack = 1;
        // system task
        xTaskCreate(DBServiceRoutine, "DBServ", 400, NULL, 1, NULL);
        xTaskCreate(vRequestDataTimer, "DBSrvTimer", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        stopTrack = 0;
        if (nodeAddr == 1)  // testing
        {
            xTaskCreate(localAccessTask, "LocalAccess", configMINIMAL_STACK_SIZE, NULL, 0, NULL );
        }
        else if (nodeAddr == 2)
        {
            xTaskCreate(remoteAccessTask, "RemoteAccess", 400, NULL, 0, NULL );
        }
        else if (nodeAddr == 3)
        {
            xTaskCreate(remoteAccessTask, "RemoteAccess", 400, NULL, 0, NULL );
        }

        sendWakeupSignal();
        vTaskStartScheduler();

    }
    else
    {
        print2uart("Recovery\n");
        VMDBConstructor();
        initRFQueues();
        enableRFInterrupt();
        failureRecovery();
        BaseType_t xReturned;

        stopTrack = 1;
        xReturned =  xTaskCreate(DBServiceRoutine, "DBServ", 400, NULL, 1, NULL);
        xReturned = xTaskCreate(vRequestDataTimer, "DBSrvTimer", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        stopTrack = 0;

        sendWakeupSignal();
        /* Start the scheduler. */
        vTaskStartScheduler();
    }

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the Idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details on the FreeRTOS heap
    http://www.freertos.org/a00111.html. */
    for (;;)
        ;
}

static void prvSetupHardware(void)
{
    /* Stop Watchdog timer. */
    WDT_A_hold(__MSP430_BASEADDRESS_WDT_A__);

    /* Set all GPIO pins to output and low. */
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15);
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_PJ, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7 | GPIO_PIN8 | GPIO_PIN9 | GPIO_PIN10 | GPIO_PIN11 | GPIO_PIN12 | GPIO_PIN13 | GPIO_PIN14 | GPIO_PIN15);

    /* Configure P2.0 - UCA0TXD and P2.1 - UCA0RXD. */
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0);
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN1, GPIO_SECONDARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN0, GPIO_SECONDARY_MODULE_FUNCTION);

    /* Set PJ.4 and PJ.5 for LFXT. */
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_PJ, GPIO_PIN4 + GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION);

    // Configure button S1 (P5.6) interrupt and S2 P(5.5)
    GPIO_selectInterruptEdge(GPIO_PORT_P5, GPIO_PIN6, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_selectInterruptEdge(GPIO_PORT_P5, GPIO_PIN5, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN5);

    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN5);

    /* Set DCO frequency to 16 MHz. */
    setFrequency(FreqLevel);

    /* Set external clock frequency to 32.768 KHz. */
    CS_setExternalClockSource(32768, 0);

    /* Set ACLK = LFXT. */
    CS_initClockSignal(CS_ACLK, CS_LFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set SMCLK = DCO with frequency divider of 1. */
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set MCLK = DCO with frequency divider of 1. */
    CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Start XT1 with no time out. */
    CS_turnOnLFXT(CS_LFXT_DRIVE_0);

    /* Disable the GPIO power-on default high-impedance mode. */
    PMM_unlockLPM5();

    /* Initialize Uart */
    uartinit();
}
/*-----------------------------------------------------------*/

int _system_pre_init(void)
{
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
__interrupt void Port_5(void)
{
    GPIO_disableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_disableInterrupt(GPIO_PORT_P5, GPIO_PIN5);

    /* Button pushed, do something if you need to */

    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN5);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN5);
}

/*
 * port 8 interrupt service routine to handle RF packet recv
 *
 */
#pragma vector = PORT8_VECTOR
__interrupt void Port_8(void)
{
    static uint8_t buf[MAX_PACKET_LEN];
    static uint8_t my_addr, pktlen, src_addr;
    static BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    static BaseType_t xSendQueueResult;
    if (GDO2_INT_FLAG_IS_SET())
    {
        DISABLE_GDO2_INT();

        if (spi_read_register(IOCFG2) == 0x06) //if sync word detect mode is used
        {
            while (GDO2_PIN_IS_HIGH())
            {
                print2uart("wait for sync word");
            }
        }

        get_packet(buf, &pktlen, &my_addr, &src_addr);
        static PacketHeader_t *packetHeader = (PacketHeader_t *)buf;

        if (packetHeader->rxAddr == my_addr || packetHeader->txAddr == BROADCAST_ADDRESS)
        {
            if (packetHeader->packetType <= ResponseData)
            {
                xSendQueueResult = xQueueSendToBackFromISR(DBServiceRoutinePacketQueue,
                                                           buf, NULL);
                if (xSendQueueResult != pdTRUE)
                {
                    print2uart("Send to queue failed\n");
                }
            }
            else if(packetHeader->packetType == DeviceWakeUp)
            {
                BaseType_t xWakeupHigherPriorityTaskWoken = pdFALSE;
                if (DBSrvTaskHandle == NULL)
                {
                    print2uart("WARNING!\n");
                }
                else
                {
                    vTaskNotifyGiveFromISR(DBSrvTaskHandle, &xWakeupHigherPriorityTaskWoken);
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
__interrupt void ADC12_ISR(void)
{
    switch (__even_in_range(ADC12IV, 76))
    {
    case ADC12IV_NONE:
        break; // Vector  0:  No interrupt
    case ADC12IV_ADC12OVIFG:
        break; // Vector  2:  ADC12MEMx Overflow
    case ADC12IV_ADC12TOVIFG:
        break; // Vector  4:  Conversion time overflow
    case ADC12IV_ADC12HIIFG:
        break; // Vector  6:  ADC12HI
    case ADC12IV_ADC12LOIFG:
        break; // Vector  8:  ADC12LO
    case ADC12IV_ADC12INIFG:
        break;                    // Vector 10:  ADC12IN
    case ADC12IV_ADC12IFG0:       // Vector 12:  ADC12MEM0
        ADC12IFGR0 &= ~ADC12IFG0; // Clear interrupt flag
        __bic_SR_register_on_exit(LPM3_bits); // Exit active CPU
        break;
    case ADC12IV_ADC12IFG1: // Vector 14:  ADC12MEM1
        break;
    case ADC12IV_ADC12IFG2:
        break; // Vector 16:  ADC12MEM2
    case ADC12IV_ADC12IFG3:
        break; // Vector 18:  ADC12MEM3
    case ADC12IV_ADC12IFG4:
        break; // Vector 20:  ADC12MEM4
    case ADC12IV_ADC12IFG5:
        break; // Vector 22:  ADC12MEM5
    case ADC12IV_ADC12IFG6:
        break; // Vector 24:  ADC12MEM6
    case ADC12IV_ADC12IFG7:
        break; // Vector 26:  ADC12MEM7
    case ADC12IV_ADC12IFG8:
        break; // Vector 28:  ADC12MEM8
    case ADC12IV_ADC12IFG9:
        break; // Vector 30:  ADC12MEM9
    case ADC12IV_ADC12IFG10:
        break; // Vector 32:  ADC12MEM10
    case ADC12IV_ADC12IFG11:
        break; // Vector 34:  ADC12MEM11
    case ADC12IV_ADC12IFG12:
        break; // Vector 36:  ADC12MEM12
    case ADC12IV_ADC12IFG13:
        break; // Vector 38:  ADC12MEM13
    case ADC12IV_ADC12IFG14:
        break; // Vector 40:  ADC12MEM14
    case ADC12IV_ADC12IFG15:
        break; // Vector 42:  ADC12MEM15
    case ADC12IV_ADC12IFG16:
        break; // Vector 44:  ADC12MEM16
    case ADC12IV_ADC12IFG17:
        break; // Vector 46:  ADC12MEM17
    case ADC12IV_ADC12IFG18:
        break; // Vector 48:  ADC12MEM18
    case ADC12IV_ADC12IFG19:
        break; // Vector 50:  ADC12MEM19
    case ADC12IV_ADC12IFG20:
        break; // Vector 52:  ADC12MEM20
    case ADC12IV_ADC12IFG21:
        break; // Vector 54:  ADC12MEM21
    case ADC12IV_ADC12IFG22:
        break; // Vector 56:  ADC12MEM22
    case ADC12IV_ADC12IFG23:
        break; // Vector 58:  ADC12MEM23
    case ADC12IV_ADC12IFG24:
        break; // Vector 60:  ADC12MEM24
    case ADC12IV_ADC12IFG25:
        break; // Vector 62:  ADC12MEM25
    case ADC12IV_ADC12IFG26:
        break; // Vector 64:  ADC12MEM26
    case ADC12IV_ADC12IFG27:
        break; // Vector 66:  ADC12MEM27
    case ADC12IV_ADC12IFG28:
        break; // Vector 68:  ADC12MEM28
    case ADC12IV_ADC12IFG29:
        break; // Vector 70:  ADC12MEM29
    case ADC12IV_ADC12IFG30:
        break; // Vector 72:  ADC12MEM30
    case ADC12IV_ADC12IFG31:
        break; // Vector 74:  ADC12MEM31
    case ADC12IV_ADC12RDYIFG:
        break; // Vector 76:  ADC12RDY
    default:
        break;
    }
}
