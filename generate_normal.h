#ifndef GENERATE_NORMAL_H
#define GENERATE_NORMAL_H

#include <stdbool.h>

#include "generate_common.h"
#include "pnmlib.h"
#include "debug.h"

#define GENERATE_SHORTOPTS "e:O:w:"

#define GENERATE_LONGOPTS \
	{"seeds", required_argument, NULL, 'e'}, \
	{"offsets", required_argument, NULL, 'O'}, \
	{"workers", required_argument, NULL, 'w'}, \
	{"divide", required_argument, NULL, 'divw'}, \
	{"noshuffle", no_argument, NULL, 'nshf'}, \
	{"maxfitness", required_argument, NULL, 'maxf'}, \
	{"symmetry", required_argument, NULL, 'symm'},

#define GENERATE_HELP \
	"Generate Options\n" \
	"	-e <int>                      Number of seeds (Default 1, or zero if there is an input image)\n" \
	"	-O <> --offsets <>            Specify type of offsets.\n" \
	"		-O n -O normal            The 8 pixels adjacent\n" \
	"		-O k -O knight            The 8 pixels one knight's-move away\n" \
	"		-O \"1,2\"                First number is the horizontal offset, followed by the vertical offset\n" \
	"	-w <int>                      Number of worker threads.\n" \
	"	--divide                      Divide the edges among the workers, insted of all workers checking all edges.\n" \
	"	--maxfitness <int>            Maximum fitness value.\n" \
	"	--symmetry <int>[f],<int>[f]  Symmetry specifier\n" \
	""


void generate_inner_normal(struct pnmdata *data, bool *used_, bool *blocked_);
void generate_outer_normal(struct pnmdata *data, bool *used_, bool *blocked_);

#endif // GENERATE_NORMAL_H
