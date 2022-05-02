#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "args.h"
#include "si4735.h"

#define CMD_REGISTER(CMD)			\
	__attribute__((constructor, used))	\
	static void				\
	link_cmd (void)				\
	{					\
		cmd_link(CMD);			\
	}

// Operating band of the command layer. Useful to set band limits, print
// prompts, etc. This differs from the si4735's chip mode, which has lower
// granularity and is either FM or AM/SW/LW.
enum cmd_band {
	CMD_BAND_FM,
	CMD_BAND_AM,
	CMD_BAND_SW,
	CMD_BAND_LW,
	CMD_BAND_NONE,
};

struct cmd_state {
	enum cmd_band             band;
	struct si4735_tune_status tune;
};

struct cmd {
	struct cmd *next;
	const char *name;
	bool (* on_call) (const struct args *args, struct cmd_state *state);
	void (* on_help) (void);
};

// Pointer to the first command in the linked list.
extern struct cmd *cmd_list;

extern void cmd_print_help (const char *cmd, const void *map, const uint8_t count, const uint8_t stride);
extern void cmd_link (struct cmd *cmd);
extern bool cmd_exec (const struct args *args);
extern void cmd_init (void);
