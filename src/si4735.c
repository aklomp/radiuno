#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>

#include "si4735.h"

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

// Byte fiddling:
#define BSWAP(n)	(n) = __builtin_bswap16(n)

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
static uint8_t
read_status (void)
{
	slave_select();
	spi_xfer(CMD_READ_SHORT);
	uint8_t status = spi_xfer(0x00);
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

bool
si4735_fm_freq_set (uint16_t freq, bool fast, bool freeze)
{
	static struct {
		uint8_t  cmd;
		uint8_t  flags;
		uint16_t freq;
	}
	c = {
		.cmd = 0x20,
	};

	c.flags = (freeze << 1) | (fast << 0);
	c.freq  = __builtin_bswap16(freq);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

bool
si4735_am_freq_set (uint16_t freq, bool fast)
{
	static struct {
		uint8_t  cmd;
		uint8_t  flags;
		uint16_t freq;
	}
	c = {
		.cmd = 0x40,
	};

	c.flags = fast;
	c.freq  = __builtin_bswap16(freq);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

bool
si4735_sw_freq_set (uint16_t freq, bool fast)
{
	static struct {
		uint8_t  cmd;
		uint8_t  flags;
		uint16_t freq;
		uint16_t antcap;
	}
	c = {
		.cmd    = 0x40,
		.antcap = 0x01,
	};

	c.flags = fast;
	c.freq  = __builtin_bswap16(freq);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

bool
si4735_fm_seek_start (bool up, bool wrap)
{
	static struct {
		uint8_t cmd;
		uint8_t flags;
	}
	c = {
		.cmd = 0x21,
	};

	c.flags = (up ? 0x08 : 0x00) | (wrap ? 0x04 : 0x00);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

bool
si4735_am_seek_start (bool up, bool wrap)
{
	static struct {
		uint8_t cmd;
		uint8_t flags;
	}
	c = {
		.cmd = 0x41,
	};

	c.flags = (up ? 0x08 : 0x00) | (wrap ? 0x04 : 0x00);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

bool
si4735_sw_seek_start (bool up, bool wrap)
{
	static struct {
		uint8_t  cmd;
		uint8_t  flags;
		uint16_t unused;
		uint16_t antcap;
	}
	c = {
		.cmd	= 0x41,
		.antcap	= __builtin_bswap16(0x01),
	};

	c.flags = (up ? 0x08 : 0x00) | (wrap ? 0x04 : 0x00);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
}

static bool
tune_status (bool fm, void *buf, uint8_t len, bool cancel_seek)
{
	static struct {
		uint8_t cmd;
		uint8_t flags;
	} c;

	c.cmd   = fm ? 0x22 : 0x42;
	c.flags = cancel_seek ? 0x02 : 0x00;

	write(&c.cmd, sizeof(c));
	if (!read_long(buf, len))
		return false;

	return true;
}

bool
si4735_fm_tune_status (struct si4735_tune_status *buf)
{
	if (!tune_status(true, buf, sizeof(*buf), false))
		return false;

	BSWAP(buf->freq);
	return true;
}

bool
si4735_am_tune_status (struct si4735_tune_status *buf)
{
	if (!tune_status(false, buf, sizeof(*buf), false))
		return false;

	BSWAP(buf->freq);
	return true;
}

bool
si4735_sw_tune_status (struct si4735_tune_status *buf)
	__attribute__((alias ("si4735_am_tune_status")));

bool
si4735_fm_seek_cancel (void)
{
	struct si4735_tune_status buf;
	return tune_status(true, &buf, sizeof(buf), true);
}

bool
si4735_am_seek_cancel (void)
{
	struct si4735_tune_status buf;
	return tune_status(false, &buf, sizeof(buf), true);
}

bool
si4735_sw_seek_cancel (void)
	__attribute__((alias ("si4735_am_seek_cancel")));

static bool
rsq_status (bool fm, struct si4735_rsq_status *buf)
{
	uint8_t cmd = fm ? 0x23 : 0x43;
	write(&cmd, sizeof(cmd));
	return read_long((uint8_t *)buf, sizeof(*buf));
}

bool
si4735_fm_rsq_status (struct si4735_rsq_status *buf)
{
	return rsq_status(true, buf);
}

bool
si4735_am_rsq_status (struct si4735_rsq_status *buf)
{
	return rsq_status(false, buf);
}

bool
si4735_sw_rsq_status (struct si4735_rsq_status *buf)
	__attribute__((alias ("si4735_am_rsq_status")));

static bool
power_up (uint8_t *cmd, uint8_t len)
{
	uint8_t status;

	write(cmd, len);

	// The chip will send 0x80 once to confirm reception:
	read_status();

	// It returns 0x00 until powerup is done:
	while ((status = read_status()) == 0x00)
		continue;

	// Return error flag:
	return !(status & 0x40);
}

bool
si4735_fm_power_up (void)
{
	static uint8_t cmd[] = { 0x01, 0x50, 0x05 };
	return power_up(cmd, sizeof(cmd));
}

bool
si4735_am_power_up (void)
{
	static uint8_t cmd[] = { 0x01, 0x51, 0x05 };
	return power_up(cmd, sizeof(cmd));
}

bool
si4735_sw_power_up (void)
	__attribute__((alias ("si4735_am_power_up")));

bool
si4735_power_down (void)
{
	static uint8_t cmd[] = { 0x11 };
	write(cmd, sizeof(cmd));
	return !(read_status() & 0x40);
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
		.cmd = 0x12,
	};

	c.prop = __builtin_bswap16(prop);
	c.val  = __builtin_bswap16(val);

	write(&c.cmd, sizeof(c));
	return !(read_status() & 0x40);
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
		.cmd = 0x13,
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
	static uint8_t cmd[] = { 0x10 };
	write(cmd, sizeof(cmd));
	if (!read_long((uint8_t *)buf, sizeof(*buf)))
		return false;

	BSWAP(buf->patch_id);
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
