/*
 * SenseCap.c
 *
 *  Created on: 2018¦~3¤ë13¤é
 *      Author: Meenchen
 */
#include <FreeRTOS.h>
#include <task.h>
#include <driverlib.h>
#include <DataManager/SimpDB.h>
#include "SenseCap.h"
#include "Semph.h"
#include <config.h>

int Cbuffersize = 5;
int Cptr;
int Ccircled;
extern int waitCap;
extern int ADCSemph;
extern int capID;
extern unsigned long information[10];
extern int readCap;

void CapLog(){
    while(1)
    {
        acquireSemph(pxCurrentTCB->uxTCBNumber);
        //Initialize for validity interval
        registerTCB(IDCAP);

        // Initialize the shared reference module
        // By default, REFMSTR=1 => REFCTL is used to configure the internal reference
        while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
        REFCTL0 |= REFVSEL_2 + REFON;           // Enable internal 1.2V reference

        // Initialize ADC12_A
        ADC12CTL0 &= ~ADC12ENC;                 // Disable ADC12
        ADC12CTL0 = ADC12SHT0_8 + ADC12ON;      // Set sample time
        ADC12CTL1 |= ADC12SHP | ADC12CONSEQ_1;   // Enable sample timer
        ADC12CTL2 |= ADC12RES_2;                  // 12-bit conversion results
        ADC12CTL3 = ADC12BATMAP;                 // Enable internal temperature sensor
        ADC12MCTL0 = ADC12VRSEL_1 + ADC12INCH_31; // ADC input ch A31 => battery sense
        ADC12IER0 = ADC12IE0;                      // ADC_IFG upon conv result-ADCMEMO

        while(!(REFCTL0 & REFGENRDY));          // Wait for reference generator
                                                // to settle
        ADC12CTL0 |= ADC12ENC;

        waitCap = 1;
        ADC12CTL0 |= ADC12SC;               // Sampling and conversion start

        __bis_SR_register(GIE); //  interrupts enabled
        while(waitCap);//wait for sampling, we don't want to mess up the scheduling here

        //write the result
        struct working data;
        unsigned int temp = ADC12MEM0;
        DBworking(&data, 4, capID);//4 bytes for long and for create(-1)
        unsigned long capadd = temp;
        capadd = (capadd * 2 * 2.5 *1000) / 4095;//ref:https://e2e.ti.com/support/microcontrollers/msp430/f/166/t/432697
        data.address = &capadd;
        capID = DBcommit(&data,NULL,NULL,4,1);
        readCap++;
        //validity done
        information[IDCAP]++;
        unresgisterTCB(IDCAP);
        releaseSemph(pxCurrentTCB->uxTCBNumber);
        portYIELD();
    }
}

