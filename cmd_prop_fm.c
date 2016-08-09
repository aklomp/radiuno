#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

#include "si4735.h"
#include "uart.h"

#define STRIDE(n)	(sizeof((n)[0]))
#define COUNT(n)	(sizeof(n) / STRIDE(n))

static bool
prop_fm_deemphasis (uint8_t argc, const char **argv, bool get, bool help)
{
	uint16_t val;
	uint16_t prop = 0x1100;
	static const char PROGMEM str[] = "FM_DEEMPHASIS";

	// Handle help:
	if (help)
		goto help;

	// Check if we can match the command:
	if (strcasecmp_P(argv[0], str))
		return false;

	// Handle get:
	if (get) {
		uart_printf("%p: ", str);
		if (si4735_prop_get(prop, &val)) {
			uart_printf("%u\n", val);
			return true;
		}
		return false;
	}

	// Handle set, must have an argument:
	if (argc != 2)
		goto help;

	// Get value, check against max:
	if ((val = atoi(argv[1])) > 3)
		goto help;

	// Set the property:
	return si4735_prop_set(prop, val);

	// Help section:
help:	if (get) {
		uart_printf("  %p\n", str);
	}
	else {
		uart_printf("  %p\n", str);
		// Print range
	}
	return true;
}

// Individual handlers:
static bool (*handlers[]) (uint8_t argc, const char **argv, bool get, bool help) = {
	prop_fm_deemphasis,
};

bool
cmd_prop_fm (uint8_t argc, const char **argv, bool get, bool help)
{
	// Handle commands:
	if (!help && argc >= 2)
		for (size_t i = 0; i < COUNT(handlers); i++)
			if (handlers[i](argc - 1, argv + 1, get, false))
				return true;

	// Fallthrough to help:
	uart_printf("%s <property>, properties:\n", argv[0]);
	for (size_t i = 0; i < COUNT(handlers); i++)
		handlers[i](argc, argv, get, true);

	// If we're here on purpose, return true, else false:
	return help;
}
