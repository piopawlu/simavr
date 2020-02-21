/*
	atmega4313_usi.c
	
	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "attiny4313");

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define pcbi(port,pin) (port &= ~(1 << pin))
#define psbi(port,pin) (port |= (1 << pin))

#define _0(b) 0
#define _1(b) (1 << (b))

int8_t usi_spi_transmit_msb(uint8_t io)
{
	USIDR = io;

	#if F_CPU > 2000000 
		#define CLK_TOGGLE (_1(USIWM0) | _1(USICS1) | _0(USICS0) | _1(USITC) | _1(USICLK))

		USISR = _1(USIOIF);

		//MSB
		do {
			USICR = CLK_TOGGLE;
		} while( !(USISR & _1(USIOIF)) );
	#else 
		//FAST SPI, 1/2 CPU speed
		register uint8_t CLK_HI = _1(USIWM0) | _0(USICS0) | _1(USITC);
		register uint8_t CLK_LO = _1(USIWM0) | _0(USICS0) | _1(USITC) | _1(USICLK);

		//MSB
		USICR = CLK_HI; // 7
		USICR = CLK_LO;
		USICR = CLK_HI; // 6
		USICR = CLK_LO;
		USICR = CLK_HI; // 5
		USICR = CLK_LO;
		USICR = CLK_HI; // 4
		USICR = CLK_LO;
		USICR = CLK_HI; // 3
		USICR = CLK_LO;
		USICR = CLK_HI; // 2
		USICR = CLK_LO;
		USICR = CLK_HI; // 1
		USICR = CLK_LO;
		USICR = CLK_HI; // 0
		USICR = CLK_LO;
	#endif
	
	io = USIDR;
	return io;
}

int main()
{	
	#define CS_BIT PB4

	SCK_DDR |= _1(SCK_BIT);
	DO_DDR  |= _1(DO_BIT);
	DI_DDR  &= ~_1(DI_BIT);
	DDRB |= _1(CS_BIT);

	pcbi(SCK_PORT, SCK_BIT);
	pcbi(DO_PORT, DO_BIT);
	pcbi(DI_PORT, DI_BIT); // no pull up on data input
	psbi(PORTB,CS_BIT);

	_delay_ms(1000);

	while(1){
		pcbi(PORTB,CS_BIT);
		usi_spi_transmit_msb(0xAA);
		psbi(PORTB,CS_BIT);
		_delay_ms(1000);
	}
}

