/*
 *
 * Engineer: Bryce Feigum (bafeigum at gmail)
 * Date: December 22, 2012
 *
 * Version: 1.0 - Initial Release
 * Version: 1.1 - Added GNUv2 License
 *
 * Copyright (C) 2013  Bryce Feigum
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA.
 * Project: DHT11_LIB
 * File:    DHT11_LIB.c
 */

#include <FreeRTOS.h>
#include <dht11.h>
#include <driverlib.h>
#include <msp430.h>
#include <myuart.h>
#include <task.h>

#include "config.h"

extern unsigned char volatile TOUT;

unsigned char read_Byte() {
    unsigned char num = 0;
    for (int i = 8; i > 0; i--) {
        while (!(TST(P2IN, DPIN)))
            ;                 // Wait for signal to go high
        __delay_cycles(30 * CYCLE_PER_US);  // 30us * 16
        if (TST(P2IN, DPIN)) {
            num |= 1 << (i - 1);
            while((TST(P2IN,DPIN))); //Wait for signal to go low
        }
    }
    return num;
}

unsigned char read_Packet(unsigned char *data) {
    start_Signal();
    data[0] = read_Byte();
    data[1] = read_Byte();
    data[2] = read_Byte();
    data[3] = read_Byte();
    data[4] = read_Byte();

    return 1;
}

void start_Signal() {
    SET(P2DIR, DPIN);                    // Set Data pin to output direction
    SET(P2OUT, DPIN);                    // set high
    __delay_cycles(100 * CYCLE_PER_US);  // High for at 100us
    CLR(P2OUT, DPIN);                    // Set output to low
    __delay_cycles(288000);              // Low for at least 18ms
    SET(P2OUT, DPIN);
    __delay_cycles(30 * CYCLE_PER_US);  // High for at 20us-40us
    // CLR(P2DIR,DPIN);     // Set data pin to input direction
    GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN5);
    while ((TST(P2IN, DPIN)))
        ;  // Wait for signal to go low
    while (!(TST(P2IN, DPIN)))
        ;  // Wait for signal to go high
    while ((TST(P2IN, DPIN)))
        ;  // Wait for signal to go low
}

// returns true if checksum is correct
unsigned char check_Checksum(unsigned char *data) {
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) {
        return 0;
    } else
        return 1;
}

void setup_MSP430_DHT11(void) {
    GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P2, GPIO_PIN5);
}
