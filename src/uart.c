#include <stdbool.h>
#include <stdarg.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "uart.h"

#define BAUDRATE	115200
#define BAUD_PRESCALE	((F_CPU / (BAUDRATE * 8UL)) - 1)

struct uart_fifo rx, tx;
static bool flag_etx = false;

ISR (USART_UDRE_vect, ISR_BLOCK)
{
	// If there is no more data in the ring buffer, disable this interrupt:
	if (tx.head == tx.tail) {
		UCSR0B &= ~_BV(UDRIE0);
		return;
	}

	// Else feed data to the buffer:
	UDR0 = tx.fifo[tx.tail];
	tx.tail = (tx.tail + 1) % UART_FIFOSIZE;
}

ISR (USART_RX_vect, ISR_BLOCK)
{
	// If we have a frame error, read the register and ignore:
	if (UCSR0A & _BV(FE0)) {
		volatile uint8_t unused __attribute__((unused)) = UDR0;
		return;
	}

	// Get character:
	uint8_t ch = UDR0;

	// If it's an End-of-Text (Ctrl-C), handle out of band by setting flag:
	if (ch == 0x03) {
		flag_etx = true;
		return;
	}

	// Put character into Rx FIFO:
	uint8_t next = (rx.head + 1) % UART_FIFOSIZE;
	rx.fifo[rx.head] = ch;
	if (next != rx.tail)
		rx.head = next;
}

bool
uart_flag_etx (void)
{
	// Get and reset End-of-Text (Ctrl-C) flag:
	if (flag_etx) {
		flag_etx = false;
		return true;
	}
	return false;
}

void
uart_putc (const uint8_t c)
{
	uint8_t next = (tx.head + 1) % UART_FIFOSIZE;

	// If the next character would coincide with the fifo's tail,
	// wait for a character to be transmitted first:
	if (next == tx.tail)
		while (!(UCSR0A & _BV(UDRE0)))
			continue;

	tx.fifo[tx.head] = c;
	tx.head = next;

	// Enable UDR0 Empty interrupt:
	UCSR0B |= _BV(UDRIE0);
}

// Printf with a RAM-based format string
static void
do_printf (bool ram, const char *restrict format, va_list argp)
{
	static const char PROGMEM hex[] = "0123456789ABCDEF";
	char c;

	while ((c = (ram) ? *format++ : pgm_read_byte(format++)) != 0)
	{
		switch (c)
		{
		case '%':
			switch ((ram) ? *format++ : pgm_read_byte(format++)) {
			case 0:
				break;

			case '%':
				uart_putc('%');
				break;

			case 'c':
				uart_putc(va_arg(argp, int));
				break;

			case 'd': {
				uint16_t div;
				int16_t d = va_arg(argp, int16_t);
				if (d < 0) {
					uart_putc('-');
					d = -d;
				}
				if (d == 0) {
					uart_putc('0');
					break;
				}
				for (div = 1; div * 10 <= d; div *= 10)
					continue;

				while (div) {
					uint8_t digit = d / div;
					d -= div * digit;
					div /= 10;
					uart_putc('0' + digit);
				}
				break;
			}

			case 's': {
				const char *s = va_arg(argp, char *);
				while (*s)
					uart_putc(*s++);
				break;
			}

			case 'p': {
				const char *s = va_arg(argp, char *);
				while (pgm_read_byte(s))
					uart_putc(pgm_read_byte(s++));
				break;
			}

			case 'u': {
				uint16_t div, u = va_arg(argp, uint16_t);
				if (u == 0) {
					uart_putc('0');
					break;
				}
				for (div = 1; div * 10 <= u; div *= 10)
					continue;

				while (div) {
					uint8_t digit = u / div;
					u -= div * digit;
					div /= 10;
					uart_putc('0' + digit);
				}
				break;
			}

			case 'x': {
				uint16_t div, x = va_arg(argp, uint16_t);
				if (x == 0) {
					uart_putc('0');
					break;
				}
				for (div = 1; div * 16 <= x; div *= 16)
					continue;

				while (div) {
					uint8_t digit = x / div;
					x -= div * digit;
					div /= 16;
					uart_putc(pgm_read_byte(hex + digit));
				}
				break;
			}
			}
			break;

		case '\n':
			uart_putc('\r');
			// Fallthrough

		default:
			uart_putc(c);
			break;
		}
	}
}

// Printf with a RAM-based format string
void
uart_printf (const char *restrict format, ...)
{
	va_list argp;

	if (!format)
		return;

	va_start(argp, format);
	do_printf(true, format, argp);
	va_end(argp);
}

// Printf with a PROGMEM-based format string
void
uart_printf_P (const char *restrict format, ...)
{
	va_list argp;

	if (!format)
		return;

	va_start(argp, format);
	do_printf(false, format, argp);
	va_end(argp);
}

void
uart_init (void)
{
	// Wake up USART0:
	PRR &= ~_BV(PRUSART0);

	// Set double speed mode:
	UCSR0A = _BV(U2X0);

	// Set baudrate:
	UBRR0H = BAUD_PRESCALE >> 8;
	UBRR0L = BAUD_PRESCALE & 0xFF;

	// Set frame format to 8N1:
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	// Start rx, tx, and enable rx interrupt:
	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}
