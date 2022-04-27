#pragma once

#include <stdint.h>

#define ARGS_MAX	8

struct args {
	const char *av[ARGS_MAX];
	uint8_t     ac;
};

extern struct args *args_parse (char *line, struct args *args);
