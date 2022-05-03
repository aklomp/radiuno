#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "../cmd.h"
#include "../uart.h"
#include "../util.h"

// Forward declaration.
static struct cmd cmd;

static const char PROGMEM up[] = "up";
static const char PROGMEM dn[] = "down";

// Subcommand map.
static const struct {
	const char *cmd;
	uint8_t     len;
	bool        up;
}
map[] = {
	{ up, sizeof (up), true  },
	{ dn, sizeof (dn), false },
};

static volatile bool timer_tick;

static void
on_help (void)
{
	cmd_print_help(cmd.name, map, NELEM(map), STRIDE(map));
}

ISR (TIMER0_OVF_vect)
{
	// Scale down timer a bit more, for a 1/3072 scaling.
	static uint8_t scale;

	if (++scale == 3) {
		scale = 0;
		timer_tick = true;
	}
}

static void
seek_status (struct cmd_state *state)
{
	bool first = true;

	// Setup a timer interrupt to periodically update the console.
	TCCR0A = 0;
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 = _BV(TOIE0);

	// Clear End-of-Text flag (Ctrl-C) by reading it.
	uart_flag_etx();

	// Loop until we found a station.
	for (;;) {

		// Sleep until a timer tick occurs.
		while (!timer_tick) {
			sleep_enable();
			sleep_cpu();
			sleep_disable();
		}

		// Acknowledge the timer tick.
		timer_tick = false;

		// Get tuning status.
		if (!si4735_tune_status(&state->tune))
			continue;

		// Print current frequency.
		if (!first)
			uart_putc('\r');
		else
			first = false;

		uart_printf("%u ", state->tune.freq);

		// Quit on End-of-Text (Ctrl-C).
		if (uart_flag_etx()) {
			static const char PROGMEM fmt_success[] = "\r\n";
			static const char PROGMEM fmt_failure[] = "\rSeek: failed to cancel.\n";

			// Cancel the seek.
			if (!si4735_seek_cancel()) {
				uart_printf_P(fmt_failure);
				continue;
			}

			// Wait for STCINT to become set, indicating that the
			// chip stopped seeking and has settled on a station.
			while (!state->tune.status.STCINT)
				if (!si4735_tune_status(&state->tune))
					break;

			uart_printf_P(fmt_success);
			break;
		}

		// If STCINT flag is set, seek has finished.
		if (state->tune.status.STCINT) {

			// Check if we found a valid station.
			if (state->tune.flags.VALID) {
				static const char PROGMEM fmt[] =
					"\rValid station: %u, rssi: %u, snr: %u\n";

				uart_printf_P(fmt,
					state->tune.freq,
					state->tune.rssi,
					state->tune.snr);
			}

			// Check if we wrapped.
			if (state->tune.flags.BLTF) {
				static const char PROGMEM fmt[] =
					"\rWrapped: %u, rssi: %u, snr: %u\n";

				uart_printf_P(fmt,
					state->tune.freq,
					state->tune.rssi,
					state->tune.snr);
			}

			break;
		}
	}

	// Disable interrupt.
	TIMSK0 = 0;
}

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	// Only valid in powerup mode.
	if (state->band == CMD_BAND_NONE)
		return false;

	// Handle insufficient arguments.
	if (args->ac < 2) {
		on_help();
		return false;
	}

	// Handle subcommands.
	FOREACH (map, m) {
		if (strncasecmp_P(args->av[1], m->cmd, m->len))
			continue;

		if (!si4735_seek_start(m->up, true, state->band == CMD_BAND_SW))
			return false;

		seek_status(state);
		return true;
	}

	return false;
}

static struct cmd cmd = {
	.name    = "seek",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
