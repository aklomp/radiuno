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

// Sleep until a timer tick occurs.
static void
wait_for_tick (void)
{
	for (;;) {

		// Check atomically if the flag is set.
		cli();
		if (timer_tick) {
			timer_tick = false;
			sei();
			return;
		}

		// Sleep until woken by an interrupt.
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_enable();
		sei();
		sleep_cpu();
		sleep_disable();
	}
}

static bool
cancel_seek (struct cmd_state *state)
{
	static const char PROGMEM fmt_success[] = "\r\n";
	static const char PROGMEM fmt_failure[] = "\rSeek: failed to cancel.\n";

	// Cancel the seek.
	if (!si4735_seek_cancel()) {
		uart_printf_P(fmt_failure);
		return false;
	}

	// Wait for STCINT to become set, indicating that the chip stopped
	// seeking and has settled on a station.
	while (!state->tune.status.STCINT)
		if (!si4735_tune_status(&state->tune))
			break;

	uart_printf_P(fmt_success);
	return true;
}

static void
finish_seek (struct cmd_state *state)
{
	// Check if a valid station was found.
	if (state->tune.VALID) {
		static const char PROGMEM fmt[] =
			"\rValid station: %u, rssi: %u, snr: %u\n";

		uart_printf_P(fmt,
			state->tune.freq,
			state->tune.rssi,
			state->tune.snr);
	}

	// Check if we wrapped.
	if (state->tune.BLTF) {
		static const char PROGMEM fmt[] =
			"\rWrapped: %u, rssi: %u, snr: %u\n";

		uart_printf_P(fmt,
			state->tune.freq,
			state->tune.rssi,
			state->tune.snr);
	}
}

static void
seek_status (struct cmd_state *state)
{
	// Setup a timer interrupt to periodically update the console.
	TCCR0A = 0;
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 = _BV(TOIE0);

	// Clear the End-of-Text flag (Ctrl-C) by reading it.
	uart_flag_etx();

	// Loop until a station is found.
	for (;;) {

		// Sleep until a timer tick occurs.
		wait_for_tick();

		// Get tuning status.
		if (!si4735_tune_status(&state->tune))
			continue;

		// Print current frequency.
		uart_printf("\r%u ", state->tune.freq);

		// Quit on End-of-Text (Ctrl-C).
		if (uart_flag_etx())
			if (cancel_seek(state))
				break;

		// If the STCINT flag is set, the seek has finished.
		if (state->tune.status.STCINT) {
			finish_seek(state);
			break;
		}
	}

	// Disable interrupt.
	TIMSK0 &= ~_BV(TOIE0);
}

static bool
dispatch (const struct args *args, struct cmd_state *state)
{
	// A subcommand is required.
	if (args->ac < 2)
		return false;

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

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	if (state->band == CMD_BAND_NONE)
		return false;

	if (dispatch(args, state))
		return true;

	on_help();
	return false;
}

static struct cmd cmd = {
	.name    = "seek",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
