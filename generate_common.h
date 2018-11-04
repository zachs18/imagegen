#ifndef GENERATE_COMMON_H
#define GENERATE_COMMON_H

#include <stdbool.h>

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
	pthread_barrier_t *const *wbarriers; // worker barriers, two workers each
	struct pnmdata *data;
	bool *used_;
	//bool *blocked_; // the workers don't need this, only the manager
	struct edgelist *edgelist;
	volatile int *pixels;
	int id; // [0, workercount)
};

extern struct offset *offsets;
extern struct offset *floodoffsets; // every offset and its inverse once each

extern int offsetcount;
extern int floodoffsetcount;

extern int seeds;

extern int workercount;
extern bool dividework;

extern bool inner;
extern bool symmetry;
extern int sym_hcount;
extern int sym_vcount;
extern bool sym_h_hflip;
extern bool sym_h_vflip;
extern bool sym_v_hflip;
extern bool sym_v_vflip;

bool generate_option(int c, char *optarg);

void generate_finalize(struct pnmdata *data, bool *blocked_);

extern void (*generate)(struct pnmdata *data, bool *used_, bool *blocked_);

extern void (*shuffleoffsets)(void);


#endif // GENERATE_h
