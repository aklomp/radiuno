#include <avr/interrupt.h>

#include "cmd.h"
#include "readline.h"
#include "si4735.h"
#include "uart.h"

int
main (void)
{
	// Enable interrupts.
	sei();

	// Initialize.
	uart_init();
	si4735_init();
	cmd_init();

	// Main loop.
	for (;;)
	{
		// Get a line of input and feed it to the command parser.
		cmd(readline());
	}

	return 0;
}
