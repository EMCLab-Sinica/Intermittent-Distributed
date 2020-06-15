/*
 * SenseTemp.c
 *
 *  Created on: 2018�~3��13��
 *      Author: Meenchen
 */
#define CALADC12_12V_30C  *((unsigned int *)0x1A1A)   // Temperature Sensor Calibration-30 C
                                                      //See device datasheet for TLV table memory mapping
#define CALADC12_12V_85C  *((unsigned int *)0x1A1C)   // Temperature Sensor Calibration-85 C

#include <FreeRTOS.h>
#include <task.h>
#include <driverlib.h>
#include <DataManager/SimpDB.h>
#include "SenseTemp.h"
#include "Semph.h"
#include <config.h>

int buffersize = 5;
int ptr;
int circled;
extern int waitTemp;
extern int ADCSemph;
extern int tempID;
extern uint64_t  information[10];
extern int readTemp;

void SenseLog(){
    while(1)
    {
        acquireSemph(pxCurrentTCB->uxTCBNumber);
        //Initialize for validity interval
        registerTCB(IDTEMP);

        unsigned int temp;
        volatile float temperatureDegC;
        volatile float temperatureDegF;

        // Initialize the shared reference module
        // By default, REFMSTR=1 => REFCTL is used to configure the internal reference
        while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
        REFCTL0 |= REFVSEL_0 + REFON;           // Enable internal 1.2V reference

        // Initialize ADC12_A
        ADC12CTL0 &= ~ADC12ENC;                 // Disable ADC12
        ADC12CTL0 = ADC12SHT0_8 + ADC12ON;      // Set sample time
        ADC12CTL1 = ADC12SHP;                   // Enable sample timer
        ADC12CTL3 = ADC12TCMAP;                 // Enable internal temperature sensor
        ADC12MCTL0 = ADC12VRSEL_1 + ADC12INCH_30; // ADC input ch A30 => temp sense
        ADC12IER0 = ADC12IE0;                      // ADC_IFG upon conv result-ADCMEMO

        while(!(REFCTL0 & REFGENRDY));          // Wait for reference generator
                                               // to settle
        ADC12CTL0 |= ADC12ENC;

        waitTemp = 1;
        ADC12CTL0 |= ADC12SC;               // Sampling and conversion start

        __bis_SR_register(GIE); //  interrupts enabled
        while(waitTemp);//wait for sampling, we don't want to mess up the scheduling here

        // Temperature in Celsius. See the Device Descriptor Table section in the
        // System Resets, Interrupts, and Operating Modes, System Control Module
        // chapter in the device user's guide for background information on the
        // used formula.
        temp = ADC12MEM0;
        temperatureDegC = (float)(((float)temp - CALADC12_12V_30C) * (85 - 30)) /
               (CALADC12_12V_85C - CALADC12_12V_30C) + 30.0f;

        // Temperature in Fahrenheit Tf = (9/5)*Tc + 32
        //        temperatureDegF = temperatureDegC * 9.0f / 5.0f + 32.0f;

        //write the result
        struct working data;
        DBworking(&data, 4, tempID);//2 bytes for int and for create(-1)
        float* tempadd = data.address;
        *tempadd = temp;
        tempID = DBcommit(&data,4,1);
        readTemp++;
        information[IDTEMP]++;
        unresgisterTCB(IDTEMP);
        releaseSemph(pxCurrentTCB->uxTCBNumber);
        portYIELD();
    }
}


