#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "cmd.h"
#include "readline.h"
#include "si4735.h"
#include "uart.h"

int
main (void)
{
	// Enable interrupts:
	sei();

	// Initialize:
	uart_init();
	si4735_init();
	cmd_init();

	// Main loop:
	for (;;)
	{
		// Go to sleep, let interrupts wake us:
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_enable();
		sleep_cpu();
		sleep_disable();

		// Get a line of input and feed to command parser:
		cmd(readline());
	}

	return 0;
}
