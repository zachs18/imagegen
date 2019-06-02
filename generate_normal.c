#include "generate_normal.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <float.h> // DBL_MAX
#include <string.h> // memcpy
#include <unistd.h> // sleep

#include "generate_common.h"
#include "seed_normal.h"
#include "setup.h"
#include "color.h"
#include "progress.h"
#include "safelib.h"
#include "pnmlib.h"
#include "randint.h"
#include "debug.h"

void generate_inner_normal(struct pnmdata *data, bool *used_, bool *blocked_);
static void *generate_inner_worker_normal(void *gdata_); // struct generatordata *gdata
static bool valid_edge_inner_normal(int dimx, int dimy, int x, int y, bool *used_);
static bool add_edge_inner_normal(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_);

void generate_outer_normal(struct pnmdata *data, bool *used_, bool *blocked_);
static void *generate_outer_worker_normal(void *gdata_); // struct generatordata *gdata
static bool valid_edge_outer_normal(int dimx, int dimy, int x, int y, bool *used_);
static bool add_edge_outer_normal_nocheck(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_);


void generate_inner_normal(struct pnmdata *data, bool *used_, bool *blocked_) {
	int dimx = data->dimx, dimy = data->dimy;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	struct edgelist edgelist = {NULL, 0};
	edgelist.edges = scalloc(dimx*dimy, sizeof(struct pixel));
	volatile int pixels = 0;
	debug_1;
	seed_image_normal(data, used_, seeds);
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
	volatile bool finished = false;
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
		pthread_create(&workers[i], NULL, generate_inner_worker_normal, &generatordata);
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
			pixels += seed_image_normal(data, used_, 1);
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
			if (!valid_edge_inner_normal(dimx, dimy, edgelist.edges[i].x, edgelist.edges[i].y, used_)) { // dont need to check blocked, shouldn't be there in the first place
				edgelist.edgecount--;
				if (i != edgelist.edgecount) {
					memcpy(&edgelist.edges[i], &edgelist.edges[edgelist.edgecount], sizeof(*edgelist.edges));
				}
			}
		}
	}
	debug_0;
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

static void *generate_inner_worker_normal(void *gdata_) {
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
					if (valid_edge_inner_normal(dimx, dimy, x+dx, y+dy, (bool*) used))
						add_edge_inner_normal(dimx, dimy, x+dx, y+dy, (struct edgelist *const) edgelist, (bool*) used);
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

static bool valid_edge_inner_normal(int dimx, int dimy, int x, int y, bool *used_) {
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	if (x < 0 || x >= dimx || y < 0 || y >= dimy || !used[y][x])
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

static bool add_edge_inner_normal(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_) {
	//bool (*used)[dimx] = (bool(*)[dimx]) used_;
	int edgecount = edgelist->edgecount;
	for (int i = 0; i < edgecount; ++i) {
		if (edgelist->edges[i].x == x && edgelist->edges[i].y == y)
			return false;
	}
	edgelist->edges[edgecount].x = x;
	edgelist->edges[edgecount].y = y;
	edgelist->edgecount++;
	return true;
}

static double outer_fitness_normal_maximum(int dimx, int dimy, double *values_, bool *used_, struct pixel pixel, double *color);


void generate_outer_normal(struct pnmdata *data, bool *used_, bool *blocked_) {
	int dimx = data->dimx, dimy = data->dimy;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	struct edgelist edgelist = {NULL, 0};
	edgelist.edges = scalloc(dimx*dimy, sizeof(struct pixel));
	volatile int pixels = 0;
	debug_1;
	seed_image_normal(data, used_, seeds);
	debug_1;
	for (int y = 0; y < dimy; ++y) {
		for (int x = 0; x < dimx; ++x) {
			if (used[y][x]) {
				for (int i = 0; i < offsetcount; ++i) {
					int ty = y+offsets[i].dy, tx = x+offsets[i].dx;
					if (ty >= 0 && ty < dimy && tx >= 0 && tx < dimx && !used[ty][tx] && !blocked[ty][tx]) {
						edgelist.edges[edgelist.edgecount].x = x;
						edgelist.edges[edgelist.edgecount].y = y;
						edgelist.edgecount++;
					}
				// we want to increase pixel count for blocked pixels, but not have them as edges
				pixels++;
				//fprintf(stderr, "Main thread, %d %s:%d\n", pixels, __FILE__, __LINE__);
				}
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
		pthread_create(&workers[i], NULL, generate_outer_worker_normal, &generatordata);
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
			pixels += seed_image_normal(data, used_, 1);
			//pixels += seed_image_naive(data, used_, 1);
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					if (used[y][x]) {
						for (int i = 0; i < offsetcount; ++i) {
							int tx = x + offsets[i].dx, ty = y + offsets[i].dy;
							if (ty >= 0 && ty < dimy && \
								tx >= 0 && tx < dimx && \
								!used[ty][tx]
							   ) {
								debug(1, "%d,%d is a valid edge next to %d,%d (used)\n", tx, ty, x, y); 
								add_edge_outer_normal_nocheck(dimx, dimy, tx, ty, &edgelist, (bool*) used);
							}
						}
						//fprintf(stderr, "Main thread, %d %s:%d\n", pixels, __FILE__, __LINE__);
					}
				}
			}
		}
		for (int i = 0; i < edgelist.edgecount; ++i) {
			if (!valid_edge_outer_normal(dimx, dimy, edgelist.edges[i].x, edgelist.edges[i].y, used_)) { // dont need to check blocked, shouldn't be there in the first place
				debug(1, "%d %d is invalid edge (%d edges)\n", edgelist.edges[i].x, edgelist.edges[i].y, edgelist.edgecount);
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

static void *generate_outer_worker_normal(void *gdata_) {
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
			double fitness = outer_fitness_normal_maximum(dimx, dimy, (double*) values, (bool*) used, edgelist->edges[i], color);
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
			int x = best->x, y = best->y, tx, ty;
			shuffleoffsets();
				debug(-2, "Adding pixel at %d,%d\n", x, y);
			if (!used[y][x]) { // Check if another thread added a pixel here
				memcpy(values[y][x], color, depth*sizeof(*color));
				debug(-2, "Adding pixel at %d,%d\n", x, y);
				used[y][x] = true;
				(*pixels)++;
				for (int i = 0; i < offsetcount; ++i) {
					tx = x + offsets[i].dx, ty = y + offsets[i].dy;
					if (ty >= 0 && ty < dimy && \
						tx >= 0 && tx < dimx && \
						!used[ty][tx]
					   ) {
						debug(-2, "Adding edge at %d,%d next to %d,%d (used)\n", tx, ty, x, y);
						add_edge_outer_normal_nocheck(dimx, dimy, tx, ty, (struct edgelist *const) edgelist, (bool*) used);
					}
				}
			}
		}
		else {
			debug(-20, "NO BEST (%d edges)\n", edgelist->edgecount);
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

static bool valid_edge_outer_normal(int dimx, int dimy, int x, int y, bool *used_) {
	// I dont think this is needed // <- IDK when i wrote that
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	if (used[y][x])
		return false;
	for (int i = 0; i < offsetcount; ++i) {
		if (used[y+offsets[i].dy][x+offsets[i].dx])
			return true;
	}
	return false;
}

static bool add_edge_outer_normal_nocheck(int dimx, int dimy, int x, int y, struct edgelist *edgelist, bool *used_) {
	// Doesn't do bounds checking; callers that already did that
	//bool (*used)[dimx] = (bool(*)[dimx]) used_;
	int edgecount = edgelist->edgecount;
	//if (x < 0 || x >= dimx || y < 0 || y >= dimy || used[y][x])
	//	return false;
	for (int i = 0; i < edgecount; ++i) {
		if (edgelist->edges[i].x == x && edgelist->edges[i].y == y)
			return false;
	}
	edgelist->edges[edgecount].x = x;
	edgelist->edges[edgecount].y = y;
	edgelist->edgecount++;
	return true;
}

double outer_fitness_normal_maximum(int dimx, int dimy, double *values_, bool *used_, struct pixel pixel, double *color) {
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	double ret = DBL_MAX;
	debug(-3, "%d,%d\n", pixel.x, pixel.y);
	for (int i = 0; i < offsetcount; ++i) {
		int tx = pixel.x + offsets[i].dx, ty = pixel.y + offsets[i].dy;
		if (tx >= 0 && tx < dimx && ty >= 0 && ty < dimy && used[ty][tx]) {
			debug(-3, "%d,%d near %d,%d\n", tx, ty, pixel.x, pixel.y);
			double temp = inner_fitness(dimx, dimy, values_, pixel, color);
			if (temp > ret) ret = temp;
		}
	}
	return ret;
}

