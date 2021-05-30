#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

#include "pnmlib.h"
#include "debug.h"

#define INPUT_SHORTOPTS "i:W:B:"

#define INPUT_LONGOPTS \
	{"input", required_argument, NULL, 'i'}, \
	{"whitelist", required_argument, NULL, 'W'}, \
	{"blacklist", required_argument, NULL, 'B'}, \
	{"background", required_argument, NULL, 'bkgd'}, \

#define INPUT_HELP \
	"Input Options\n" \
	"	-i <file> --input <file>       Start with <file> centered on the canvas.\n"\
	"	-W <file> --whitelist <file>   Only pixels that are white in <file> are filled.\n"\
	"	-B <file> --blacklist <file>   Pixels that are black in <file> are not filled.\n"\
	"	--background <file>            Pixels that are not yet filled are pixels in <file>.\n"\
	""

bool input_option(int c, char *optarg);

void input_finalize(struct pnmdata *data, bool *used_, bool *blocked_);



#endif // INPUT_H
