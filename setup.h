#ifndef SETUP_H
#define SETUP_H

#include <stdbool.h>

#include "pnmlib.h"
#include "debug.h"

#define SETUP_SHORTOPTS "x:y:s:gcS:"

#define SETUP_LONGOPTS \
	{"x", required_argument, NULL, 'x'}, \
	{"y", required_argument, NULL, 'y'}, \
	{"size", required_argument, NULL, 's'}, \
	{"maxval", required_argument, NULL, 'maxv'}, \
	{"grey", no_argument, NULL, 'g'}, \
	{"gray", no_argument, NULL, 'g'}, \
	{"color", no_argument, NULL, 'c'}, \
	{"depth", required_argument, NULL, 'dept'}, \
	{"seed", required_argument, NULL, 'S'}, \
	{"interactive", no_argument, NULL, 'intr'},

#define SETUP_HELP \
	"Setup Options\n" \
	"	-x <width> --x <width>     Image width.\n" \
	"	-y <height> --y <height>   Image height.\n" \
	"	-s <w>x<h> --s <w>x<h>     Image width and height.\n" \
	"	--maxval <value>           Maximum sample value.\n" \
	"	-g --grey --gray           Greyscale image (depth 1).\n" \
	"	-c --color                 Color image (depth 3).\n" \
	"	--depth <depth>            Specific depth.\n" \
	"	--seed <uint> -S <uint>    Random seed.\n" \
	""

extern int depth;
extern bool interactive;

bool setup_option(int c, char *optarg);

void setup_finalize(struct pnmdata *data, bool **used, bool **blocked, const char *progname);

void setup_interactive(struct pnmdata *data, bool *used_, bool *blocked_);


#endif // SETUP_H