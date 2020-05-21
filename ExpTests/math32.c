/*
 * math32.h
 *
 *  Created on: 2018¦~3¤ë19¤é
 *      Author: Meenchen
 */

#include <config.h>
extern unsigned long timeCounter;
extern unsigned long taskRecency[12];
extern unsigned long information[10];

/*******************************************************************************
*
* Name : 32-bit Math
* Purpose : Benchmark 32-bit math functions.
*
*******************************************************************************/
#include <math.h>
typedef unsigned long UInt32;
UInt32 add(UInt32 a, UInt32 b)
{
    return (a + b);
}
UInt32 mul(UInt32 a, UInt32 b)
{
    return (a * b);
}
UInt32 div(UInt32 a, UInt32 b)
{
    return (a / b);
}

#pragma NOINIT(Fresult32)
volatile UInt32 Fresult32[4];
volatile UInt32 Sresult32[4];

void math32()
{
    int i;

    while(1){
        for(i = 0; i < ITERMATH32; i++){
            Sresult32[0] = 43125;
            Sresult32[1] = 14567;
            Sresult32[2] = Sresult32[0] + Sresult32[1];
            Sresult32[1] = Sresult32[0] * Sresult32[2];
            Fresult32[3] = Sresult32[1] / Sresult32[2];
        }
        information[IDMATH32]++;
    }
}

