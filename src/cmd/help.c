#include "../cmd.h"
#include "../uart.h"

// Forward declaration.
static struct cmd cmd;

static void
on_help (void)
{
	uart_printf("%s\n", cmd.name);
}

static bool
on_call (const struct args *args, struct cmd_state *state)
{
	(void) state;

	// Call the help callback on all modules.
	for (const struct cmd *c = cmd_list; c; c = c->next)
		c->on_help();

	return true;
}

static struct cmd cmd = {
	.name    = "help",
	.on_call = on_call,
	.on_help = on_help,
};

CMD_REGISTER(&cmd);
