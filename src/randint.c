#include <stdlib.h>
#include <stdbool.h>

#include "randint.h"

/**
 * @brief Return a random int from [min,max)
 * @return A random int from [min,max)
 */
int randint(int min, int max) {
	int range = max - min;
	if (range <= RAND_MAX) {
		int high = (RAND_MAX / range) * range;
		int rand;
		while (true) {
			rand = random();
			if (rand > high)
				continue;
			return min + rand%range;
		}
	}
}