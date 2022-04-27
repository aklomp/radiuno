#include <avr/interrupt.h>

#include "args.h"
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
	for (;;) {
		struct args args;

		// Get a line of input, parse it into arguments, and feed it to
		// the command executer.
		cmd_exec(args_parse(readline(), &args));
	}

	return 0;
}
