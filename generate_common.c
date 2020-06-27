#include "generate_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <float.h> // DBL_MAX
#include <string.h> // memcpy
#include <unistd.h> // sleep

#include "generate_normal.h"
#include "generate_symmetric.h"
#include "seed_common.h"
#include "seed_normal.h"
#include "seed_symmetric.h"
#include "setup.h"
#include "color.h"
#include "progress.h"
#include "safelib.h"
#include "pnmlib.h"
#include "randint.h"
#include "debug.h"

struct offset *offsets = NULL;
struct offset *floodoffsets = NULL; // every offset or its inverse once (i.e. not both)

int offsetcount = 0;
int floodoffsetcount = 0;

int seeds = -1;
int colorcount = -1;

int workercount = 2;

bool inner = true;
bool symmetric = false;
int sym_hcount = 1;
int sym_vcount = 1;
bool sym_h_hflip = true; // horizontal flip from one section to the next horizontally (pq)
bool sym_h_vflip = false; // vertical flip from one section to the next horizontally (pb)
bool sym_v_hflip = false; // horizontal flip from one section to the next vertically (p/q)
bool sym_v_vflip = false; // vertical flip from one section to the next vertically (p/b)

bool sym_sharedrow = false; // Is the row between sections shared between them (true for 1 vertical section, otherwise depends on height)
bool sym_sharedcolumn = false; // Is the column between sections shared between them (true for 1 horizontal section, otherwise depends on width)

int sym_maxrow = 0; // highest row number that is is not already
int sym_maxcolumn = 0;

double maxfitness = DBL_MAX;

int start_wait_time = 0;


/*struct progressdata {
	pthread_rwlock_t *datalock;
	pthread_barrier_t *progressbarrier;
	struct pnmdata *data;
*/


// progress thread --RO lock----------------------------------RO unlock-------------------------------------------------------------------------------------pbarrier RO lock----
// main thread ------groupbarrier--------------------------groupbarrier----------------------------------------------------------------------------------groupbarrier-- pbarrier --groupbarrier(beginning)
// worker 0          groupbarrier-- RO lock -- RO unlock --groupbarrier-- WR lock -wbarrier- WR unlock wbarrier------------wbarrier------------wbarrier--groupbarrier--------------groupbarrier(beginning)
// worker 1          groupbarrier-- RO lock -- RO unlock --groupbarrier------------wbarrier-- WR lock -wbarrier- WR unlock wbarrier------------wbarrier--groupbarrier--------------groupbarrier(beginning)
// worker N          groupbarrier-- RO lock -- RO unlock --groupbarrier------------wbarrier------------wbarrier-- WR lock -wbarrier- WR unlock wbarrier--groupbarrier--------------groupbarrier(beginning)

// id-loops (all waiting all the time, one wbarrier)
//worker 0 ----------- do thing -wbarrier----wait----wbarrier----wait---
//worker 1 --------------wait----wbarrier- do thing -wbarrier----wait----
//worker N --------------wait----wbarrier----wait----wbarrier- do thing -

// id-loops (each waits until it is done, then moves on; workercount-1 wbarriers)
//worker 0 ----------- do thing -wbarrier0---------------------------------
//worker 1 --------------wait----wbarrier0- do thing -wbarrier1------------
//worker 3 -----------------------------------wait----wbarrier1- do thing -
// this is used

static void allocoffsets(int count);

static void normaloffsets(void);
static void knightoffsets(void);
static void cardinaloffsets(void);
static void diagonaloffsets(void);
static void coordinateoffsets(char *optarg);

void (*shuffleoffsets)(void) = NULL;
static void doshuffleoffsets(void);
static void dontshuffleoffsets(void);

void (*generate)(struct pnmdata *data, bool *used_, bool *blocked_) = NULL;

bool generate_option(int c, char *optarg) {
	int ret = 0, count = 0, count2 = 0, flags_used = 0;
	switch (c) {
		case 'e': // seed count
			ret = sscanf(optarg, "%d%n", &seeds, &count);
			if (ret != 1 || seeds < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid seed count: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'maxf':
			ret = sscanf(optarg, "%lf%n", &maxfitness, &count);
			if (ret != 1 || maxfitness < 1. || optarg[count] != 0) {
				fprintf(stderr, "Invalid max fitness: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'O': // add offsets to the offset list
			switch (optarg[0]) {
				case 'n': // normal
					normaloffsets();
					break;
				case 'k': // knight's-path
					knightoffsets();
					break;
				case 'c': // cardinal directions
					cardinaloffsets();
					break;
				case 'd': // diagonal directions
					diagonaloffsets();
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '-': // relative coordinate pair
					coordinateoffsets(optarg);
					break;
				default:
					fprintf(stderr, "Unrecognized offset type: '%s'\n", optarg);
					exit(EXIT_FAILURE);
			}
			break;
		case 'nshf': // don't shuffle offsets
			shuffleoffsets = dontshuffleoffsets;
			break;
		case 'w': // worker count
			ret = sscanf(optarg, "%d%n", &workercount, &count);
			if (ret != 1 || workercount < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid worker count: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'C': // color count
			ret = sscanf(optarg, "%d%n", &colorcount, &count);
			if (ret != 1 || colorcount < 1 || optarg[count] != 0) {
				fprintf(stderr, "Invalid color count: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'symm': // symmetry
			ret = sscanf(optarg, "%d%n", &sym_hcount, &count);
			flags_used = 0;
			while (optarg[count] != ',') {
				if (optarg[count] == 'n' && flags_used == 0) {
					sym_h_hflip = false;
				}
				else if (optarg[count] == 'n' && flags_used == 1) {
					sym_h_vflip = false;
				}
				else if (flags_used == 2) {
					break;
				}
				else if (optarg[count] == 'h') {
					sym_h_hflip = true;
				}
				else if (optarg[count] == 'v') {
					sym_h_vflip = false;
				}
				else { // This will catch the null byte if it occurs, so no segfault
					fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
					exit(EXIT_FAILURE);
				}
				count++;
				flags_used++;
			}
			if (flags_used == 0 && sym_hcount == 1) {
				sym_h_hflip = true; // so the edges dont wrap by default
			}

			if (ret != 1 || sym_hcount < 1 || optarg[count] != ',') {
				fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			count++; // the comma in the specifier
			ret = sscanf(optarg+count, "%d%n", &sym_vcount, &count2);
			flags_used = 0;
			while (optarg[count+count2] != '\0') {
				if (optarg[count+count2] == 'n' && flags_used == 0) {
					sym_v_hflip = false;
				}
				else if (optarg[count+count2] == 'n' && flags_used == 1) {
					sym_v_vflip = false;
				}
				else if (flags_used == 2) {
					break;
				}
				else if (optarg[count+count2] == 'h') {
					sym_v_hflip = true;
				}
				else if (optarg[count+count2] == 'v') {
					sym_v_vflip = false;
				}
				else {
					fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
					exit(EXIT_FAILURE);
				}
				count2++;
				flags_used++;
			}
			if (flags_used == 0 && sym_vcount == 1) {
				sym_v_vflip = true; // so the edges dont wrap by default
			}
			if (ret != 1 || sym_vcount < 1 || optarg[count+count2] != 0) {
				fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			symmetric = true;
			break;
		case 'oute':
			inner = false;
			break;
		case 'wsta': // wait start time
			ret = sscanf(optarg, "%d%n", &start_wait_time, &count);
			if (ret != 1 || start_wait_time < 0 || optarg[count] != 0) {
				fprintf(stderr, "Invalid start wait time: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			return false;
	}
	return true;
}

void generate_finalize(struct pnmdata *data, bool *blocked_) {
	if (offsets == NULL)
		normaloffsets();
	if (shuffleoffsets == NULL)
		shuffleoffsets = doshuffleoffsets;
	//if (colorcount < 0)
	//	colorcount = 1; // in progress_finalize
	for (int i = 0; i < offsetcount; ++i) {
		bool exists = false;
		for (int j = 0; j < floodoffsetcount; ++j) {
			if (
				offsets[i].dx == floodoffsets[j].dx && \
				offsets[i].dy == floodoffsets[j].dy
			) exists = true;
			if (
				offsets[i].dx == -floodoffsets[j].dx && \
				offsets[i].dy == -floodoffsets[j].dy
			) exists = true;
		}
		if (!exists) {
			floodoffsets = sreallocarray(floodoffsets, ++floodoffsetcount, sizeof(*floodoffsets));
			floodoffsets[floodoffsetcount-1].dx = offsets[i].dx;
			floodoffsets[floodoffsetcount-1].dy = offsets[i].dy;
		}
	}
	if (
		data->dimx % sym_hcount == 0 || // Exact fit
		data->dimx % sym_hcount == 1    // Overlapping by 1 pixel
	) {
		// pass
	}
	else {
		fprintf(stderr, "Invalid width %d for symmetry count %d\n", data->dimx, sym_hcount);
		exit(EXIT_FAILURE);
	}
	if (
		data->dimy % sym_vcount == 0 || // Exact fit
		data->dimy % sym_vcount == 1    // Overlapping by 1 pixel
	) {
		// pass
	}
	else {
		fprintf(stderr, "Invalid height %d for symmetry count %d\n", data->dimy, sym_vcount);
		exit(EXIT_FAILURE);
	}

	if (symmetric) {
		compute_floodplanes_symmetric(data, blocked_);
	}
	else {
		compute_floodplanes_normal(data, blocked_);
	}

	if (seeds < 0)
		seeds = floodplanecount;
	//fprintf(stderr, "\t%d floodplanes %d seeds\n", floodplanecount, seeds);
	if (inner) {
		if (symmetric) {
			generate = generate_inner_symmetric;
		}
		else {
			generate = generate_inner_normal;
		}
	}
	else {
		if (symmetric) {
			generate = generate_outer_symmetric;
		}
		else {
			generate = generate_outer_normal;
		}
	}
}

static void allocoffsets(int count) {
	offsetcount += count;
	offsets = sreallocarray(offsets, offsetcount, sizeof(*offsets));
}

static void normaloffsets(void) {
	struct offset *oss;
	allocoffsets(8);
	oss = &offsets[offsetcount-8];
	oss[0].dy = oss[1].dy = oss[2].dy = \
		oss[0].dx = oss[3].dx = oss[5].dx = -1;

	oss[3].dy = oss[4].dy = \
		oss[1].dx = oss[6].dx = 0;

	oss[5].dy = oss[6].dy = oss[7].dy = \
		oss[2].dx = oss[4].dx = oss[7].dx = 1;
	// 012
	// 3X4
	// 567
}

static void knightoffsets(void) {
	struct offset *oss;
	allocoffsets(8);
	oss = &offsets[offsetcount-8];
	oss[0].dy = oss[1].dy = \
		oss[2].dx = oss[4].dx = -2;

	oss[2].dy = oss[3].dy = \
		oss[0].dx = oss[6].dx = -1;

	oss[4].dy = oss[5].dy = \
		oss[1].dx = oss[7].dx = 1;

	oss[6].dy = oss[7].dy = \
		oss[3].dx = oss[5].dx = 2;

	//  0 1
	// 2   3
	//   X
	// 4   5
	//  6 7
}

static void cardinaloffsets(void) {
	struct offset *oss;
	allocoffsets(4);
	oss = &offsets[offsetcount-4];
	oss[0].dy = \
		oss[1].dx = -1;

	oss[1].dy = oss[2].dy = \
		oss[0].dx = oss[3].dx = 0;

	oss[3].dy = \
		oss[2].dx = 1;
	//  0
	// 1X2
	//  3
}

static void diagonaloffsets(void) {
	struct offset *oss;
	allocoffsets(4);
	oss = &offsets[offsetcount-4];
	oss[0].dy = oss[1].dy = \
		oss[0].dx = oss[2].dx = -1;

	oss[2].dy = oss[3].dy = \
		oss[1].dx = oss[3].dx = 1;
	// 0 1
	//  X
	// 2 3
}

static void coordinateoffsets(char *optarg) {
	int ret, dx, dy, index;
	struct offset *oss;
	allocoffsets(1);
	oss = &offsets[offsetcount-1];
	ret = sscanf(optarg, "%d%n", &dx, &index);
	if (ret != 1) {
		fprintf(stderr, "Unrecognized offset coordinate.\n");
		exit(EXIT_FAILURE);
	}
	++index; // IDC what character separates the two numbers
	ret = sscanf(optarg+index, " %d", &dy);
	if (ret != 1) {
		fprintf(stderr, "Unrecognized offset coordinate.\n");
		exit(EXIT_FAILURE);
	}
	oss->dx = dx;
	oss->dy = dy;
}

static void doshuffleoffsets(void) {
	struct offset temp;
	for (int i = offsetcount-1; i>=0; --i) {
		int j = randint(0, i+1);
		if (j != i) {
			temp = offsets[i];
			offsets[i] = offsets[j];
			offsets[j] = temp;
		}
	}
}

static void dontshuffleoffsets(void) {}

double inner_fitness(int dimx, int dimy, const __m128 *values_, struct pixel pixel, const __m128 color) {
	const __m128 (*values)[dimx] = (const __m128(*)[dimx]) values_;
	__m128 diff = (values[pixel.y][pixel.x] - color)*65536;
	__m128 sq_diff = diff * diff;
	double ret = sq_diff[3] + sq_diff[2] + sq_diff[1] + sq_diff[0];
	return ret;
}
