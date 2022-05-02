#pragma once

#include "args.h"
#include "si4735.h"

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

extern void cmd_exec (struct args *args);
extern void cmd_init (void);
