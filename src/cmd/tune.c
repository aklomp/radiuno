#include <stdlib.h>

#include <avr/pgmspace.h>

#include "../cmd.h"
#include "../uart.h"

// Forward declaration.
static struct cmd cmd;

static const char PROGMEM up[] = "up";
static const char PROGMEM dn[] = "down";

static void
on_help (void)
{
	uart_printf("%s [ %p | %p | <freq> ]\n", cmd.name, up, dn);
}

static bool
freq_set (struct cmd_state *state, const uint16_t freq)
{
	if (!si4735_freq_set(freq, false, false, state->band == CMD_BAND_SW))
		return false;

	// If the command was successful, wait for STCINT to become set,
	// to indicate that the chip has settled on a frequency.
	while (si4735_tune_status(&state->tune))
		if (state->tune.status.STCINT)
			return true;

	return false;
}

static bool
freq_nudge (struct cmd_state *state, bool up)
{
	uint16_t freq;

	if (state->tune.freq == 0)
		return false;

	freq = up ? state->tune.freq + 1
	          : state->tune.freq - 1;

	return freq_set(state, freq);
}

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	int freq;

	// Handle help function and insufficient args.
	if (args->ac < 2) {
		on_help();
		return false;
	}

	// Try to match any of the named arguments.
	if (!strncasecmp_P(args->av[1], up, sizeof (up)))
		return freq_nudge(state, true);

	if (!strncasecmp_P(args->av[1], dn, sizeof (dn)))
		return freq_nudge(state, false);

	// Primitive integer conversion.
	if ((freq = atoi(args->av[1])) <= 0)
		return false;

	return freq_set(state, freq);
}

static struct cmd cmd = {
	.name    = "tune",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
