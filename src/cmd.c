#include <string.h>
#include <avr/pgmspace.h>

#include "cmd.h"
#include "uart.h"
#include "version.h"

// Forward declaration of the program banner symbols.
// Logo source:
//   http://patorjk.com/software/taag/#p=display&f=Doom&t=Radiuno
extern uint8_t _binary_src_banner_txt_start;
extern uint8_t _binary_src_banner_txt_end;

struct cmd *cmd_list = NULL;

static struct cmd_state state = {
	.band = CMD_BAND_NONE,
};

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

	// Print the version hash.
	uart_printf("Version %s\n", version);
}

void
cmd_init (void)
{
	banner();

	if (cmd_exec(&(struct args) { .ac = 2, .av = { "mode", "fm" } }))
	       cmd_exec(&(struct args) { .ac = 2, .av = { "seek", "up" } });

	prompt();
}
