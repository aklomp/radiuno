#include <stddef.h>
#include <avr/io.h>
#include <util/delay.h>

#include "si4735.h"
#include "si4735_cmd.h"

#define PIN_POWER	PORTB0
#define PIN_RESET	PORTB1

// Redefine ports as used in SPI:
#define PIN_SS		PORTB2
#define PIN_MOSI	PORTB3
#define PIN_MISO	PORTB4
#define PIN_SCK		PORTB5

// Chip commands:
#define CMD_WRITE	0x48
#define CMD_READ_SHORT	0xA0
#define CMD_READ_LONG	0xE0

// Chip bootup mode.
static enum si4735_mode mode = SI4735_MODE_DOWN;

static inline void
bswap16 (uint16_t *n)
{
	*n = __builtin_bswap16(*n);
}

static inline void
slave_select (void)
{
	PORTB &= ~_BV(PIN_SS);
	_delay_us(100);
}

static inline void
slave_unselect (void)
{
	PORTB |= _BV(PIN_SS);
}

static uint8_t
spi_xfer (uint8_t c)
{
	SPDR = c;

	while (!(SPSR & _BV(SPIF)))
		continue;

	return SPDR;
}

static void
write (const uint8_t *cmd, uint8_t len)
{
	slave_select();

	// Send prefix:
	spi_xfer(CMD_WRITE);

	// Write command:
	for (uint8_t i = 0; i < len; i++)
		spi_xfer(cmd[i]);

	// Zero-fill remainder:
	for (uint8_t i = len; i < 8; i++)
		spi_xfer(0x00);

	slave_unselect();
}

// Read short response from chip:
static struct si4735_status
read_status (void)
{
	slave_select();
	spi_xfer(CMD_READ_SHORT);
	const struct si4735_status status = { .raw = spi_xfer(0x00) };
	slave_unselect();
	return status;
}

// Read long response from chip:
static bool
read_long (uint8_t *buf, uint8_t len)
{
	slave_select();

	spi_xfer(CMD_READ_LONG);
	_delay_us(300);

	// Save response bytes into caller-supplied buffer:
	for (uint8_t i = 0; i < len; i++)
		buf[i] = spi_xfer(0x00);

	// Pad to 16 bytes transferred:
	for (uint8_t i = len; i < 16; i++)
		spi_xfer(0x00);

	slave_unselect();

	// Return error status:
	return !(buf[0] & 0x40);
}

enum si4735_mode
si4735_mode_get (void)
{
	return mode;
}

bool
si4735_freq_set (const uint16_t freq, const bool fast, const bool freeze, const bool sw)
{
	static struct {
		uint8_t  cmd;
		uint8_t  flags;
		uint16_t freq;
		uint16_t antcap;
	} c;

	switch (mode) {
	case SI4735_MODE_FM:
		c.cmd   = SI4735_CMD_FM_TUNE_FREQ;
		c.flags = (freeze << 1) | fast;
		break;

	case SI4735_MODE_AM:
		c.cmd   = SI4735_CMD_AM_TUNE_FREQ;
		c.flags = fast;

		// For the SW band, the programming guide says that the antenna
		// capacitance must be set to 1. For other bands (FM/AM/LW), it
		// is best set to zero (auto). The chip does not track the
		// specific band it is operating in, so that information must
		// be passed in through a parameter.
		c.antcap = sw ? __builtin_bswap16(1) : 0;
		break;

	default:
		return false;
	}

	c.freq  = __builtin_bswap16(freq);

	write(&c.cmd, sizeof(c));
	return !read_status().ERR;
}

bool
si4735_seek_start (const bool up, const bool wrap, const bool sw)
{
	static struct {
		uint8_t cmd;
		uint8_t flags;
		struct {
			uint16_t unused;	// AM/SW/LW only
			uint16_t antcap;	// AM/SW/LW only
		} am;
	} c;
	size_t size;

	switch (mode) {
	case SI4735_MODE_FM:
		c.cmd = SI4735_CMD_FM_SEEK_START;
		size  = sizeof (c) - sizeof (c.am);
		break;

	case SI4735_MODE_AM:
		c.cmd = SI4735_CMD_AM_SEEK_START;
		size  = sizeof (c);

		// For the SW band, the programming guide says that the antenna
		// capacitance must be set to 1. For other bands (FM/AM/LW), it
		// is best set to zero (auto). The chip does not track the
		// specific band it is operating in, so that information must
		// be passed in through a parameter.
		c.am.antcap = sw ? __builtin_bswap16(1) : 0;
		break;

	default:
		return false;
	}

	c.flags = (up << 3) | (wrap << 2);

	write(&c.cmd, size);
	return !read_status().ERR;
}

static bool
tune_status (struct si4735_tune_status *buf, const bool cancel_seek)
{
	static struct {
		uint8_t cmd;
		uint8_t flags;
	} c;

	switch (mode) {
	case SI4735_MODE_FM:
		c.cmd = SI4735_CMD_FM_TUNE_STATUS;
		break;

	case SI4735_MODE_AM:
		c.cmd = SI4735_CMD_AM_TUNE_STATUS;
		break;

	default:
		return false;
	}

	c.flags = cancel_seek << 1;

	write(&c.cmd, sizeof(c));
	return read_long((uint8_t *)buf, sizeof(*buf));
}

bool
si4735_tune_status (struct si4735_tune_status *buf)
{
	if (!tune_status(buf, false))
		return false;

	if (mode == SI4735_MODE_AM)
		bswap16(&buf->am.readantcap);

	bswap16(&buf->freq);
	return true;
}

bool
si4735_seek_cancel (void)
{
	struct si4735_tune_status buf;

	return tune_status(&buf, true);
}

bool
si4735_rsq_status (struct si4735_rsq_status *buf)
{
	uint8_t cmd;

	switch (mode) {
	case SI4735_MODE_FM:
		cmd = SI4735_CMD_FM_RSQ_STATUS;
		break;

	case SI4735_MODE_AM:
		cmd = SI4735_CMD_AM_RSQ_STATUS;
		break;

	default:
		return false;
	}

	write(&cmd, sizeof(cmd));
	return read_long((uint8_t *)buf, sizeof(*buf));
}

static bool
power_up (const enum si4735_mode new_mode)
{
	static struct si4735_status status;
	static struct {
		uint8_t cmd;
		struct {
			uint8_t FUNC    : 4;
			uint8_t XOSCEN  : 1;
			uint8_t PATCH   : 1;
			uint8_t GPO2OEN : 1;
			uint8_t CTSIEN  : 1;
		};
		uint8_t opmode;
	}
	c = {
		.cmd    = SI4735_CMD_POWER_UP,
		.XOSCEN = 1,
		.opmode = SI4735_CMD_POWER_UP_OPMODE_ANALOG_OUT,
	};

	switch (new_mode) {
	case SI4735_MODE_FM:
		c.FUNC = SI4735_CMD_POWER_UP_FUNC_FM_RECV;
		break;

	case SI4735_MODE_AM:
		c.FUNC = SI4735_CMD_POWER_UP_FUNC_AM_RECV;
		break;

	default:
		return false;
	}

	write(&c.cmd, sizeof (c));

	// The chip will send 0x80 once to confirm reception.
	read_status();

	// It returns 0x00 until powerup is done.
	while ((status = read_status()).raw == 0x00)
		continue;

	if (status.ERR)
		return false;

	mode = new_mode;
	return true;
}

bool
si4735_fm_power_up (void)
{
	return power_up(SI4735_MODE_FM);
}

bool
si4735_am_power_up (void)
{
	return power_up(SI4735_MODE_AM);
}

bool
si4735_power_down (void)
{
	static uint8_t cmd[] = { SI4735_CMD_POWER_DOWN };

	write(cmd, sizeof(cmd));

	if (read_status().ERR)
		return false;

	mode = SI4735_MODE_DOWN;
	return true;
}

bool
si4735_prop_set (uint16_t prop, uint16_t val)
{
	static struct {
		uint8_t  cmd;
		uint8_t  unused;
		uint16_t prop;
		uint16_t val;
	}
	c = {
		.cmd = SI4735_CMD_SET_PROPERTY,
	};

	c.prop = __builtin_bswap16(prop);
	c.val  = __builtin_bswap16(val);

	write(&c.cmd, sizeof(c));
	return !read_status().ERR;
}

bool
si4735_prop_get (uint16_t prop, uint16_t *val)
{
	static struct {
		uint8_t  cmd;
		uint8_t  unused;
		uint16_t prop;
	}
	c = {
		.cmd = SI4735_CMD_GET_PROPERTY,
	};
	uint8_t buf[4];

	c.prop = __builtin_bswap16(prop);

	write(&c.cmd, sizeof(c));
	if (!read_long(buf, sizeof(buf)))
		return false;

	((uint8_t *)val)[0] = buf[3];
	((uint8_t *)val)[1] = buf[2];
	return true;
}

bool
si4735_rev_get (struct si4735_rev *buf)
{
	static uint8_t cmd[] = { SI4735_CMD_GET_REV };
	write(cmd, sizeof(cmd));
	if (!read_long((uint8_t *)buf, sizeof(*buf)))
		return false;

	bswap16(&buf->patch_id);
	return true;
}

void
si4735_init (void)
{
	// Prepare pins as output:
	DDRB |= _BV(PIN_POWER) | _BV(PIN_RESET) | _BV(PIN_MISO);

	// Power down, keep Reset low:
	PORTB &= ~_BV(PIN_RESET);
	PORTB &= ~_BV(PIN_POWER);

	// Select SPI protocol:
	PORTB |= _BV(PIN_MISO);
	_delay_ms(1);

	// Reset sequence:
	PORTB |= _BV(PIN_POWER);
	_delay_ms(100);

	PORTB |= _BV(PIN_RESET);
	_delay_ms(100);

	// Turn on SPI engine:
	PRR &= ~_BV(PRSPI);

	slave_unselect();

	// SS: make output:
	DDRB |= _BV(PIN_SS);

	// MISO: make input:
	DDRB &= ~_BV(PIN_MISO);

	// 32x clock prescaler. The datasheet claims the chip supports transfer
	// speeds up to 2.5 MHz, but tests show that responses become corrupted
	// at speeds above 500 KHz.
	SPCR = _BV(SPR1);
	SPSR = _BV(SPI2X);

	// Enable SPI and set master mode:
	SPCR |= _BV(SPE) | _BV(MSTR);

	// SCK, MOSI: make output after enabling SPI:
	DDRB |= _BV(PIN_SCK) | _BV(PIN_MOSI);
}
