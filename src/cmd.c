#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "cmd.h"
#include "uart.h"
#include "util.h"

// Forward declaration of the program banner symbols.
// Logo source:
//   http://patorjk.com/software/taag/#p=display&f=Doom&t=Radiuno
extern uint8_t _binary_src_banner_txt_start;
extern uint8_t _binary_src_banner_txt_end;

struct cmd *cmd_list = NULL;

static struct cmd_state state = {
	.band = CMD_BAND_NONE,
};

static volatile bool timer_tick;

ISR (TIMER0_OVF_vect)
{
	// Scale down timer a bit more, for a 1/3072 scaling:
	static uint8_t scale;

	if (++scale == 3) {
		scale = 0;
		timer_tick = true;
	}
}

void
cmd_print_help (const char *cmd, const void *map, const uint8_t count, const uint8_t stride)
{
	static const char PROGMEM p1[] = "%s [";
	static const char PROGMEM p2[] = "%s %p ";
	static const char PROGMEM p3[] = "]\n";

	uart_printf_P(p1, cmd);
	for (uint8_t i = 0; i < count; map += stride, i++)
		uart_printf_P(p2, i ? "|" : "", *(const char **) map);
	uart_printf_P(p3);
}

static bool
freq_set (const uint16_t freq)
{
	if (!si4735_freq_set(freq, false, false, state.band == CMD_BAND_SW))
		return false;

	// If the command was successful, wait for STCINT to become set,
	// indicating that the chip has settled on a frequency.
	while (si4735_tune_status(&state.tune))
		if (state.tune.status.STCINT)
			return true;

	return false;
}

static bool
freq_nudge (bool up)
{
	uint16_t freq;

	if (state.tune.freq == 0)
		return false;

	freq = (up) ? state.tune.freq + 1
	            : state.tune.freq - 1;

	return freq_set(freq);
}

__attribute__((used))
static bool
cmd_info (struct args *args, bool help)
{
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

	// Only valid in powerup state:
	if (state.band == CMD_BAND_NONE)
		return false;

	if (help) {
		uart_printf("%s\n", args->av[0]);
		return true;
	}

	// Get tune status:
	if (!si4735_tune_status(&state.tune))
		return false;

	// Print generic info.
	uart_printf_P(str,
		state.tune.flags & (1 << 7) ? "[Band limit] " : "",
		state.tune.flags & (1 << 1) ? "[AFC rail] " : "",
		state.tune.flags & (1 << 0) ? "[Valid]" : "",
		state.tune.freq, state.tune.rssi, state.tune.snr);

	// Print band-specific info.
	if (state.band == CMD_BAND_FM) {
		uart_printf_P(str_fm, state.tune.fm.mult, state.tune.fm.readantcap);
	} else {
		uart_printf_P(str_am, state.tune.am.readantcap);
	}

	return true;
}

static void
seek_status (void)
{
	bool first = true;

	// Setup a timer interrupt to periodically update the console:
	TCCR0A = 0;
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 = _BV(TOIE0);

	// Clear End-of-Text flag (Ctrl-C) by reading it:
	uart_flag_etx();

	// Loop until we found a station:
	for (;;)
	{
		// Sleep until a timer tick occurs:
		while (!timer_tick) {
			sleep_enable();
			sleep_cpu();
			sleep_disable();
		}

		// Acknowledge timer tick:
		timer_tick = false;

		// Get tuning status:
		if (!si4735_tune_status(&state.tune))
			continue;

		// Print current frequency:
		if (!first)
			uart_putc('\r');
		else
			first = false;

		uart_printf("%u ", state.tune.freq);

		// Quit on End-of-Text (Ctrl-C):
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
			while (!state.tune.status.STCINT)
				if (!si4735_tune_status(&state.tune))
					break;

			uart_printf_P(fmt_success);
			break;
		}

		// If STCINT flag is set, seek has finished:
		if (state.tune.status.STCINT) {

			// Check if we found a valid station:
			if (state.tune.flags & 0x01) {
				static const char PROGMEM fmt[] =
					"\rValid station: %u, rssi: %u, snr: %u\n";
				uart_printf_P(fmt, state.tune.freq, state.tune.rssi, state.tune.snr);
			}

			// Check if we wrapped:
			if (state.tune.flags & 0x80) {
				static const char PROGMEM fmt[] =
					"\rWrapped: %u, rssi: %u, snr: %u\n";
				uart_printf_P(fmt, state.tune.freq, state.tune.rssi, state.tune.snr);
			}

			break;
		}
	}

	// Disable interrupt:
	TIMSK0 = 0;
}

static const char PROGMEM up[] = "up";
static const char PROGMEM dn[] = "down";

static bool
cmd_seek (struct args *args, bool help)
{
	static const struct {
		const char	*cmd;
		uint8_t		 len;
		bool		 up;
	}
	map[] = {
		{ up, sizeof(up), true  },
		{ dn, sizeof(dn), false },
	};

	// Only valid in powerup mode:
	if (state.band == CMD_BAND_NONE)
		return false;

	// Handle help function and insufficient args:
	if (help || args->ac < 2) {
		cmd_print_help(args->av[0], map, NELEM(map), STRIDE(map));
		return help;
	}

	// Handle subcommands.
	FOREACH (map, m) {
		if (strncasecmp_P(args->av[1], m->cmd, m->len))
			continue;

		if (!si4735_seek_start(m->up, true, state.band == CMD_BAND_SW))
			return false;

		seek_status();
		return true;
	}

	return false;
}

__attribute__((used))
static bool
cmd_tune (struct args *args, bool help)
{
	int freq;

	// Only valid in powerup mode:
	if (state.band == CMD_BAND_NONE)
		return false;

	// Handle help function and insufficient args:
	if (help || args->ac < 2) {
		uart_printf("%s [ %p | %p | <freq> ]\n", args->av[0], up, dn);
		return help;
	}

	// Check if we can match any of the named arguments:
	if (!strncasecmp_P(args->av[1], up, sizeof(up)))
		return freq_nudge(true);

	if (!strncasecmp_P(args->av[1], dn, sizeof(dn)))
		return freq_nudge(false);

	// Primitive conversion:
	if ((freq = atoi(args->av[1])) <= 0)
		return false;

	return freq_set(freq);
}

void
cmd_link (struct cmd *cmd)
{
	struct cmd *c;

	// If this is the first command to register itself, or if it sorts
	// below the current list head, then insert it at the front.
	if (cmd_list == NULL || strcasecmp(cmd->name, cmd_list->name) < 0) {
		cmd->next = cmd_list;
		cmd_list  = cmd;
		return;
	}

	// Walk the linked list to find either the last element, or the element
	// whose next element sorts above this command. This yields the element
	// after which the command should be inserted.
	for (c = cmd_list; c->next != NULL; c = c->next)
		if (strcasecmp(c->next->name, cmd->name) > 0)
			break;

	// Insert the given command structure at this location.
	cmd->next = c->next;
	c->next   = cmd;
}

static void
prompt (void)
{
	static const char PROGMEM prompt_none[][6] = {
		[CMD_BAND_FM]   = "fm > ",
		[CMD_BAND_AM]   = "am > ",
		[CMD_BAND_SW]   = "sw > ",
		[CMD_BAND_LW]   = "lw > ",
		[CMD_BAND_NONE] = "-- > ",
	};

	static const char PROGMEM prompt_freq[][9] = {
		[CMD_BAND_FM] = "fm %u > ",
		[CMD_BAND_AM] = "am %u > ",
		[CMD_BAND_SW] = "sw %u > ",
		[CMD_BAND_LW] = "lw %u > ",
	};

	(si4735_tune_status(&state.tune) && state.tune.freq)
		? uart_printf_P(prompt_freq[state.band], state.tune.freq)
		: uart_printf_P(prompt_none[state.band]);
}

static bool
dispatch_cmd (const struct args *args)
{
	static const char PROGMEM failed[]  = "%s: failed\n";
	static const char PROGMEM unknown[] = "%s: unknown command\n";

	// Allow empty lines.
	if (!args->ac)
		return true;

	// Find the command in the linked list and call it.
	for (const struct cmd *c = cmd_list; c; c = c->next) {
		if (strcasecmp(args->av[0], c->name))
			continue;

		if (c->on_call(args, &state))
			return true;

		uart_printf_P(failed, args->av[0]);
		return false;
	}

	// Print an error message if the command was not found.
	uart_printf_P(unknown, args->av[0]);
	return false;
}

// Parse a command line.
bool
cmd_exec (const struct args *args)
{
	// Start on a new line.
	uart_printf("\n");

	// Dispatch the command.
	const bool ret = dispatch_cmd(args);

	// Return to the prompt.
	prompt();
	return ret;
}

static void
banner (void)
{
	// Print all bytes of the banner, converting \n to \r\n on the fly.
	for (uint8_t c, i = 0; i < &_binary_src_banner_txt_end - &_binary_src_banner_txt_start; i++) {
		if ((c = pgm_read_byte(&_binary_src_banner_txt_start + i)) == '\n')
			uart_putc('\r');

		uart_putc(c);
	}
}

void
cmd_init (void)
{
	banner();

	if (cmd_exec(&(struct args) { .ac = 2, .av = { "mode", "fm" } }))
	       cmd_seek(&(struct args) { .ac = 2, .av = { NULL, "up" } }, false);

	prompt();
}
