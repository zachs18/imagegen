#include "progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#define _GNU_SOURCE // for non-modifying basename
#include <libgen.h>
#include <string.h>

#ifdef SDL_PROGRESS
#include <SDL2/SDL.h>
#include <sys/time.h>
#endif // def SDL_PROGRESS


#include "safelib.h"
#include "generate_common.h" // workercount
#include "pnmlib.h"
#include "debug.h"

static int extras = 0;

void *(*progress)(void *pdata_) = NULL;
static void *(**progresslist)(void *pdata_) = NULL;
static int progresslist_count = 0;
static int progress_interval = -1;

void *no_progress(void *pdata_);
void *progress_manager(void *pdata_);
void *progress_file(void *pdata_);
void *progress_text(void *pdata_);
#ifdef SDL_PROGRESS
void *progress_sdl2(void *pdata_);
void *progress_sdl2_helper(void *sdlhelperdata_);

struct sdlhelperdata {
	pthread_cond_t *cond;
	pthread_mutex_t *mutex;
	pthread_rwlock_t *datalock;
	const volatile bool *finished;
	int dimx;
	int dimy;
	int depth;
	double *rawdata_;
};

int sdl_wait_time = 0;
int sdl_pos[2] = {SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED};
#endif // def SDL_PROGRESS

static FILE *progressfile = NULL;
static bool progressfile_defaultname = false;
static FILE *textfile = NULL;

void add_progressor(void *(*progressor)(void *)) {
	if (progress == NULL) { // First progressor
		progress = progressor;
	}
	else if (progresslist == NULL) { // Second progressor
		progresslist = scalloc(2, sizeof(*progresslist));
		progresslist_count = 2;
		progresslist[0] = progress;
		progresslist[1] = progressor;
		progress = progress_manager;
	}
	else { // Later progressors
		progresslist = sreallocarray(progresslist, progresslist_count+1, sizeof(*progresslist));
		progresslist[progresslist_count] = progressor;
		++progresslist_count;
	}
}

bool progress_option(int c, char *optarg) {
	int ret, count;
	switch (c) {
		case 'P':
			if (progressfile != NULL || progressfile_defaultname) {
				fprintf(stderr, "Repeated progress file '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			else if (optarg == 0) {
				progressfile_defaultname = true;
			}
			else if (!strcmp("-", optarg)) {
				progressfile = stdout;
			}
			else {
				progressfile = fopen(optarg, "wb");
				if (progressfile == NULL) {
					fprintf(stderr, "Could not open progress file '%s'.\n", optarg);
					exit(EXIT_FAILURE);
				}
			}
			add_progressor(progress_file);
			break;
		case 'M':
			if (sscanf(optarg, "%d", &extras) != 1) {
				fprintf(stderr, "Invalid extra progress count: %s.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'I':
			if (sscanf(optarg, "%d", &progress_interval) != 1) {
				fprintf(stderr, "Invalid progress interval: %s.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'T':
			textfile = stderr;
			add_progressor(progress_text);
			break;
		case 'nopr':
			add_progressor(no_progress);
			break;
	#ifdef SDL_PROGRESS
		case 'SDL':
			add_progressor(progress_sdl2);
			break;
		case 'wait':
			if (optarg == 0) {
				sdl_wait_time = -1; // indefinitely
			}
			else if (sscanf(optarg, "%d", &sdl_wait_time) != 1) {
				fprintf(stderr, "Invalid wait time: %s.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'pos':
			ret = sscanf(optarg, "%d,%d%n", &sdl_pos[0], &sdl_pos[1], &count);
			if (ret != 2 || sdl_pos[0] < 0 || sdl_pos[1] < 0 || optarg[count] != 0) {
				fprintf(stderr, "Invalid position: '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
	#endif // def SDL_PROGRESS
		default:
			return false;
	}
	return true;
}

void progress_finalize(const char *progname, int dimx, int dimy, unsigned int seed, int depth) {
	debug(-3, "progress == %p\n", progress);
	if (progress == NULL || progressfile_defaultname) { // No progress was requested, use default file output
		char *tempstr = smalloc(128);
		char *base = basename(progname);
		snprintf(tempstr, 128, "progress_%s_%dx%d_%u.p%cm", base, dimx, dimy, seed, depth == 1 ? 'g' : 'p');
		progressfile = fopen(tempstr, "wb");
		if (progressfile == NULL) {
			fprintf(stderr, "Could not open progress file %s\n", tempstr);
			exit(EXIT_FAILURE);
		}
		free(tempstr);
		if (progress == NULL) {
			add_progressor(progress_file);
			
			textfile = stderr;
			add_progressor(progress_text);
		}
	}
	//debug(-1, "%d %d\n", workercount, progress_interval);
	progress_interval /= workercount;
	//debug(-1, "%d %d\n", workercount, progress_interval);
	if (progress_interval < 1) {
		progress_interval = 256 / workercount;
	}
}

void *no_progress(void *pdata_) { // also an example
	struct progressdata *pdata = (struct progressdata *) pdata_;
	
	pthread_rwlock_t *datalock = pdata->datalock;
	pthread_barrier_t *progressbarrier = pdata->progressbarrier;
	const struct pnmdata *const data = pdata->data;
	const volatile bool *finished = pdata->finished;
	int step_count = 0;
	while (!*finished) {
		pthread_barrier_wait(progressbarrier);
		
		pthread_rwlock_rdlock(datalock);
		pthread_barrier_wait(progressbarrier); // ensure rdlock
		if (step_count % progress_interval == 0) {
			// Output here
		}
		pthread_rwlock_unlock(datalock);
		++step_count;
	}
	pthread_rwlock_rdlock(datalock);
	for (int i = 0; i < extras; ++i) {
		// Output here
	}
	pthread_rwlock_unlock(datalock);
	debug_0;
	return NULL;
}

void *progress_manager(void *pdata_) {
	struct progressdata *pdata = (struct progressdata *) pdata_;
	pthread_barrier_t *child_barrier = smalloc(sizeof(*child_barrier));
	pthread_barrier_init(child_barrier, NULL, progresslist_count + 1); // +1 for manager thread
	
	pthread_barrier_t *progressbarrier = pdata->progressbarrier;
	const volatile bool *finished = pdata->finished;
	
	struct progressdata child_pdata = {
		pdata->datalock,
		child_barrier,
		pdata->data,
		finished
	};
	pthread_t progressors[progresslist_count];
	for (int i = 0; i < progresslist_count; ++i) {
		debug(-1, "starting thread #%d with function %p\n", i, progresslist[i]);
		pthread_create(&progressors[i], NULL, progresslist[i], &child_pdata);
	}
	
	while (!*finished) {
		pthread_barrier_wait(progressbarrier);
		pthread_barrier_wait(child_barrier);
		
		
		pthread_barrier_wait(child_barrier); // ensure rdlock
		pthread_barrier_wait(progressbarrier); // ensure rdlock
	}
	debug_0;
	for (int i = 0; i < progresslist_count; ++i) {
	debug_0;
		pthread_join(progressors[i], NULL);
	}
	debug_0;
	return NULL;
}

void *progress_file(void *pdata_) {
	struct progressdata *pdata = (struct progressdata *) pdata_;
	debug_0;
	pthread_rwlock_t *datalock = pdata->datalock;
	pthread_barrier_t *progressbarrier = pdata->progressbarrier;
	const struct pnmdata *const data = pdata->data;
	const volatile bool *finished = pdata->finished;
	int step_count = 0;
	while (!*finished) {
		pthread_barrier_wait(progressbarrier);
		
		pthread_rwlock_rdlock(datalock);
		pthread_barrier_wait(progressbarrier); // ensure rdlock
		if (step_count % progress_interval == 0) {
			fwritepnm(data, progressfile);
		}
		pthread_rwlock_unlock(datalock);
		++step_count;
	}
	pthread_rwlock_rdlock(datalock);
	for (int i = 0; i < extras; ++i) {
		fwritepnm(data, progressfile);
	}
	pthread_rwlock_unlock(datalock);
	debug_0;
	return NULL;
}

void *progress_text(void *pdata_) {
	struct progressdata *pdata = (struct progressdata *) pdata_;
	
	pthread_rwlock_t *datalock = pdata->datalock;
	pthread_barrier_t *progressbarrier = pdata->progressbarrier;
	const struct pnmdata *const data = pdata->data;
	const volatile bool *finished = pdata->finished;
	int step_count = 0;
	int output_count = 0;
	int size = data->dimx * data->dimy;
	printf("\n");
	while (!*finished) {
		pthread_barrier_wait(progressbarrier);
		debug_0;
		pthread_barrier_wait(progressbarrier); // ensure rdlock
		if (step_count % progress_interval == 0) {
			fprintf(textfile, "\x1b[AApproximately %4.1lf%% done (%d, %d).\x1b[0K\n", 100 * (double)step_count * workercount / (double)size, progress_interval, step_count);
			fflush(textfile);
			++output_count;;
		}
		++step_count;
		debug_0;
	}
		debug_0;
	fprintf(textfile, "Finished\x1b[0K\n");
	fflush(textfile);
	debug_0;
	return NULL;
}

#ifdef SDL_PROGRESS
void *progress_sdl2(void *pdata_) {
	struct progressdata *pdata = (struct progressdata *) pdata_;
	
	pthread_rwlock_t *datalock = pdata->datalock;
	pthread_barrier_t *progressbarrier = pdata->progressbarrier;
	const struct pnmdata *const data = pdata->data;
	int dimx = data->dimx, dimy = data->dimy, depth = data->depth;
	double (*rawdata)[dimx][depth] = scalloc(dimy, sizeof(*rawdata)); // use memcpy each time
	const volatile bool *finished = pdata->finished;
	int step_count = 0;
	
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_rwlock_t datacopylock;
	pthread_rwlock_init(&datacopylock, NULL);
	bool sdl_finished = false;
	
	struct sdlhelperdata sdata = {
		&cond,
		&mutex,
		&datacopylock,
		&sdl_finished,
		dimx,
		dimy,
		depth,
		(double*)rawdata
	};
	
	pthread_t helper;
	
	pthread_create(&helper, NULL, progress_sdl2_helper, &sdata);
	
	
	while (!*finished) {
		pthread_barrier_wait(progressbarrier);
		pthread_rwlock_rdlock(datalock);
		pthread_barrier_wait(progressbarrier); // ensure rdlock
		if (step_count % progress_interval == 0) {
			pthread_rwlock_wrlock(&datacopylock);
			memcpy(rawdata, data->rawdata, dimy*sizeof(*rawdata));
			pthread_rwlock_unlock(datalock);
			pthread_rwlock_unlock(&datacopylock);
			pthread_cond_signal(&cond);
		}
		else {
			pthread_rwlock_unlock(datalock);
		}
		++step_count;
	}
	pthread_rwlock_rdlock(datalock);
	pthread_rwlock_wrlock(&datacopylock);
	memcpy(rawdata, data->rawdata, dimy*sizeof(*rawdata));
	pthread_rwlock_unlock(&datacopylock);
	pthread_rwlock_unlock(datalock);
	
	sdl_finished = true;
	pthread_cond_signal(&cond);
	
	debug_0;
	pthread_join(helper, NULL);
	return NULL;
}

void *progress_sdl2_helper(void *sdata_) {
	struct sdlhelperdata *sdata = (struct sdlhelperdata*) sdata_;
	pthread_cond_t *cond = sdata->cond;
	pthread_mutex_t *mutex = sdata->mutex;
	pthread_rwlock_t *datalock = sdata->datalock;
	const volatile bool *finished = sdata->finished;
	int dimx = sdata->dimx;
	int dimy = sdata->dimy;
	int depth = sdata->depth;
	double (*rawdata)[dimx][depth] = (double(*)[dimx][depth]) sdata->rawdata_;
	
	
	SDL_Window *window = NULL;
	SDL_Surface *surface = NULL;
	
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL could not initialize: SDL_Error: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	window = SDL_CreateWindow("Image Generator (SDL2)", sdl_pos[0], sdl_pos[1], dimx, dimy, SDL_WINDOW_SHOWN);
	if (window == NULL) {
		fprintf(stderr, "SDL could not create window: SDL_Error: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	surface = SDL_GetWindowSurface(window);
	int pitch = surface->pitch;
	
	Uint32 (*pixelarr)[pitch/sizeof(Uint32)] = (Uint32(*)[pitch]) surface->pixels;
	SDL_PixelFormat *pixelformat = surface->format;
	debug(-2, "\n\n");
	while (!*finished) {
		struct timespec abstime;
		clock_gettime(CLOCK_REALTIME, &abstime);
		if (abstime.tv_nsec >= 80000000) abstime.tv_sec++, abstime.tv_nsec -= 80000000;
		else abstime.tv_nsec += 20000000;
		if (pthread_cond_timedwait(cond, mutex, &abstime) == 0) {
			pthread_rwlock_rdlock(datalock);
			// Output here // maybe eventually only update changed pixels with help from generate.{c,h}
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
				}
			}
			pthread_rwlock_unlock(datalock);
		}
		SDL_UpdateWindowSurface(window);
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				debug(-1, "Quitting\n");
				exit(EXIT_FAILURE);
			}
			else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				SDL_KeyboardEvent *ev = (SDL_KeyboardEvent*) &event;
				if (
					(
						ev->keysym.sym == SDLK_ESCAPE ||
						ev->keysym.sym == SDLK_q
					)
					&& ev->state == SDL_PRESSED
				) {
					debug(-1, "Quitting\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	pthread_rwlock_rdlock(datalock);
	// Output here // maybe eventually only update changed pixels with help from generate.{c,h}
	for (int y = 0; y < dimy; ++y) {
		for (int x = 0; x < dimx; ++x) {
			pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
		}
	}
	pthread_rwlock_unlock(datalock);
	if (sdl_wait_time < 0) {
		bool waiting = true;
		while(waiting) {
			SDL_Event event;
			while (waiting && SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					waiting = false;
				}
			}
			SDL_UpdateWindowSurface(window);
			SDL_Delay(20);
		}
	}
	else {
		bool waiting = true;
		for (int i = 0; waiting && i < sdl_wait_time; ++i) {
			for (int j = 0; waiting && j < 1000/20; ++j) {
				SDL_Event event;
				while (waiting && SDL_PollEvent(&event)) {
					if (event.type == SDL_QUIT) {
						waiting = false;
					}
					else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
						SDL_KeyboardEvent *ev = (SDL_KeyboardEvent*) &event;
						if (
							(
								ev->keysym.sym == SDLK_ESCAPE ||
								ev->keysym.sym == SDLK_q
							)
							&& ev->state == SDL_PRESSED
						) {
							waiting = false;
						}
					}
				}
				SDL_UpdateWindowSurface(window);
				SDL_Delay(20);
			}
		}
	}
	debug_0;
	return NULL;
}
#endif // def SDL_PROGRESS



