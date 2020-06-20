#include "setup.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h> // UINT_MAX
#include <string.h> // strlen
#include <stdalign.h>

#include "safelib.h"
#include "pnmlib.h"
#include "generate_common.h" // workercount
#include "progress.h" // progress_finalize
#include "safelib.h"
#include "debug.h"

#define SEED_STR " seed: "
#define WORKERS_STR " workers: "

int dimx = -1, dimy = -1; // used in progress.c for progressmask
static int maxval = -1;
int depth = -1; // used in color.h
static unsigned int seed;
static bool seed_given = false;

bool setup_option(int c, char *optarg) {
	int ret, count;
	switch (c) {
		case 's':
			ret = sscanf(optarg, "%dx%d%n", &dimx, &dimy, &count);
			if (ret != 2 || dimx < 1 || dimy < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid size: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'x':
			ret = sscanf(optarg, "%d%n", &dimx, &count);
			if (ret != 1 || dimx < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid width: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'y':
			ret = sscanf(optarg, "%d%n", &dimy, &count);
			if (ret != 1 || dimy < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid height: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'maxv':
			ret = sscanf(optarg, "%d%n", &maxval, &count);
			if (ret != 1 || maxval < 1 || maxval > 65535 || optarg[count] != 0) {
				fprintf(stderr, "Invalid maxval: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'g':
			depth = 1;
			break;
		case 'c':
			depth = 3;
			break;
		case 'dept':
			ret = sscanf(optarg, "%d%n", &depth, &count);
			if (ret != 1 || depth < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid depth: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'S':
			ret = sscanf(optarg, "%u%n", &seed, &count);
			if (ret != 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid seed: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			seed_given = true;
			break;
		default:
			return false;
	}
	return true;
}

void setup_finalize(struct pnmdata *data, bool **used, bool **blocked, const char *progname) {
	if (dimx < 1)
		dimx = 256;
	if (dimy < 1)
		dimy = 256;
	if (maxval < 1)
		maxval = 255;
	if (depth < 1)
		depth = 3;
	initpnm(data);
	data->dimx = dimx;
	data->dimy = dimy;
	data->maxval = maxval;
	data->depth = depth;
	__m256d (*values)[dimx] = (__m256d(*)[dimx]) s_mm_malloc(dimy * sizeof(*values), alignof(*values));
	data->rawdata = (__m256d*) values;
	bool (*u)[dimx] = scalloc(dimy, sizeof(*u)); // initializes to false
	*used = (bool*) u;
	bool (*b)[dimx] = scalloc(dimy, sizeof(*b)); // initializes to false
	*blocked = (bool*) b;
	if (!seed_given) seed = time(NULL);
	srandom(seed);
	
	data->commentcount = 2;
	data->comments = scalloc(2, sizeof(*data->comments));
	int len = 0, max = UINT_MAX; //  get decimal string length
	while (max)
		len++, max /= 10; // of UINT_MAX
	data->comments[0] = scalloc(len + strlen(SEED_STR) + 1, sizeof(*data->comments[0]));
	data->comments[1] = scalloc(len + strlen(WORKERS_STR) + 1, sizeof(*data->comments[1]));
	
	snprintf(data->comments[0], len + strlen(SEED_STR) + 1, "%s%u", SEED_STR, seed); // add seed as PNM comment
	snprintf(data->comments[1], len + strlen(WORKERS_STR) + 1, "%s%u", WORKERS_STR, workercount); // add seed as PNM comment
	
	progress_finalize(progname, dimx, dimy, seed, depth);
}
