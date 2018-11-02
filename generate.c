#include "generate.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <float.h> // DBL_MAX
#include <string.h> // memcpy
#include <unistd.h> // sleep

#include "seed.h"
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

int workercount = 2;
bool dividework = false;

bool symmetry = false;
int sym_hcount = 1;
int sym_vcount = 1;
bool sym_hflip = false;
bool sym_vflip = false;

static double maxfitness = DBL_MAX;


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
static void generate_inner(struct pnmdata *data, bool *used_, bool *blocked_);
static void *generate_inner_worker(void *gdata_); // struct generatordata *gdata
static bool valid_edge_inner(int dimx, int dimy, int x, int y, bool *used_);
static bool add_edge_inner(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_);
static double inner_fitness(int dimx, int dimy, double *values_, struct pixel pixel, double *color);

static void generate_outer(struct pnmdata *data, bool *used_);

bool generate_option(int c, char *optarg) {
	int ret = 0, count = 0, count2 = 0;
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
		case 'divw': // divide work
			dividework = true;
			break;
		case 'symm': // symmetry
			ret = sscanf(optarg, "%d%n", &sym_hcount, &count);
			if (optarg[count] == 'f') {
				sym_hflip = true;
				count++;
			}
			if (ret != 1 || sym_hcount < 1 || optarg[count] != ',') {
				fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			count++; // the comma in the specifier
			ret = sscanf(optarg+count, "%d%n", &sym_vcount, &count2);
			if (optarg[count+count2] == 'f') {
				sym_vflip = true;
				count++;
			}
			if (ret != 1 || sym_hcount < 1 || optarg[count+count2] != 0) {
				fprintf(stderr, "Invalid symmetry specifier: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			symmetry = true;
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
		
	
	compute_floodplanes(data, blocked_);
	
	if (seeds < 0)
		seeds = floodplanecount;
	//fprintf(stderr, "\t%d floodplanes %d seeds\n", floodplanecount, seeds);
	if (generate == NULL)
		generate = generate_inner;
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

void generate_inner(struct pnmdata *data, bool *used_, bool *blocked_) {
	int dimx = data->dimx, dimy = data->dimy;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	double (*values)[dimx][depth] = (double(*)[dimx][depth]) data->rawdata;
	struct edgelist edgelist = {NULL, 0};
	edgelist.edges = scalloc(dimx*dimy, sizeof(struct pixel));
	volatile int pixels = 0;
	debug_1;
	seed_image(data, used_, seeds);
	debug_1;
	for (int y = 0; y < dimy; ++y) {
		for (int x = 0; x < dimx; ++x) {
			if (used[y][x]) { // not using add_edge_inner because the checking is not needed here
				if (!blocked[y][x]) {
					edgelist.edges[edgelist.edgecount].x = x;
					edgelist.edges[edgelist.edgecount].y = y;
					edgelist.edgecount++;
				} // we want to increase pixel count for blocked pixels, but not have them as edges
				pixels++;
				//fprintf(stderr, "Main thread, %d %s:%d\n", pixels, __FILE__, __LINE__);
			}
		}
	}
	pthread_rwlock_t *datalock = smalloc(sizeof(pthread_rwlock_t));;
	pthread_rwlock_init(datalock, NULL);
	// locks struct pnmdata *data, int pixels,
	//       struct edgelist edgelist, 
	//       struct offset *offsets and random (for shuffling)
	pthread_barrier_t *supervisorbarrier = smalloc(sizeof(pthread_barrier_t));
	pthread_barrier_init(supervisorbarrier, NULL, 2);
	pthread_barrier_t *groupbarrier = smalloc(sizeof(pthread_barrier_t));
	pthread_barrier_init(groupbarrier, NULL, 1+workercount);
	pthread_barrier_t *workerbarriers[workercount-1];
	for (int i = 0; i < workercount-1; ++i) {
		workerbarriers[i] = smalloc(sizeof(pthread_barrier_t));
		pthread_barrier_init(workerbarriers[i], NULL, 2);
	}
	pthread_barrier_t *progressbarrier = smalloc(sizeof(pthread_barrier_t));
	pthread_barrier_init(progressbarrier, NULL, 2);
	struct generatordata generatordata = {
		datalock,
		supervisorbarrier,
		groupbarrier,
		workerbarriers,
		data,
		used_,
		&edgelist,
		&pixels,
		0,	// this changes per thread,
			// so we need to use supervisorbarrier to wait until the thread has read it
	};
	bool finished = false;
	struct progressdata progressdata = {
		datalock,
		progressbarrier,
		data,
		&finished
	};
	pthread_t progressor;
	pthread_t workers[workercount];
	for (int i = 0; i < workercount; ++i) {
		generatordata.id = i;
		pthread_create(&workers[i], NULL, generate_inner_worker, &generatordata);
		pthread_barrier_wait(supervisorbarrier);
	}
	pthread_create(&progressor, NULL, progress, &progressdata);
	pthread_barrier_wait(progressbarrier); // output first (zeroth?) progress image
	
	pthread_barrier_wait(progressbarrier); // make sure progress rdlocked
	/*FILE *testfile = fopen("test_.txt", "w");
	for (int y = 0; y < dimy; ++y) {
		for (int x = 0; x < dimx; ++x) {
			fprintf(testfile, used[y][x] ? "X" : ".");
		}
		fprintf(testfile, "\n");
	}*/
	while (pixels < dimx*dimy) {
		debug(0, "%d\n", pixels);
		pthread_barrier_wait(groupbarrier); // sync with workers
		// workers manage themselves for a while, reading then writing
		pthread_barrier_wait(groupbarrier); // sync with workers
		debug(0, "%d\n", pixels);
		// output progress image
		pthread_barrier_wait(progressbarrier);
		
		pthread_barrier_wait(progressbarrier); // make sure progress rdlocked
		debug(0, "%d\n", pixels);
		// remove any non-edge edges
		/*for (int y = 0; y < dimy; ++y) {
			for (int x = 0; x < dimx; ++x) {
				fprintf(testfile, used[y][x] ? "X" : ".");
			}
			fprintf(testfile, "\n");
		}*/
		if (edgelist.edgecount < 1) {
			pixels += seed_image(data, used_, 1);
			//pixels += seed_image_naive(data, used_, 1);
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					if (used[y][x] && !blocked[y][x]) { // not using add_edge_inner because the checking is not needed here
						edgelist.edges[edgelist.edgecount].x = x;
						edgelist.edges[edgelist.edgecount].y = y;
						edgelist.edgecount++;
						//fprintf(stderr, "Main thread, %d %s:%d\n", pixels, __FILE__, __LINE__);
					}
				}
			}
		}
		for (int i = 0; i < edgelist.edgecount; ++i) {
			if (!valid_edge_inner(dimx, dimy, edgelist.edges[i].x, edgelist.edges[i].y, used_)) { // dont need to check blocked, shouldn't be there in the first place
				edgelist.edgecount--;
				if (i != edgelist.edgecount) {
					memcpy(&edgelist.edges[i], &edgelist.edges[edgelist.edgecount], sizeof(*edgelist.edges));
				}
			}
		}
	}
	pthread_barrier_wait(progressbarrier); // last progress image (completed)
	finished = true; // between barriers to avoid race condition
	pthread_barrier_wait(progressbarrier); // make sure progress rdlocked, progress will then exit
	for (int i = 0; i < workercount; ++i) {
		debug_0;
		pthread_join(workers[i], NULL);
	}
	debug_0;
	pthread_join(progressor, NULL);
	debug_0;
	
}

static void *generate_inner_worker(void *gdata_) {
	struct generatordata *gdata = (struct generatordata *) gdata_;
	int id = gdata->id;
	pthread_barrier_t *supervisorbarrier = gdata->supervisorbarrier;
	//fprintf(stderr, "Thread %d %s:%d\n", id, __FILE__, __LINE__);
	pthread_barrier_wait(supervisorbarrier); // All values other than id are not changed
	pthread_rwlock_t *const datalock = gdata->datalock;
	pthread_barrier_t *const groupbarrier = gdata->groupbarrier;
	pthread_barrier_t *const *const wbarriers = gdata->wbarriers;
	struct pnmdata *const data = gdata->data;
	volatile struct edgelist *const edgelist = gdata->edgelist;
	const int dimx = data->dimx, dimy = data->dimy;
	double (*const values)[dimx][depth] = (double(*)[dimx][depth]) data->rawdata;
	bool (*used)[dimx] = (bool(*)[dimx]) gdata->used_;
	volatile int *const pixels = gdata->pixels;
	bool isfirst = id == 0;
	bool islast = id == workercount-1;
	double *color = scalloc(depth, sizeof(*color));
	int size = dimx*dimy;
	while (*pixels < size) { // datalock doesn't need to be locked here for pixels?
		debug(0, "worker %d\n", id);
		pthread_barrier_wait(groupbarrier); // sync all workers with main thread
		debug(0, "worker %d\n", id);
		pthread_rwlock_rdlock(datalock); // read lock until best is found
		if (!isfirst)
			pthread_barrier_wait(wbarriers[id-1]); // wait for previous thread to generate its color
		new_color(color);
		if (!islast)
			pthread_barrier_wait(wbarriers[id]); // let the next thread go 
		double bestfitness = maxfitness; // lower if better
		struct pixel *best = NULL;
		int range = edgelist->edgecount/workercount;
		int start = dividework ? range * id : 0;
		int end = dividework ? (islast ? edgelist->edgecount : range*(id+1)) : edgelist->edgecount;
		for (int i = start; i < end; ++i) {
			double fitness = inner_fitness(dimx, dimy, (double*) values, edgelist->edges[i], color);
			if (fitness < bestfitness) {
				bestfitness = fitness;
				best = &edgelist->edges[i];
			}
		}
		pthread_rwlock_unlock(datalock);
		if (!isfirst)
			pthread_barrier_wait(wbarriers[id-1]);
		pthread_rwlock_wrlock(datalock);
		// add pixel to best place
		if (best != NULL) {
			int x = best->x, y = best->y, dx, dy;
			shuffleoffsets();
			for (int i = 0; i < offsetcount; ++i) {
				dx = offsets[i].dx, dy = offsets[i].dy;
				if (y+dy >= 0 && y+dy < dimy && \
					x+dx >= 0 && x+dx < dimx && \
					!used[y+dy][x+dx]
				   ) {
					memcpy(values[y+dy][x+dx], color, depth*sizeof(*color));
					used[y+dy][x+dx] = true;
					(*pixels)++;
					if (valid_edge_inner(dimx, dimy, x+dx, y+dy, (bool*) used))
						add_edge_inner(dimx, dimy, x+dx, y+dy, edgelist, (bool*) used);
					break;
				}
			}
		}
		pthread_rwlock_unlock(datalock);
		if (!islast)
			pthread_barrier_wait(wbarriers[id]);
		pthread_barrier_wait(groupbarrier);
		// so we don't access pixels when a thread has a wrlock
		// also so that either all threads see *pixels < size, or none do
		// so generate doesn't have to call cancel
	}
	return NULL;
}

static bool valid_edge_inner(int dimx, int dimy, int x, int y, bool *used_) {
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	if (!used[y][x])
		return false;
	for (int i = 0; i < offsetcount; ++i) {
		if (y+offsets[i].dy >= 0 && y+offsets[i].dy < dimy && \
			x+offsets[i].dx >= 0 && x+offsets[i].dx < dimx && \
			!used[y+offsets[i].dy][x+offsets[i].dx]
		   )
			return true;
	}
	return false;
}

static bool add_edge_inner(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_) {
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	int edgecount = edgelist->edgecount;
	if (x < 0 || x >= dimx || y < 0 || y >= dimy)
		return false;
	for (int i = 0; i < edgecount; ++i) {
		if (edgelist->edges[i].x == x && edgelist->edges[i].y == y)
			return false;
	}
	edgelist->edges[edgecount].x = x;
	edgelist->edges[edgecount].y = y;
	edgelist->edgecount++;
	return true;
}

static double inner_fitness(int dimx, int dimy, double *values_, struct pixel pixel, double *color) {
	double (*values)[dimx][depth] = (double(*)[dimx][depth]) values_;
	double ret = 0.;
	for (int i = 0; i < depth; ++i) {
		double t = (values[pixel.y][pixel.x][i] - color[i])*65536;
		ret += t*t;
	}
	if (ret < 0.)
		return DBL_MAX;
	return ret;
}

static bool valid_edge_outer(int dimx, int dimy, int x, int y, bool *used_) {
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	if (used[y][x])
		return false;
	for (int i = 0; i < offsetcount; ++i) {
		if (used[y+offsets[i].dy][x+offsets[i].dx])
			return true;
	}
	return false;
}
