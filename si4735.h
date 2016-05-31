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

void si4735_init (void);

bool si4735_rev_get (struct si4735_rev *);

bool si4735_prop_get (uint16_t prop, uint16_t *val);
bool si4735_prop_set (uint16_t prop, uint16_t val);

bool si4735_fm_power_up (void);
bool si4735_am_power_up (void);
bool si4735_sw_power_up (void);

bool si4735_power_down (void);

bool si4735_fm_freq_set (uint16_t freq, bool fast, bool freeze);
bool si4735_am_freq_set (uint16_t freq, bool fast);
bool si4735_sw_freq_set (uint16_t freq, bool fast);

bool si4735_fm_tune_status (struct si4735_tune_status *);
bool si4735_am_tune_status (struct si4735_tune_status *);
bool si4735_sw_tune_status (struct si4735_tune_status *);

bool si4735_fm_rsq_status (struct si4735_rsq_status *);
bool si4735_am_rsq_status (struct si4735_rsq_status *);
bool si4735_sw_rsq_status (struct si4735_rsq_status *);

bool si4735_fm_seek_start (bool up, bool wrap);
bool si4735_am_seek_start (bool up, bool wrap);
bool si4735_sw_seek_start (bool up, bool wrap);

bool si4735_fm_seek_cancel (void);
bool si4735_am_seek_cancel (void);
bool si4735_sw_seek_cancel (void);
