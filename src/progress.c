
#include "progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <libgen.h>
#include <string.h>
#include <stdalign.h>

#ifdef SDL_PROGRESS
#include <SDL2/SDL.h>
#include <sys/time.h>
#endif // def SDL_PROGRESS

#ifdef FRAMEBUFFER_PROGRESS
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <fcntl.h>
#endif // def FRAMEBUFFER_PROGRESS

#include "safelib.h"
#include "generate_common.h" // workercount
#include "pnmlib.h"
#include "setup.h" // dimx, dimy
#include "debug.h"
#include "color.h" // color_t

static int extras = 0;

void *(*progress)(void *pdata_) = NULL;
static void *(**progresslist)(void *pdata_) = NULL;
static int progresslist_count = 0;
static int progress_interval = -1;

static struct pnmdata *mask = NULL;

void *no_progress(void *pdata_);
void *progress_manager(void *pdata_);
void *progress_file(void *pdata_);
void *progress_text(void *pdata_);
#ifdef FRAMEBUFFER_PROGRESS
void *progress_framebuffer(void *pdata_);
void *progress_framebuffer_helper(void *pdata_);
#endif // def FRAMEBUFFER_PROGRESS
#ifdef SDL_PROGRESS
void *progress_sdl2(void *pdata_);
void *progress_sdl2_helper(void *graphicshelperdata_);
void *progress_sdl2_helper_scaled(void *graphicshelperdata_);
static int sdl_pos[2] = {SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED};
static int sdl_scale[2] = {1, 1};
#endif // def SDL_PROGRESS

struct graphicshelperdata {
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    pthread_rwlock_t *datalock;
    const volatile bool *finished;
    int dimx;
    int dimy;
    int depth;
    const color_t *rawdata_;
};

static int end_wait_time = 0;

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
        case 'dP':
            if (progressfile != NULL || progressfile_defaultname) {
                fprintf(stderr, "Repeated progress file '%s'.\n", optarg);
                exit(EXIT_FAILURE);
            }
            else {
                progressfile_defaultname = true;
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
        case 'mask':
            if (mask != NULL) {
                fprintf(stderr, "Mask file already specified (duplicate: %s).\n", optarg);
                exit(EXIT_FAILURE);
            }
            FILE *maskfile = fopen(optarg, "rb");
            if (maskfile == NULL) {
                fprintf(stderr, "Invalid mask file: %s.\n", optarg);
                exit(EXIT_FAILURE);
            }
            mask = allocpnm();
            if (!freadpnm(mask, maskfile)) {
                fprintf(stderr, "Could not read progressmask file.\n");
                exit(EXIT_FAILURE);
            }
            fclose(maskfile);
            if (dimx == -1) dimx = mask->dimx;
            if (dimx != mask->dimx) {
                fprintf(stderr, "Progress mask file must have same size as image generated (%dx%d specified, %dx%d mask)\n", dimx, dimy, mask->dimx, mask->dimy);
                exit(EXIT_FAILURE);
            }
            if (dimy == -1) dimy = mask->dimy;
            if (dimy != mask->dimy) {
                fprintf(stderr, "Progress mask file must have same size as image generated (%dx%d specified, %dx%d mask)\n", dimx, dimy, mask->dimx, mask->dimy);
                exit(EXIT_FAILURE);
            }
            break;
    #ifdef SDL_PROGRESS
        case 'SDL':
            add_progressor(progress_sdl2);
            break;
        case 'pos':
            ret = sscanf(optarg, "%d,%d%n", &sdl_pos[0], &sdl_pos[1], &count);
            if (ret != 2 || sdl_pos[0] < 0 || sdl_pos[1] < 0 || optarg[count] != 0) {
                fprintf(stderr, "Invalid position: '%s'.\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'scal':
            ret = sscanf(optarg, "%d%*c%d%n", &sdl_scale[0], &sdl_scale[1], &count);
            if (ret != 2 || sdl_scale[0] < 1 || sdl_scale[1] < 1 || optarg[count] != 0) {
                fprintf(stderr, "Invalid scale: '%s'.\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
    #endif // def SDL_PROGRESS
    #ifdef FRAMEBUFFER_PROGRESS
        case 'fram':
            add_progressor(progress_framebuffer);
            break;
    #endif // def FRAMEBUFFER_PROGRESS
        case 'wait':
            if (optarg == NULL) {
                end_wait_time = -1; // indefinitely
            }
            else if (sscanf(optarg, "%d", &end_wait_time) != 1) {
                fprintf(stderr, "Invalid wait time: %s.\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            return false;
    }
    return true;
}

void progress_finalize(const char *progname_, int dimx, int dimy, unsigned int seed, int depth) {
    if (colorcount < 0) // would be in generate_finalize but we need it initialized here
        colorcount = 1;
    if (progress == NULL || progressfile_defaultname) { // No progress was requested, use default file output
        debug(-3, "%p %d\n", progress, progressfile_defaultname);
        char *progname = strdup(progname_);
        debug(-3, "progress == %p\n", progress);
        char *tempstr = smalloc(128);
        char *base = basename(progname);
        snprintf(tempstr, 128, "progress_%s_%dx%d_%u.p%cm", base, dimx, dimy, seed, depth == 1 ? 'g' : 'p');
        free(progname);
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
    //debug(-1, "%d %d\n", workercount, progress_interval);
    if (progress_interval < 1) {
        progress_interval = 256;
    }
    progress_interval /= colorcount;
    if (progress_interval < 1) progress_interval = 1;
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
            (void)data;
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
    const volatile int *edgecount = pdata->edgecount;
    volatile bool child_finished = false;
    
    struct progressdata child_pdata = {
        pdata->datalock,
        child_barrier,
        pdata->data,
        &child_finished,
        edgecount,
    };
    pthread_t progressors[progresslist_count];
    for (int i = 0; i < progresslist_count; ++i) {
        debug(-1, "starting thread #%d with function %p\n", i, progresslist[i]);
        pthread_create(&progressors[i], NULL, progresslist[i], &child_pdata);
    }
    
    while (!*finished) {
        debug(1, "%s\n", *finished ? "finished" : "not finished");
        pthread_barrier_wait(progressbarrier);
        pthread_barrier_wait(child_barrier);
        debug_1;
        
        pthread_barrier_wait(child_barrier); // ensure rdlock
        pthread_barrier_wait(progressbarrier); // ensure rdlock
        debug(1, "%s\n", *finished ? "finished" : "not finished");
    }
    debug_1;
    pthread_barrier_wait(child_barrier);
    child_finished = true;
    pthread_barrier_wait(child_barrier);

    debug_0;
    for (int i = 0; i < progresslist_count; ++i) {
    debug_0;
        pthread_join(progressors[i], NULL);
    }
    debug_0;
    return NULL;
}

void *progress_file_masked(void *pdata_);

void *progress_file(void *pdata_) {
    if (mask != NULL) {
        return progress_file_masked(pdata_);
    }
    struct progressdata *pdata = (struct progressdata *) pdata_;
    debug_0;
    pthread_rwlock_t *datalock = pdata->datalock;
    pthread_barrier_t *progressbarrier = pdata->progressbarrier;
    const struct pnmdata *const data = pdata->data;
    const volatile bool *finished = pdata->finished;
    int step_count = 0;
    debug_0;
    while (!*finished) {
        pthread_barrier_wait(progressbarrier);
    debug_0;
        
        pthread_rwlock_rdlock(datalock);
        pthread_barrier_wait(progressbarrier); // ensure rdlock
        if (step_count % progress_interval == 0) {
            fwritepnm(data, progressfile);
        }
        pthread_rwlock_unlock(datalock);
        ++step_count;
    debug_0;
    }
    pthread_rwlock_rdlock(datalock);
    for (int i = 0; i < extras; ++i) {
        fwritepnm(data, progressfile);
    }
    pthread_rwlock_unlock(datalock);
    debug_0;
    return NULL;
}

void *progress_file_masked(void *pdata_) {
    struct progressdata *pdata = (struct progressdata *) pdata_;
    debug_0;
    pthread_rwlock_t *datalock = pdata->datalock;
    pthread_barrier_t *progressbarrier = pdata->progressbarrier;
    const struct pnmdata *const data = pdata->data;
    const color_t (*rawdata)[dimx] = (color_t (*)[dimx]) data->rawdata;
    const color_t (*maskdata)[dimx] = (color_t (*)[dimx]) mask->rawdata;
    int masked_depth = depth >= mask->depth ? depth : mask->depth;
    color_t (*masked_data)[dimx] = s_mm_malloc(dimy * sizeof(*masked_data), alignof(*masked_data));
    struct pnmdata masked = {dimx, dimy, data->maxval >= mask->maxval ? data->maxval : mask->maxval, masked_depth, data->commentcount, (color_t*) masked_data, data->comments, NULL};
    const volatile bool *finished = pdata->finished;
    int step_count = 0;
    debug_0;
    while (!*finished) {
        pthread_barrier_wait(progressbarrier);
        
        pthread_rwlock_rdlock(datalock);
        pthread_barrier_wait(progressbarrier); // ensure rdlock
        if (step_count % progress_interval == 0) {
            for (int y = 0; y < dimy; ++y) {
                for (int x = 0; x < dimx; ++x) {
                    masked_data[y][x] = color_product(rawdata[y][x], maskdata[y][x]);
                }
            }
            pthread_rwlock_unlock(datalock);
            fwritepnm(&masked, progressfile);
        }
        else {
            pthread_rwlock_unlock(datalock);
        }
        ++step_count;
    }
    debug_0;
    pthread_rwlock_rdlock(datalock);
    debug_0;
    for (int y = 0; y < dimy; ++y) {
        for (int x = 0; x < dimx; ++x) {
            masked_data[y][x] = color_product(rawdata[y][x], maskdata[y][x]);
        }
    }
    for (int i = 0; i < extras; ++i) {
        fwritepnm(&masked, progressfile);
    }
    pthread_rwlock_unlock(datalock);
    debug_0;
    return NULL;
}

void *progress_text(void *pdata_) {
    struct progressdata *pdata = (struct progressdata *) pdata_;
    
    //pthread_rwlock_t *datalock = pdata->datalock; // unused in this function
    pthread_barrier_t *progressbarrier = pdata->progressbarrier;
    const struct pnmdata *const data = pdata->data;
    const volatile bool *finished = pdata->finished;
    const volatile int *edgecount = pdata->edgecount;
    int step_count = 0;
    int output_count = 0;
    int size = data->dimx * data->dimy;
    fprintf(textfile, "\n");
    while (!*finished) {
        pthread_barrier_wait(progressbarrier);
        debug_0;
        pthread_barrier_wait(progressbarrier); // ensure rdlock
        if (step_count % progress_interval == 0) {
            fprintf(textfile, "\x1b[AApproximately %4.1lf%% done (%d, %d, %d).\x1b[0K\n", 100 * (double)step_count * colorcount / (double)size, progress_interval, step_count, *edgecount);
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
    color_t (*rawdata)[dimx] = s_mm_malloc(dimy * sizeof(*rawdata), alignof(*rawdata)); // use memcpy each time
    const volatile bool *finished = pdata->finished;
    int step_count = 0;
    
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_rwlock_t datacopylock;
    pthread_rwlock_init(&datacopylock, NULL);
    bool sdl_finished = false;
    
    struct graphicshelperdata gdata = {
        &cond,
        &mutex,
        &datacopylock,
        &sdl_finished,
        dimx,
        dimy,
        depth,
        (color_t*)rawdata
    };
    
    pthread_t helper;
    
    if (sdl_scale[0] == 1 && sdl_scale[1] == 1) {
        pthread_create(&helper, NULL, progress_sdl2_helper, &gdata);
    }
    else {
        pthread_create(&helper, NULL, progress_sdl2_helper_scaled, &gdata);
    }
    
    
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

void *progress_sdl2_helper(void *gdata_) {
    struct graphicshelperdata *gdata = (struct graphicshelperdata*) gdata_;
    pthread_cond_t *cond = gdata->cond;
    pthread_mutex_t *mutex = gdata->mutex;
    pthread_rwlock_t *datalock = gdata->datalock;
    const volatile bool *finished = gdata->finished;
    int dimx = gdata->dimx;
    int dimy = gdata->dimy;
    int depth = gdata->depth;
    color_t (*rawdata)[dimx] = (color_t(*)[dimx]) gdata->rawdata_;
    
    
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
    
    
    struct timespec abstime;
    
    while (!*finished) {
        clock_gettime(CLOCK_REALTIME, &abstime);
        if (abstime.tv_nsec >= 80000000) abstime.tv_sec++, abstime.tv_nsec -= 80000000;
        else abstime.tv_nsec += 20000000;
        if (pthread_cond_timedwait(cond, mutex, &abstime) == 0) {
            pthread_rwlock_rdlock(datalock);
            // Output here // maybe eventually only update changed pixels with help from generate.{c,h}
            if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        pixelarr[y][x] = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(rawdata[y][x], 0) * 255,
                            color_get_channel(rawdata[y][x], 1) * 255,
                            color_get_channel(rawdata[y][x], 2) * 255
                        );
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0)*255;
                        pixelarr[y][x] = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t masked = color_product(rawdata[y][x], ((color_t(*)[dimx])(mask->rawdata))[y][x]);
                        pixelarr[y][x] = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(masked, 0) * 255,
                            color_get_channel(masked, 1) * 255,
                            color_get_channel(masked, 2) * 255
                        );
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t masked = color_product(rawdata[y][x], color_set1(color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0)));
                        pixelarr[y][x] = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(masked, 0) * 255,
                            color_get_channel(masked, 1) * 255,
                            color_get_channel(masked, 2) * 255
                        );
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t masked = color_product(((color_t(*)[dimx])(mask->rawdata))[y][x], color_set1(color_get_channel(rawdata[y][x], 0)));
                        pixelarr[y][x] = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(masked, 0) * 255,
                            color_get_channel(masked, 1) * 255,
                            color_get_channel(masked, 2) * 255
                        );
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0) * 255 * color_get_channel(((color_t (*)[dimx])(mask->rawdata))[y][x], 0);
                        pixelarr[y][x] = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
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
    if (depth == 3 && mask == NULL) {
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                pixelarr[y][x] = SDL_MapRGB(
                    pixelformat,
                    color_get_channel(rawdata[y][x], 0) * 255,
                    color_get_channel(rawdata[y][x], 1) * 255,
                    color_get_channel(rawdata[y][x], 2) * 255
                );
            }
        }
    }
    else if (depth == 1 && mask == NULL) {
        double tmp;
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                tmp = color_get_channel(rawdata[y][x], 0)*255;
                pixelarr[y][x] = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
            }
        }
    }
    else if (depth == 3 && mask->depth == 3) {
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                color_t masked = color_product(rawdata[y][x], ((color_t(*)[dimx])(mask->rawdata))[y][x]);
                pixelarr[y][x] = SDL_MapRGB(
                    pixelformat,
                    color_get_channel(masked, 0) * 255,
                    color_get_channel(masked, 1) * 255,
                    color_get_channel(masked, 2) * 255
                );
            }
        }
    }
    else if (depth == 3 && mask->depth == 1) {
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                color_t masked = color_product(rawdata[y][x], color_set1(color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0)));
                pixelarr[y][x] = SDL_MapRGB(
                    pixelformat,
                    color_get_channel(masked, 0) * 255,
                    color_get_channel(masked, 1) * 255,
                    color_get_channel(masked, 2) * 255
                );
            }
        }
    }
    else if (depth == 1 && mask->depth == 3) {
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                color_t masked = color_product(((color_t(*)[dimx])(mask->rawdata))[y][x], color_set1(color_get_channel(rawdata[y][x], 0)));
                pixelarr[y][x] = SDL_MapRGB(
                    pixelformat,
                    color_get_channel(masked, 0) * 255,
                    color_get_channel(masked, 1) * 255,
                    color_get_channel(masked, 2) * 255
                );
            }
        }
    }
    else if (depth == 1 && mask->depth == 1) {
        double tmp;
        for (int y = 0; y < dimy; ++y) {
            for (int x = 0; x < dimx; ++x) {
                tmp = color_get_channel(rawdata[y][x], 0) * 255 * color_get_channel(((color_t (*)[dimx])(mask->rawdata))[y][x], 0);
                pixelarr[y][x] = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
            }
        }
    }
    else {
        debug(-1, "Logic error.\n");
        exit(EXIT_FAILURE);
    }
    pthread_rwlock_unlock(datalock);
    if (end_wait_time < 0) {
        bool waiting = true;
        while(waiting) {
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
    else {
        bool waiting = true;
        for (int i = 0; waiting && i < end_wait_time; ++i) {
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

void *progress_sdl2_helper_scaled(void *gdata_) {
    struct graphicshelperdata *gdata = (struct graphicshelperdata*) gdata_;
    pthread_cond_t *cond = gdata->cond;
    pthread_mutex_t *mutex = gdata->mutex;
    pthread_rwlock_t *datalock = gdata->datalock;
    const volatile bool *finished = gdata->finished;
    int dimx = gdata->dimx;
    int dimy = gdata->dimy;
    int depth = gdata->depth;
    color_t (*rawdata)[dimx] = (color_t(*)[dimx]) gdata->rawdata_;
    
    
    SDL_Window *window = NULL;
    SDL_Surface *surface = NULL;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize: SDL_Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    window = SDL_CreateWindow("Image Generator (SDL2)", sdl_pos[0], sdl_pos[1], dimx*sdl_scale[0], dimy*sdl_scale[1], SDL_WINDOW_SHOWN);
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
            if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0)*255;
                        Uint32 temp = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = color_set1(color_get_channel(((color_t(*)[dimx])mask->rawdata)[y][x], 0));
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_set1(color_get_channel(rawdata[y][x], 0)*255.0);
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0)*255*color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0);
                        Uint32 temp = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
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
    if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0)*255;
                        Uint32 temp = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = color_set1(color_get_channel(((color_t(*)[dimx])mask->rawdata)[y][x], 0));
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_set1(color_get_channel(rawdata[y][x], 0)*255.0);
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        Uint32 temp = SDL_MapRGB(
                            pixelformat,
                            color_get_channel(color, 0),
                            color_get_channel(color, 1),
                            color_get_channel(color, 2)
                        );
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                double tmp;
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        tmp = color_get_channel(rawdata[y][x], 0)*255*color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0);
                        Uint32 temp = SDL_MapRGB(pixelformat, tmp, tmp, tmp);
                        for (int yy = 0; yy < sdl_scale[1]; ++yy) {
                            for (int xx = 0; xx < sdl_scale[0]; ++xx) {
                                pixelarr[y*sdl_scale[1]+yy][x*sdl_scale[0]+xx] = temp;
                            }
                        }
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
            }
    pthread_rwlock_unlock(datalock);
    if (end_wait_time < 0) {
        bool waiting = true;
        while(waiting) {
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
                        debug(-1, "Quitting\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            SDL_UpdateWindowSurface(window);
            SDL_Delay(20);
        }
    }
    else {
        bool waiting = true;
        for (int i = 0; waiting && i < end_wait_time; ++i) {
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



#ifdef FRAMEBUFFER_PROGRESS
void *progress_framebuffer(void *pdata_) {
    struct progressdata *pdata = (struct progressdata *) pdata_;
    
    pthread_rwlock_t *datalock = pdata->datalock;
    pthread_barrier_t *progressbarrier = pdata->progressbarrier;
    const struct pnmdata *const data = pdata->data;
    int dimx = data->dimx, dimy = data->dimy, depth = data->depth;
    color_t (*rawdata)[dimx] = s_mm_malloc(dimy * sizeof(*rawdata), alignof(*rawdata)); // use memcpy each time
    const volatile bool *finished = pdata->finished;
    int step_count = 0;
    
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_rwlock_t datacopylock;
    pthread_rwlock_init(&datacopylock, NULL);
    bool framebuffer_finished = false;
    
    struct graphicshelperdata gdata = {
        &cond,
        &mutex,
        &datacopylock,
        &framebuffer_finished,
        dimx,
        dimy,
        depth,
        (color_t*)rawdata
    };
    
    pthread_t helper;
    
    pthread_create(&helper, NULL, progress_framebuffer_helper, &gdata);
    
    
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
    
    
    framebuffer_finished = true;
    pthread_cond_signal(&cond);

    
    debug_0;
    pthread_join(helper, NULL);
    return NULL;
}

void *progress_framebuffer_helper(void *gdata_) {
    struct graphicshelperdata *gdata = (struct graphicshelperdata*) gdata_;
    pthread_cond_t *cond = gdata->cond;
    pthread_mutex_t *mutex = gdata->mutex;
    pthread_rwlock_t *datalock = gdata->datalock;
    const volatile bool *finished = gdata->finished;
    int dimx = gdata->dimx;
    int dimy = gdata->dimy;
    int depth = gdata->depth;
    color_t (*rawdata)[dimx] = (color_t(*)[dimx]) gdata->rawdata_;
    
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    char *fbp = NULL;
    
    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd) {
        fprintf(stderr, "Error: cannot open framebuffer device.\n");
        exit(EXIT_FAILURE);
    }
    
    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        fprintf(stderr, "Error reading fixed information.\n");
        exit(EXIT_FAILURE);
    }
    
    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        fprintf(stderr, "Error reading variable information.\n");
        exit(EXIT_FAILURE);
    }
    // map framebuffer to user memory 
    screensize = finfo.smem_len;
    
    fbp = mmap(
        NULL,
        screensize, 
        PROT_READ | PROT_WRITE, 
        MAP_SHARED, 
        fbfd,
        0
    );
    unsigned char (*pixels)[finfo.line_length/4][4] = (unsigned char(*)[finfo.line_length/4][4]) fbp;
    //int cx = vinfo.xres / 2;
    //int cy = vinfo.yres / 2;
    
    if (dimx > vinfo.xres_virtual) {
        fprintf(stderr, "Image too wide for framebuffer (%d > %d).\n", dimx, vinfo.xres_virtual);
        exit(EXIT_FAILURE);
    }
    if (dimy > vinfo.yres_virtual) {
        fprintf(stderr, "Image too tall for framebuffer (%d > %d).\n", dimy, vinfo.yres_virtual);
        exit(EXIT_FAILURE);
    }
    
    while (!*finished) {
        struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        if (abstime.tv_nsec >= 80000000) abstime.tv_sec++, abstime.tv_nsec -= 80000000;
        else abstime.tv_nsec += 20000000;
        if (pthread_cond_timedwait(cond, mutex, &abstime) == 0) {
            pthread_rwlock_rdlock(datalock);
            // Output here // maybe eventually only update changed pixels with help from generate.{c,h}
            if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = color_get_channel(rawdata[y][x], 0)*255;
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(color_set1(color_get_channel(rawdata[y][x], 0)), color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) { 
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = color_set1(color_get_channel(((color_t(*)[dimx])mask->rawdata)[y][x], 0));
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = 
                            color_get_channel(rawdata[y][x], 0)*255*color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0);
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
            }
            pthread_rwlock_unlock(datalock);
        }
    }
    pthread_rwlock_rdlock(datalock);
    // Output here // maybe eventually only update changed pixels with help from generate.{c,h}
    pthread_rwlock_unlock(datalock);
    if (end_wait_time <= 0) { // add == 0 to reduce code
        bool waiting = (end_wait_time < 0);
        do {
            if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = color_get_channel(rawdata[y][x], 0)*255;
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(color_set1(color_get_channel(rawdata[y][x], 0)), color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) { 
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = color_set1(color_get_channel(((color_t(*)[dimx])mask->rawdata)[y][x], 0));
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = 
                            color_get_channel(rawdata[y][x], 0)*255*color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0);
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
            }
            usleep(20000);
        } while (waiting);
    }
    else {
        bool waiting = true;
        for (int i = 0; waiting && i < end_wait_time; ++i) {
            for (int j = 0; waiting && j < 1000/20; ++j) {
                if (depth == 3 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask == NULL) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = color_get_channel(rawdata[y][x], 0)*255;
                    }
                }
            }
            else if (depth == 3 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 3) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(color_set1(color_get_channel(rawdata[y][x], 0)), color_set1(255.0));
                        color_t mask_color = ((color_t(*)[dimx])mask->rawdata)[y][x];
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 3 && mask->depth == 1) { 
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        color_t color = color_product(rawdata[y][x], color_set1(255.0));
                        color_t mask_color = color_set1(color_get_channel(((color_t(*)[dimx])mask->rawdata)[y][x], 0));
                        color = color_product(color, mask_color);
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = color_get_channel(color, 2); // BGR pixel order
                        pixels[y][x][1] = color_get_channel(color, 1); // BGR pixel order
                        pixels[y][x][2] = color_get_channel(color, 0); // BGR pixel order
                    }
                }
            }
            else if (depth == 1 && mask->depth == 1) {
                for (int y = 0; y < dimy; ++y) {
                    for (int x = 0; x < dimx; ++x) {
                        //pixelarr[y][x] = SDL_MapRGB(pixelformat, rawdata[y][x][0]*255, rawdata[y][x][1]*255, rawdata[y][x][2]*255);
                        pixels[y][x][0] = pixels[y][x][1] = pixels[y][x][2] = 
                            color_get_channel(rawdata[y][x], 0)*255*color_get_channel(((color_t(*)[dimx])(mask->rawdata))[y][x], 0);
                    }
                }
            }
            else {
                debug(-1, "Logic error.\n");
                exit(EXIT_FAILURE);
            }
                usleep(20000);
            }
        }
    }
    debug_0;
    return NULL;
}
#endif // def FRAMEBUFFER_PROGRESS
