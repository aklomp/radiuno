#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "uart.h"

// Size of the line buffer:
#define LINESIZE	40

// Our line buffer:
static uint8_t line[LINESIZE], llen, lpos;

// Keys we distinguish:
enum keytype {
	KEY_REGULAR,
	KEY_BKSP,
	KEY_ENTER,
	KEY_HOME,
	KEY_DEL,
	KEY_END,
	KEY_PGUP,
	KEY_PGDN,
	KEY_ARROWUP,
	KEY_ARROWDN,
	KEY_ARROWRT,
	KEY_ARROWLT,
};

// Scancodes for special keys, sorted lexicographically:
static const uint8_t key_bksp[]		= { 0x08			};
static const uint8_t key_enter[]	= { 0x0D			};
static const uint8_t key_home[]		= { 0x1B, 0x5B, 0x31, 0x7E	};
static const uint8_t key_del[]		= { 0x1B, 0x5B, 0x33, 0x7E	};
static const uint8_t key_end[]		= { 0x1B, 0x5B, 0x34, 0x7E	};
static const uint8_t key_pgup[]		= { 0x1B, 0x5B, 0x35, 0x7E	};
static const uint8_t key_pgdn[]		= { 0x1B, 0x5B, 0x36, 0x7E	};
static const uint8_t key_arrowup[]	= { 0x1B, 0x5B, 0x41		};
static const uint8_t key_arrowdn[]	= { 0x1B, 0x5B, 0x42		};
static const uint8_t key_arrowrt[]	= { 0x1B, 0x5B, 0x43		};
static const uint8_t key_arrowlt[]	= { 0x1B, 0x5B, 0x44		};

// Table of special keys. We could store this in PROGMEM, but we don't,
// because the code to retrieve the data is much larger than the savings.
static const struct key {
	const uint8_t	*code;
	uint8_t		 len;
	enum keytype	 type;
}
keys[] = {
	{ key_bksp,	sizeof(key_bksp),	KEY_BKSP	},
	{ key_enter,	sizeof(key_enter),	KEY_ENTER	},
	{ key_home,	sizeof(key_home),	KEY_HOME	},
	{ key_del,	sizeof(key_del),	KEY_DEL		},
	{ key_end,	sizeof(key_end),	KEY_END		},
	{ key_pgup,	sizeof(key_pgup),	KEY_PGUP	},
	{ key_pgdn,	sizeof(key_pgdn),	KEY_PGDN	},
	{ key_arrowup,	sizeof(key_arrowup),	KEY_ARROWUP	},
	{ key_arrowdn,	sizeof(key_arrowdn),	KEY_ARROWDN	},
	{ key_arrowrt,	sizeof(key_arrowrt),	KEY_ARROWRT	},
	{ key_arrowlt,	sizeof(key_arrowlt),	KEY_ARROWLT	},
};

static bool
key_matches (const uint8_t c, const int8_t idx, const int8_t pos)
{
	// If key is too short, skip:
	if (pos >= keys[idx].len)
		return false;

	// Character must match:
	return (c == keys[idx].code[pos]);
}

static bool
find_key (const uint8_t c, int8_t *idx, const int8_t pos)
{
	// If character matches current index, return:
	if (c == keys[*idx].code[pos])
		return true;

	// If character is less than current index, seek up:
	if (c < keys[*idx].code[pos]) {
		for (int8_t i = *idx - 1; i >= 0; i--) {
			if (key_matches(c, i, pos)) {
				*idx = i;
				return true;
			}
		}
		return false;
	}

	// If character is greater than current index, seek down:
	else {
		for (int8_t i = *idx + 1; i < sizeof(keys) / sizeof(keys[0]); i++) {
			if (key_matches(c, i, pos)) {
				*idx = i;
				return true;
			}
		}
		return false;
	}
}

// Get next character from FIFO
static bool
next_char (enum keytype *type, uint8_t *val)
{
	static int8_t idx, pos;

	// Process all available bytes in the Rx FIFO:
	while (rx.tail != rx.head) {

		// Consume one character from the Rx FIFO:
		char c = rx.fifo[rx.tail];
		rx.tail = (rx.tail + 1) % UART_FIFOSIZE;

		// Try to find a matching key from the special keys table:
		if (find_key(c, &idx, pos)) {

			// If we are at max length, we're done:
			if (++pos == keys[idx].len) {
				*type = keys[idx].type;
				idx = pos = 0;
				return true;
			}

			// Otherwise, need more input to be certain:
			continue;
		}

		// No matching special key: it's a regular character:
		idx = pos = 0;
		*val = c;
		*type = KEY_REGULAR;
		return true;
	}

	return false;
}

// Take characters from the Rx FIFO and create a line
const uint8_t *
readline (void)
{
	enum keytype	type;
	uint8_t		val = 0;

	while (next_char(&type, &val))
	{
		switch (type)
		{
		case KEY_REGULAR:
			// Refuse insert if we are at the end of the line:
			if (lpos == LINESIZE)
			       break;

			// If there is string to the right, move it over one place:
			if (llen > lpos)
				for (int8_t i = llen; i >= lpos; i--)
					line[i + 1] = line[i];

			// Insert character:
			line[lpos++] = val;

			// Increase line length, if possible:
			if (llen < LINESIZE)
				llen++;

			// Paint new string:
			for (int8_t i = lpos - 1; i < llen; i++)
				uart_putc(line[i]);

			// Backtrack to current position:
			for (int8_t i = lpos; i < llen; i++)
				uart_putc('\b');

			break;

		case KEY_BKSP:
			// Nothing to do if we are at the start of the line:
			if (lpos == 0)
				break;

			// Line becomes one character shorter:
			llen--;
			lpos--;

			uart_putc('\b');

			// Move characters one over to the left:
			for (int8_t i = lpos; i < llen; i++) {
				line[i] = line[i + 1];
				uart_putc(line[i]);
			}
			uart_putc(' ');

			// Backtrack to current position:
			for (int8_t i = lpos; i <= llen; i++)
				uart_putc('\b');

			break;

		case KEY_ENTER:
			// Null-terminate the line and return pointer:
			line[llen] = '\0';
			llen = lpos = 0;
			return line;

		case KEY_HOME:
			while (lpos) {
				uart_putc('\b');
				lpos--;
			}
			break;

		case KEY_DEL:
			// Nothing to do if we are at the end of the line:
			if (lpos == llen)
				break;

			llen--;

			// Move characters one over to the left:
			for (uint8_t i = lpos; i < llen; i++) {
				line[i] = line[i + 1];
				uart_putc(line[i]);
			}
			uart_putc(' ');

			// Backtrack to current position:
			for (int8_t i = lpos; i <= llen; i++)
				uart_putc('\b');

			break;

		case KEY_END:
			while (lpos < llen)
				uart_putc(line[lpos++]);

			break;

		case KEY_PGUP:
		case KEY_PGDN:
		case KEY_ARROWUP:
		case KEY_ARROWDN:
			break;

		case KEY_ARROWRT:
			if (lpos < llen)
				uart_putc(line[lpos++]);

			break;

		case KEY_ARROWLT:
			if (lpos == 0)
				break;

			uart_putc('\b');
			lpos--;
			break;
		}
	}

	return NULL;
}
