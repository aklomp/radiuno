#pragma once

#include <stdbool.h>
#include <stdint.h>

enum si4735_mode {
	SI4735_MODE_DOWN,
	SI4735_MODE_FM,		// FM
	SI4735_MODE_AM,		// AM/SW/LW
};

struct si4735_status {
	union {
		struct {
			uint8_t STCINT : 1;	// Seek/Tune Complete Interrupt
			uint8_t ASQINT : 1;	// Audio Signal Quality Interrupt (unavailable)
			uint8_t RDSINT : 1;	// Radio Data System (RDS) Interrupt
			uint8_t RSQINT : 1;	// Received Signal Quality Interrupt
			uint8_t pad    : 2;	// Reserved padding
			uint8_t ERR    : 1;	// Error
			uint8_t CTS    : 1;	// Clear to Send
		};
		uint8_t raw;
	};
};

struct si4735_rev {
	struct si4735_status status;
	uint8_t  part_number;
	uint8_t  fwmajor;
	uint8_t  fwminor;
	uint16_t patch_id;
	uint8_t  cmpmajor;
	uint8_t  cmpminor;
	uint8_t  chip_rev;
};

struct si4735_tune_status {
	struct si4735_status status;
	struct {
		uint8_t VALID : 1;	// Valid channel
		uint8_t AFCRL : 1;	// AFC Rail indicator
		uint8_t pad   : 5;	// Reserved padding
		uint8_t BLTF  : 1;	// Band limit
	};
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
	struct si4735_status status;
	struct {
		uint16_t RSSILINT : 1;	// RSSI Detect Low
		uint16_t RSSIHINT : 1;	// RSSI Detect High
		uint16_t SNRLINT  : 1;	// SNR Detect Low
		uint16_t SNRHINT  : 1;	// SNR Detect High
		uint16_t MULTLINT : 1;	// Multipath Detect Low (FM only)
		uint16_t MULTHINT : 1;	// Multipath Detect High (FM only)
		uint16_t pad0     : 1;
		uint16_t BLENDINT : 1;	// Blend Detect (FM only)
		uint16_t VALID    : 1;	// Valid Channel
		uint16_t AFCRL    : 1;	// AFC Rail Indicator
		uint16_t pad1     : 1;
		uint16_t SMUTE    : 1;	// Soft Mute Indicator
		uint16_t pad2     : 4;
	};
	struct {
		uint8_t STBLEND   : 7;	// Stereo Blend Indicator (FM only)
		uint8_t PILOT     : 1;	// Pilot Indicator (FM only)
	};
	uint8_t rssi;
	uint8_t snr;
	struct {
		uint8_t mult;		// FM only
		int8_t  freqoff;	// FM only
	} fm;
};

extern void si4735_init (void);
extern bool si4735_rev_get (struct si4735_rev *);
extern bool si4735_prop_get (uint16_t prop, uint16_t *val);
extern bool si4735_prop_set (uint16_t prop, uint16_t val);
extern bool si4735_fm_power_up (void);
extern bool si4735_am_power_up (void);
extern bool si4735_power_down (void);
extern bool si4735_freq_set (const uint16_t freq, const bool fast, const bool freeze, const bool sw);
extern bool si4735_tune_status (struct si4735_tune_status *);
extern bool si4735_rsq_status (struct si4735_rsq_status *);
extern bool si4735_seek_start (const bool up, const bool wrap, const bool sw);
extern bool si4735_seek_cancel (void);
extern enum si4735_mode si4735_mode_get (void);
