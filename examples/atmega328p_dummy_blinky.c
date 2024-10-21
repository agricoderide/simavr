/*
	atmega328p_dummy_blinky.c

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

#ifdef F_CPU
#undef F_CPU
#endif

#define F_CPU 16000000

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <util/delay.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator, No need for the real arduino
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega328p");

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
										 _FDEV_SETUP_WRITE);

int main()
{
	stdout = &mystdout;

	printf("Bootloader properly programmed, and ran me! Huzzah!\n");

	// this quits the simulator, since interrupts are off
	// this is a "feature" that allows running tests cases and exit
	// sleep_cpu();

	// stdout = &mystdout;

	// Set up the UART (just for printing messages)
	UBRR0H = 0;
	UBRR0L = 103;							// Baud rate 9600 with 16MHz clock
	UCSR0B = (1 << TXEN0);					// Enable TX
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data

	// Set up the LED on pin PB5 (Arduino's built-in LED on pin 13)
	DDRB |= (1 << PB5); // Set PB5 as output

	while (1)
	{
		// Toggle the LED
		PORTB ^= (1 << PB5);

		// Print a message over UART
		printf("LED state changed!\n");

		// Delay for a second
		_delay_ms(1000);
	}

	return 0;
}
