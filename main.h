/* Used for maintaining a 32-bit run time stats counter from a 16-bit timer. */
volatile uint32_t ulRunTimeCounterOverflows = 0;


uint32_t divider = 0;
uint32_t timeCounter = 0;

/* Use for recovery */
#pragma DATA_SECTION(firstTime, ".map") //indicate whether task stacks exist
int firstTime;

/* -------------- FreeRTOS related functions --------------- */
/* Prototypes for the standard FreeRTOS callback/hook functions implemented */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );
/*-----------------------------------------------------------*/
/* Called if a call to pvPortMalloc() fails because there is insufficient
free memory available in the FreeRTOS heap.  pvPortMalloc() is called
internally by FreeRTOS API functions that create tasks, queues, software
timers, and semaphores.  The size of the FreeRTOS heap is set by the
configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
void vApplicationMallocFailedHook( void )
{
    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/
/* Run time stack overflow checking is performed if
configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
function is called if a stack overflow is detected.
See http://www.freertos.org/Stacks-and-stack-overflow-checking.html */
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;
    print2uart("Error: %s Application StackOverflow\n", pcTaskName);

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/
/* Can be used to implement background services */
void vApplicationIdleHook( void )
{
    __bis_SR_register( LPM4_bits + GIE );
    __no_operation();
}
/*-----------------------------------------------------------*/
/* Hook at each application tick */
void vApplicationTickHook( void )
{
    divider++;
    if (divider >= 200)
    {
        timeCounter++; //keep track of running time
        divider = 0;
    }
    return;
}
/*-----------------------------------------------------------*/

/* The MSP430X port uses this callback function to configure its tick interrupt.
This allows the application to choose the tick interrupt source.
configTICK_VECTOR must also be set in FreeRTOSConfig.h to the correct
interrupt vector for the chosen tick interrupt source.  This implementation of
vApplicationSetupTimerInterrupt() generates the tick from timer A0, so in this
case configTICK_VECTOR is set to TIMER0_A0_VECTOR. */
void vApplicationSetupTimerInterrupt( void )
{
const unsigned short usACLK_Frequency_Hz = 32768;

    /* Ensure the timer is stopped. */
    TA0CTL = 0;

    /* Run the timer from the ACLK. */
    TA0CTL = TASSEL_1;

    /* Clear everything to start with. */
    TA0CTL |= TACLR;

    /* Set the compare match value according to the tick rate we want. */
    TA0CCR0 = usACLK_Frequency_Hz / configTICK_RATE_HZ;

    /* Enable the interrupts. */
    TA0CCTL0 = CCIE;

    /* Start up clean. */
    TA0CTL |= TACLR;

    /* Up mode. */
    TA0CTL |= MC_1;
}
/*-----------------------------------------------------------*/

void vConfigureTimerForRunTimeStats( void )
{
    /* Configure a timer that is used as the time base for run time stats.  See
    http://www.freertos.org/rtos-run-time-stats.html */

    /* Ensure the timer is stopped. */
    TA1CTL = 0;

    /* Start up clean. */
    TA1CTL |= TACLR;

    /* Run the timer from the ACLK/8, continuous mode, interrupt enable. */
    TA1CTL = TASSEL_1 | ID__8 | MC__CONTINUOUS | TAIE;
}

#pragma vector=TIMER1_A1_VECTOR
__interrupt void v4RunTimeStatsTimerOverflow( void )
{
    TA1CTL &= ~TAIFG;

    /* 16-bit overflow, so add 17th bit. */
    ulRunTimeCounterOverflows += 0x10000;
    __bic_SR_register_on_exit( SCG1 + SCG0 + OSCOFF + CPUOFF );
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    print2uart("Hey, don't use STATIC!\n");
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*———————————————————–*/


