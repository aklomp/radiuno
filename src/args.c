#include <stddef.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "args.h"

struct args *
args_parse (char *line, struct args *args)
{
	static const char PROGMEM whitespace[] = " \t\f\v";

	args->ac = 0;

	while (args->ac < ARGS_MAX) {

		// Skip consecutive whitespace.
		while (strpbrk_P(line, whitespace) == line)
			*line++ = '\0';

		// Quit at the end of the string.
		if (*line == '\0')
			break;

		// The current character starts a token.
		args->av[args->ac++] = line++;

		// Skip to the next whitespace if it exists.
		if ((line = strpbrk_P(line, whitespace)) == NULL)
			break;
	}

	return args;
}
