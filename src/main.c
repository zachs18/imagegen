#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#include "setup.h"
#include "input.h"
#include "generate_common.h"
#include "color.h"
#include "progress.h"
#include "output.h"
#include "debug.h"

// string literals are concatenated
const char shortopts[] = \
	"h" \
	SETUP_SHORTOPTS \
	INPUT_SHORTOPTS \
	GENERATE_SHORTOPTS \
	COLOR_SHORTOPTS \
	PROGRESS_SHORTOPTS \
	OUTPUT_SHORTOPTS \
	DEBUG_SHORTOPTS;

const struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	SETUP_LONGOPTS
	INPUT_LONGOPTS
	GENERATE_LONGOPTS
	COLOR_LONGOPTS
	PROGRESS_LONGOPTS
	OUTPUT_LONGOPTS
	DEBUG_LONGOPTS
	{NULL, 0, NULL, 0}
};

const char helptext[] = \
	"Image Generator\n\n"\
	SETUP_HELP \
	INPUT_HELP \
	GENERATE_HELP \
	COLOR_HELP \
	PROGRESS_HELP \
	OUTPUT_HELP \
	DEBUG_HELP;

int main(int argc, char **argv) {
	int c;
	while ((c = getopt_long(argc, argv , shortopts, longopts, NULL)) != -1) {
		if (setup_option(c, optarg) || \
			input_option(c, optarg) || \
			generate_option(c, optarg) || \
			color_option(c, optarg) || \
			progress_option(c, optarg) || \
			output_option(c, optarg) || \
			debug_option(c, optarg)
		) {
			//pass;
		}
		else if (c == 'h') {
			fprintf(stderr, "%s", helptext);
			exit(EXIT_SUCCESS);
		}
		else {
			fprintf(stderr, "Unrecognized option: %c\n", c == '?' ? optopt : c);
		}
	}
	struct pnmdata *data = allocpnm();
	bool *used_, *blocked_;
	setup_finalize(data, &used_, &blocked_, argv[0]);
	//int dimx = data->dimx, dimy = data->dimy, depth = data->depth;
	//bool (*used)[dimx] = (bool(*)[dimx]) used_;
	debug_1;
	input_finalize(data, used_, blocked_);

	debug_1;
	generate_finalize(data, blocked_);

	debug_1;
	color_initialize();

	debug_1;
	generate(data, used_, blocked_);

	debug_1;
	output(data);
}
