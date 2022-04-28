#include <stdbool.h>
#include <stdint.h>

enum si4735_mode {
	SI4735_MODE_DOWN,
	SI4735_MODE_FM,		// FM
	SI4735_MODE_AM,		// AM/SW/LW
};

struct si4735_rev {
	uint8_t  status;
	uint8_t  part_number;
	uint8_t  fwmajor;
	uint8_t  fwminor;
	uint16_t patch_id;
	uint8_t  cmpmajor;
	uint8_t  cmpminor;
	uint8_t  chip_rev;
};

struct si4735_tune_status {
	uint8_t  status;
	uint8_t  flags;
	uint16_t freq;
	uint8_t  rssi;
	uint8_t  snr;
	union {
		struct {
			uint8_t mult;
			uint8_t readantcap;
		} fm;
		struct {
			uint16_t readantcap;
		} am;
	};
};

struct si4735_rsq_status {
	uint8_t  status;
	uint8_t flags[2];
	uint8_t stblend;	// Only in FM mode
	uint8_t rssi;
	uint8_t snr;
	uint8_t mult;		// Only in FM mode
	uint8_t freqoff;	// Only in FM mode
};

extern void si4735_init (void);

extern bool si4735_rev_get (struct si4735_rev *);

extern bool si4735_prop_get (uint16_t prop, uint16_t *val);
extern bool si4735_prop_set (uint16_t prop, uint16_t val);

extern bool si4735_fm_power_up (void);
extern bool si4735_am_power_up (void);
extern bool si4735_sw_power_up (void);

extern bool si4735_power_down (void);

extern bool si4735_fm_freq_set (uint16_t freq, bool fast, bool freeze);
extern bool si4735_am_freq_set (uint16_t freq, bool fast);
extern bool si4735_sw_freq_set (uint16_t freq, bool fast);

extern bool si4735_fm_tune_status (struct si4735_tune_status *);
extern bool si4735_am_tune_status (struct si4735_tune_status *);
extern bool si4735_sw_tune_status (struct si4735_tune_status *);

extern bool si4735_rsq_status (struct si4735_rsq_status *);
extern bool si4735_seek_start (const bool up, const bool wrap, const bool sw);
extern bool si4735_seek_cancel (void);
extern enum si4735_mode si4735_mode_get (void);
