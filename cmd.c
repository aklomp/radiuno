#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "si4735.h"
#include "uart.h"

#define STRIDE(n)	(sizeof((n)[0]))
#define COUNT(n)	(sizeof(n) / STRIDE(n))

// Current chip mode:
enum mode {
	MODE_FM,
	MODE_AM,
	MODE_SW,
	MODE_DOWN,
};

static struct {
	enum mode			mode;
	struct si4735_tune_status	tune;
	bool				timer_tick;
}
state = {
	.mode = MODE_DOWN,
};

ISR (TIMER0_OVF_vect)
{
	// Scale down timer a bit more, for a 1/3072 scaling:
	static uint8_t scale;

	if (++scale == 3) {
		scale = 0;
		state.timer_tick = true;
	}
}

static const char *
skip_whitespace (const char *line)
{
	while (*line && (*line == ' ' || *line == '\t'))
		line++;

	return line;
}

static void
print_help (const char *cmd, const void *map, uint8_t count, uint8_t stride)
{
	static const char PROGMEM p1[] = "%s [";
	static const char PROGMEM p2[] = "%s %p ";
	static const char PROGMEM p3[] = "]\n";

	uart_printf_P(p1, cmd);
	for (uint8_t i = 0; i < count; map += stride, i++)
		uart_printf_P(p2, i ? "|" : "", *(const char **)map);
	uart_printf_P(p3);
}

static void
print_revision (void)
{
	struct si4735_rev rev;
	static const char PROGMEM fmt[] =
		"part number        : si47%u\n"
		"chip revision      : %c\n"
		"component revision : %c.%c\n"
		"firmware           : %c.%c\n"
		"patch id           : %u\n";

	if (si4735_rev_get(&rev) == false)
		return;

	uart_printf_P(fmt,
		rev.part_number,
		rev.chip_rev,
		rev.cmpmajor, rev.cmpminor,
		rev.fwmajor, rev.fwminor,
		rev.patch_id);
}

static bool
power_up (const enum mode mode)
{
	switch (mode) {
	case MODE_FM: return si4735_fm_power_up();
	case MODE_AM: return si4735_am_power_up();
	case MODE_SW: return si4735_sw_power_up();
	default     : return false;
	};
}

static bool
seek_start (const bool up, const bool wrap)
{
	switch (state.mode) {
	case MODE_FM: return si4735_fm_seek_start(up, wrap);
	case MODE_AM: return si4735_am_seek_start(up, wrap);
	case MODE_SW: return si4735_sw_seek_start(up, wrap);
	default     : return false;
	};
}

static bool
tune_status (struct si4735_tune_status *tune)
{
	switch (state.mode) {
	case MODE_FM: return si4735_fm_tune_status(tune);
	case MODE_AM: return si4735_am_tune_status(tune);
	case MODE_SW: return si4735_sw_tune_status(tune);
	default     : return false;
	};
}

static bool
cmd_mode (const char *cmd, const char *line, bool help)
{
	static const char PROGMEM sub[][3] = { "fm", "am", "sw" };

	static const struct {
		const char	*cmd;
		enum mode	 mode;
	}
	map[] = {
		{ sub[0], MODE_FM },
		{ sub[1], MODE_AM },
		{ sub[2], MODE_SW },
	};

	// Handle subcommands:
	for (uint8_t i = 0; !help && i < COUNT(map); i++) {
		if (strncasecmp_P(line, map[i].cmd, 2))
			continue;

		// If already in desired mode, ignore:
		if (state.mode == map[i].mode)
			return true;

		// Power down the chip if not already down:
		if (state.mode != MODE_DOWN)
			if (!si4735_power_down())
				return false;

		// Power up the chip in new mode:
		if (!power_up(map[i].mode))
			return false;

		// Print chip revision data:
		print_revision();

		// For SW, set non-default band limits:
		if (map[i].mode == MODE_SW) {
			si4735_prop_set(0x3400, 1711);
			si4735_prop_set(0x3401, 27000);
		}

		state.tune.freq = 0;
		state.mode = map[i].mode;
		return true;
	}

	// Fallthrough: help function:
	print_help(cmd, map, COUNT(map), STRIDE(map));
	return help;
}

static bool
cmd_info (const char *cmd, const char *line, bool help)
{
	static const char PROGMEM str[] =
		"flags : %x\n"
		"freq  : %u\n"
		"rssi  : %d\n"
		"snr   : %u\n";

	// Only valid in powerup state:
	if (state.mode == MODE_DOWN)
		return false;

	if (help) {
		uart_printf("%s\n", cmd);
		return true;
	}

	// Get tune status:
	if (!tune_status(&state.tune))
		return false;

	// Print:
	uart_printf_P(str, state.tune.flags, state.tune.freq, state.tune.rssi, state.tune.snr);
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

	// Loop until we found a station:
	for (;;)
	{
		// Sleep until a timer tick occurs:
		while (!state.timer_tick) {
			sleep_enable();
			sleep_cpu();
			sleep_disable();
		}

		// Acknowledge timer tick:
		state.timer_tick = false;

		// Get tuning status:
		if (!tune_status(&state.tune))
			continue;

		// Print current frequency:
		if (!first)
			uart_putc('\r');
		else
			first = false;

		uart_printf("%u ", state.tune.freq);

		// If STCINT flag is not set, seek has not finished:
		if (!(state.tune.status & 0x01))
			continue;

		// Exit loop if we found a valid station:
		if (state.tune.flags & 0x01) {
			static const char PROGMEM fmt[] =
				"\rValid station: %u, rssi: %u, snr: %u\n";
			uart_printf_P(fmt, state.tune.freq, state.tune.rssi, state.tune.snr);
		}

		// Exit loop if we wrapped:
		if (state.tune.flags & 0x80) {
			static const char PROGMEM fmt[] =
				"\rWrapped: %u, rssi: %u, snr: %u\n";
			uart_printf_P(fmt, state.tune.freq, state.tune.rssi, state.tune.snr);
		}

		break;
	}

	// Disable interrupt:
	TIMSK0 = 0;
}

static bool
cmd_seek (const char *cmd, const char *line, bool help)
{
	static const char PROGMEM up[] = "up";
	static const char PROGMEM dn[] = "down";

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
	if (state.mode == MODE_DOWN)
		return false;

	// Handle subcommands:
	for (uint8_t i = 0; !help && i < COUNT(map); i++) {
		if (strncasecmp_P(line, map[i].cmd, map[i].len))
			continue;

		if (!seek_start(map[i].up, true))
			return false;

		seek_status();
		return true;
	}

	// Fallthrough: help function:
	print_help(cmd, map, COUNT(map), STRIDE(map));
	return help;
}

static bool
cmd_tune (const char *cmd, const char *line, bool help)
{
	// Only valid in powerup mode:
	if (state.mode == MODE_DOWN)
		return false;

	if (help) {
		uart_printf("%s\n", cmd);
		return true;
	}

	return false;
}

// Need to forward-declare this one:
static bool cmd_help (const char *, const char *, bool);

// Toplevel command table:
static const struct {
	const char *cmd;
	bool (* handler) (const char *, const char *, bool);
}
map[] = {
	{ "help", cmd_help },
	{ "info", cmd_info },
	{ "mode", cmd_mode },
	{ "seek", cmd_seek },
	{ "tune", cmd_tune },
};

static bool
cmd_help (const char *cmd, const char *line, bool help)
{
	if (help) {
		uart_printf("%s\n", cmd);
		return true;
	}

	// Call all toplevel handlers in help mode:
	for (uint8_t i = 0; i < COUNT(map); i++)
		map[i].handler(map[i].cmd, skip_whitespace(line + strlen(cmd)), true);

	return true;
}

static void
prompt (void)
{
	static const char PROGMEM prompt_none[][6] = {
		[MODE_FM]   = "fm > ",
		[MODE_AM]   = "am > ",
		[MODE_SW]   = "sw > ",
		[MODE_DOWN] = "-- > ",
	};

	static const char PROGMEM prompt_freq[][9] = {
		[MODE_FM] = "fm %u > ",
		[MODE_AM] = "am %u > ",
		[MODE_SW] = "sw %u > ",
	};

	(state.tune.freq)
		? uart_printf_P(prompt_freq[state.mode], state.tune.freq)
		: uart_printf_P(prompt_none[state.mode]);
}

// Parse a zero-terminated commandline
void
cmd (const char *line)
{
	static const char PROGMEM failed[] = "failed\n";

	if (!line)
		return;

	// Start on new line:
	uart_printf("\n");

	// Skip whitespace at start:
	line = skip_whitespace(line);

	// Try to match a command:
	for (uint8_t i = 0; i < COUNT(map); i++)
		if (strncasecmp(line, map[i].cmd, strlen(map[i].cmd)) == 0)
			if (!map[i].handler(map[i].cmd, skip_whitespace(line + strlen(map[i].cmd)), false))
				uart_printf_P(failed);

	prompt();
}

static void
banner (void)
{
	// http://patorjk.com/software/taag/#p=display&f=Doom&t=Radiuno
	static const char PROGMEM banner[] =
		"\n"
		"______          _ _\n"
		"| ___ \\        | (_)\n"
		"| |_/ /__ _  __| |_ _   _ _ __   ___\n"
		"|    // _` |/ _` | | | | | '_ \\ / _ \\\n"
		"| |\\ \\ (_| | (_| | | |_| | | | | (_) |\n"
		"\\_| \\_\\__,_|\\__,_|_|\\__,_|_| |_|\\___/\n"
		"\n";

	uart_printf_P(banner);
}

void
cmd_init (void)
{
	banner();

	if (cmd_mode(NULL, "fm", false))
	       cmd_seek(NULL, "up", false);

	prompt();
}