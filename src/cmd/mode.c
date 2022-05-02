#include <avr/pgmspace.h>

#include "../cmd.h"
#include "../uart.h"
#include "../util.h"

// Forward declaration.
static struct cmd cmd;

static const char PROGMEM sub[][3] = {
	"fm", "am", "sw", "lw"
};

// Subcommand map.
static const struct {
	const char   *cmd;
	enum cmd_band band;
}
map[] = {
	{ sub[0], CMD_BAND_FM },
	{ sub[1], CMD_BAND_AM },
	{ sub[2], CMD_BAND_SW },
	{ sub[3], CMD_BAND_LW },
};

static void
on_help (void)
{
	cmd_print_help(cmd.name, map, NELEM(map), STRIDE(map));
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
power_up (const enum cmd_band band)
{
	switch (band) {
	case CMD_BAND_AM:
	case CMD_BAND_SW:
	case CMD_BAND_LW: return si4735_am_power_up();
	case CMD_BAND_FM: return si4735_fm_power_up();
	default         : return false;
	}
}

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	// Handle insufficient args.
	if (args->ac < 2) {
		on_help();
		return false;
	}

	// Handle subcommands.
	FOREACH (map, m) {
		if (strncasecmp_P(args->av[1], m->cmd, 2))
			continue;

		// If already in the desired band, ignore.
		if (state->band == m->band)
			return true;

		// Power down the chip if not already down.
		if (state->band != CMD_BAND_NONE)
			if (!si4735_power_down())
				return false;

		// Power up the chip in the new mode.
		if (!power_up(m->band))
			return false;

		// Print chip revision data.
		print_revision();

		// For SW, set non-default band limits.
		if (m->band == CMD_BAND_SW) {
			si4735_prop_set(0x3400, 1711);
			si4735_prop_set(0x3401, 27000);
		}

		state->tune.freq = 0;
		state->band = m->band;
		return true;
	}

	return false;
}

static struct cmd cmd = {
	.name    = "mode",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
