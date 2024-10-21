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
#include <avr/power.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega328p");

#ifdef F_CPU
#undef F_CPU
#endif
#define F_CPU 16000000

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

// Timer1 interrupt for toggling LED on PB5
ISR(TIMER1_COMPA_vect)
{
	PORTB ^= (1 << PB5); // Toggle PB5 (Arduino pin 13)
}

void setup_timer1()
{
	// Configure Timer1 to toggle LED every 1 second
	TCCR1A = 0; // Clear Timer/Counter Control Registers
	TCCR1B = 0;
	TCNT1 = 0;	   // Reset Timer Counter
	OCR1A = 15624; // Set Output Compare value for 1 Hz

	TCCR1B |= (1 << WGM12);				 // CTC mode
	TCCR1B |= (1 << CS12) | (1 << CS10); // Set prescaler to 1024
	TIMSK1 |= (1 << OCIE1A);			 // Enable Output Compare A Match Interrupt
}

int main()
{
	stdout = &mystdout;

	// Set up UART for printing
	UBRR0H = 0;
	UBRR0L = 103;							// Baud rate 9600 with 16MHz clock
	UCSR0B = (1 << TXEN0);					// Enable TX
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data

	// Set up the LED on pin PB5 (Arduino's built-in LED on pin 13)
	DDRB |= (1 << PB5); // Set PB5 as output

	// Setup Timer1 for interrupt-based LED toggling
	setup_timer1();

	// Enable global interrupts
	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

	while (1)
	{
		// Print a message over UART every second (UART is not using the timer)
		printf("LED toggled via Timer1!\n");
		sleep_mode();
		// Delay to prevent flooding the UART (this delay is not controlling the LED)
		// _delay_ms(1000);
	}

	return 0;
}