#ifndef COLOR_H
#define COLOR_H

#include <stdbool.h>
#include <x86intrin.h>

#include "setup.h" // extern int depth;
#include "debug.h"

#define COLOR_SHORTOPTS "Nnv:t:b:"

#define COLOR_LONGOPTS \
	{"normal", no_argument, NULL, 'N'}, \
	{"newvectorset", no_argument, NULL, 'n'}, \
	{"vector", required_argument, NULL, 'v'}, \
	{"basecolor", required_argument, NULL, 'b'}, \
	{"vectortype", required_argument, NULL, 't'}, \
	{"hues", no_argument, NULL, 'hues'},

#define COLOR_HELP \
	"Color Options\n" \
	"	Normal Colors - Full range of each channel\n" \
	"		-N --normal    Default\n" \
	"	Vectorized Colors - The first of these options used implies -n \n" \
	"		-n             Start a new vectorset\n" \
	"		-v <vector>    Add <vector> to the current vectorset\n" \
	"		-b <vector>    Make <vector> the starting color for the current vectorset\n" \
	"		-t <type>      Change type of vectorset to <type>: full, triangular, or sum_one\n" \
	"		--hues         All full-intensity hues\n" \
	""


bool color_option(int c, char *optarg);
void color_initialize(void);

extern __m256d (*new_color)(void);

#endif // COLOR_H


