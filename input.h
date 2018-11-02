#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

#include "pnmlib.h"
#include "debug.h"

#define INPUT_SHORTOPTS "i:W:B:"

#define INPUT_LONGOPTS \
	{"input", required_argument, NULL, 'i'}, \
	{"whitelist", required_argument, NULL, 'W'}, \
	{"blacklist", required_argument, NULL, 'B'},

#define INPUT_HELP \
	"Input Options\n" \
	"	-i <file> --input <file>   Start with <file> centered on the canvas.\n"\
	""

bool input_option(int c, char *optarg);

void input_finalize(struct pnmdata *data, bool *used_, bool *blocked_);



#endif // INPUT_H
