#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>

#include "pnmlib.h"
#include "debug.h"

#define OUTPUT_SHORTOPTS "o:"

#define OUTPUT_LONGOPTS \
	{"output", required_argument, NULL, 'o'}, 

#define OUTPUT_HELP \
	"Output Options\n" \
	"	-o <file> --output <file>   Output final image as a PNM file <file>.\n"\
	""

bool output_option(int c, char *optarg);

void output(const struct pnmdata *data);

#endif // OPTION_H
