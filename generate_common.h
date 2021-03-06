#ifndef GENERATE_COMMON_H
#define GENERATE_COMMON_H

#include <stdbool.h>

#include "pnmlib.h"
#include "debug.h"

#define GENERATE_SHORTOPTS "e:O:w:C:"

#define GENERATE_LONGOPTS \
	{"seeds", required_argument, NULL, 'e'}, \
	{"offsets", required_argument, NULL, 'O'}, \
	{"workers", required_argument, NULL, 'w'}, \
	{"colorcount", required_argument, NULL, 'C'}, \
	{"noshuffle", no_argument, NULL, 'nshf'}, \
	{"maxfitness", required_argument, NULL, 'maxf'}, \
	{"startwait", required_argument, NULL, 'wsta'}, \
	{"symmetry", required_argument, NULL, 'symm'}, \
	{"outer", no_argument, NULL, 'oute'},

#define GENERATE_HELP \
	"Generate Options\n" \
	"	-e <int>                            Number of seeds (Default 1, or zero if there is an input image)\n" \
	"	-O <> --offsets <>                  Specify type of offsets.\n" \
	"		-O n -O normal                  The 8 pixels adjacent\n" \
	"		-O k -O knight                  The 8 pixels one knight's-move away\n" \
	"		-O \"1,2\"                      First number is the horizontal offset, followed by the vertical offset\n" \
	"	-w <int>                            Number of worker threads (doesn't change image, but higher generally means faster).\n" \
	"	--colorcount <int>                  Place <int> colors at a time (DOES change image).\n" \
	"	--maxfitness <int>                  Maximum fitness value.\n" \
	"	--symmetry <int>[nh][nv],<int>[nh][nv]  Symmetry specifier (n is no) (one n is no for both) \n" \
	"	                                    (first int: number of horizontal sections;\n" \
	"	                                     h: flip horizontally (p -> q) between adjacent horizontal sections (implied when int is 1);\n" \
	"	                                     v: flip vertically (p -> b) between adjacent horizontal sections)\n" \
	"	                                    (second int: number of vertical sections;\n" \
	"	                                     h: flip horizontally (p -> q) between adjacent vertical sections;\n" \
	"	                                     v: flip vertically (p -> b) between adjacent vertical sections (implied when int is 1))\n" \
	""

struct pixel {
	int x, y;
};

struct offset {
	int dx, dy;
};

struct edgelist {
	struct pixel *edges;
	int edgecount;
};

struct generatordata {
	pthread_rwlock_t *datalock;
	pthread_barrier_t *supervisorbarrier; // supervisor barrier, main thread and one worker
	pthread_barrier_t *groupbarrier; // group barrier, main thread and all workers
	struct pnmdata *data;
	bool *used_;
	//bool *blocked_; // the workers don't need this, only the manager
	struct edgelist *edgelist;
	volatile int *pixels;
	int id; // [0, workercount)
	volatile int *bests_; // list of indexes in edgelist; only the worker with id i can read/write index i
	volatile double *fitnesses_; // list of fitnesses at bests[i] in edgelist; only the worker with id i can read/write index i
	const double *colors_; // all workers cooperate for all colors
};

extern struct offset *offsets;
extern struct offset *floodoffsets; // every offset and its inverse once each

extern int offsetcount;
extern int floodoffsetcount;

extern int seeds;
extern int colorcount;

extern int workercount;

extern bool inner;
extern bool symmetric;
extern int sym_hcount;
extern int sym_vcount;
extern bool sym_h_hflip;
extern bool sym_h_vflip;
extern bool sym_v_hflip;
extern bool sym_v_vflip;

extern bool sym_h_hflip; // horizontal flip from one section to the next horizontally
extern bool sym_h_vflip; // vertical flip from one section to the next horizontally
extern bool sym_v_hflip; // horizontal flip from one section to the next vertically
extern bool sym_v_vflip; // vertical flip from one section to the next vertically

extern bool sym_sharedrow; // Is the row between sections shared between them
extern bool sym_sharedcolumn; // Is the column between sections shared between them

extern int sym_maxrow;
extern int sym_maxcolumn;

extern double maxfitness;

extern int start_wait_time;

bool generate_option(int c, char *optarg);

void generate_finalize(struct pnmdata *data, bool *blocked_);

extern void (*generate)(struct pnmdata *data, bool *used_, bool *blocked_);

extern void (*shuffleoffsets)(void);

extern double inner_fitness(int dimx, int dimy, const double *values_, struct pixel pixel, const double *color);

#endif // GENERATE_h
