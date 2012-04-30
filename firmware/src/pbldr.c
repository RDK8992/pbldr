/*
 * Copyright 2012 Eric Evenchick
 *
 * This file is part of pbldr.
 *
 * pbldr is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * pbldr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with pbldr If not, see http://www.gnu.org/licenses/.
 */

#include <p18f26k80.h>

// Configuration Bits
#pragma config XINST = OFF	// disable extended instructions
#pragma config FOSC = XT	// using XTal oscillator
#pragma config PLLCFG = OFF	// disable 4x pll
#pragma config FCMEN = OFF	// disable fail-safe clock monitor
#pragma config IESO = OFF	// disable internal oscillator switch over
#pragma config PWRTEN = OFF	// disable power up timer
#pragma config BOREN = OFF	// disable brown out protect
#pragma config BORV = 2		// set brownout threshold at 2V
#pragma config BORPWR = MEDIUM	// set BORMV to medium power level
#pragma config WDTEN = OFF	// disable watchdog
#pragma config CANMX = PORTB	// use port b pins for CAN
#pragma config MCLRE = ON	// enable MCLR (needed for debug)
#pragma config CPB = OFF	// disable boot code protect
#pragma config CPD = OFF	// disable ee read protect
#pragma config CP1 = OFF	// disable code protect
#pragma config CP2 = OFF
#pragma config CP3 = OFF
#pragma config WRT1 = OFF	// disable table write protect
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF

#define FCY 8000000
#define FLASH_ADDR 0x800
#define BOOT_ADDR 0x820
#define REMAP_HIGH_INTERRUPT 0x808
#define REMAP_LOW_INTERRUPT 0x818

// choose protocol here
#define USE_UART

/************************
 Program Memory Functions
*************************/
void FlashErase(long addr)
{

    TBLPTR = addr;

    EECON1bits.EEPGD = 1;	// select program memory
    EECON1bits.CFGS = 0;	// enable program memory access
    EECON1bits.WREN = 1;	// enable write access
    EECON1bits.FREE = 1;	// enable the erase

    INTCONbits.GIE = 0;		// disable interrupts

    // erase sequence
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;

    INTCONbits.GIE = 1;		// enable interrupts
}

void FlashWrite(long addr, char *data)
{
    int i;

    FlashErase(addr);		// must erase flash before writing

    TBLPTR = addr;

    // load the table latch with data
    for (i = 0; i < 64; i++)
    {
	TABLAT = data[i];	// copy data from buffer
	_asm
	    TBLWTPOSTINC	// increment the table latch
	_endasm
    }

    TBLPTR = addr;

    EECON1bits.EEPGD = 1;	// select program memory
    EECON1bits.CFGS = 0;	// enable program memory access
    EECON1bits.WREN = 1;	// enable write access

    INTCONbits.GIE = 0;		// disable interrupts

    // write sequence
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;

    INTCONbits.GIE = 1;		// enable interrupts

}

#ifdef USE_UART
#include "uart.h"

void main()
{
    int i;
	char high, low, res;
    long cur_addr = FLASH_ADDR;
    char buf[64];
    char done = 0;

    UART1Init(115200);

    // wait for request to load code
    if (UART1RxByte() == -1)
	_asm goto BOOT_ADDR _endasm	// no request, jump to program


    UART1TxROMString("OK\n");
    for (;;)
    {
	for (i = 0; i < 64; i++)
	{
		high = a2h(UART1RxByte());
		low = a2h(UART1RxByte());
	    if (high == -1 || low == -1)
	    {
			done = 1;
			break;
	    }
		res = (high<<4) + low;
		buf[i] = res;
	}
	FlashWrite(cur_addr, buf);
	cur_addr += 64;
	UART1TxByte('K');
	if (done)
	    break;
    }

    UART1TxROMString("DONE\n");
    _asm goto BOOT_ADDR _endasm
}

#endif

// interrupt remapping
#pragma code high_vector=0x08
void high_isr()
{
	_asm goto REMAP_HIGH_INTERRUPT _endasm
}
#pragma code low_vector=0x18
void low_isr()
{
	_asm goto REMAP_LOW_INTERRUPT _endasm
}
