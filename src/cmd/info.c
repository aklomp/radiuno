#include <avr/pgmspace.h>

#include "../cmd.h"
#include "../uart.h"

// Forward declaration.
static struct cmd cmd;

static const char PROGMEM str[] =
	"flags      : %s%s%s\n"
	"freq       : %u\n"
	"rssi       : %d\n"
	"snr        : %u\n";

static const char PROGMEM str_fm[] =
	"multipath  : %u\n"
	"readantcap : %u\n";

static const char PROGMEM str_am[] =
	"readantcap : %u\n";

static void
on_help (void)
{
	uart_printf("%s\n", cmd.name);
}

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	// Only valid in powerup state.
	if (state->band == CMD_BAND_NONE)
		return false;

	// Get tune status.
	if (!si4735_tune_status(&state->tune))
		return false;

	// Print generic info.
	uart_printf_P(str,
		state->tune.flags & (1 << 7) ? "[Band limit] " : "",
		state->tune.flags & (1 << 1) ? "[AFC rail] " : "",
		state->tune.flags & (1 << 0) ? "[Valid]" : "",
		state->tune.freq, state->tune.rssi, state->tune.snr);

	// Print band-specific info.
	if (state->band == CMD_BAND_FM) {
		uart_printf_P(str_fm, state->tune.fm.mult, state->tune.fm.readantcap);
	} else {
		uart_printf_P(str_am, state->tune.am.readantcap);
	}

	return true;
}

static struct cmd cmd = {
	.name    = "info",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
