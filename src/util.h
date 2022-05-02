#pragma once

// Size of one array element in bytes.
#define STRIDE(array) \
	(sizeof (*(array)))

// Number of elements in the array.
#define NELEM(array) \
	(sizeof (array) / STRIDE(array))

// Iterate over each array element.
#define FOREACH(array, iter) \
	for (__typeof__(*(array)) *iter = (array); \
		iter < (array) + NELEM(array); \
		iter++)
