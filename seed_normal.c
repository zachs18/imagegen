#include "seed.h"

#include <stdbool.h>

#include "setup.h"
#include "generate_common.h"
#include "color.h"
#include "safelib.h"
#include "pnmlib.h"
#include "randint.h"
#include "debug.h"

struct floodplane {
	struct pixel *pixels;
	int count;
};

static struct floodplane *floodplanes = NULL;
int floodplanecount = 0;

static int seed_floodplane(struct floodplane *floodplane, struct pnmdata *data, bool *used_, int seedcount);
static struct pixel *flood_recursive(int dimx, int dimy, int cx, int cy, struct pixel *pixels, bool *filled_, bool *blocked_);
static int flood_iterative(int dimx, int dimy, int cx, int cy, struct pixel *pixels, bool *filled_, bool *blocked_);

int seed_image_naive(struct pnmdata *data, bool *used_, int seedcount) { // Unused
	int dimx = data->dimx, dimy = data->dimy, depth = data->depth;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	double (*values)[dimx][depth] = (double(*)[dimx][depth]) data->rawdata;
	for (int i = 0; i < seedcount; ++i) {
		int x = randint(0, dimx);
		int y = randint(0, dimy);
		if (used[y][x]) {
			--i;
			continue;
		}
		used[y][x] = true;
		new_color(values[y][x]);
	}
	return seedcount;
}

int seed_image(struct pnmdata *data, bool *used_, int seedcount) {
	if (seedcount > 0) {
		//if (seedcount < floodplanecount) seedcount = floodplanecount;
		// so we don't have to deal with re-seeding ever
		// Actually, with a black/whitelist we might still need to reseed
		int realseedcount = 0;
		while (realseedcount < seedcount) {
			debug_1;
			for (int i = 0; i < floodplanecount; ++i) {
				int try = (seedcount - realseedcount) / (floodplanecount - i); // never x/0
				realseedcount += seed_floodplane(&floodplanes[i], data, used_, try);
			}
			for (int i = 0; i < floodplanecount; ++i) {
				int try = (seedcount - realseedcount) / (floodplanecount - i); // never x/0
				realseedcount += seed_floodplane(&floodplanes[floodplanecount-i-1], data, used_, try);
			}
		}
		return realseedcount;
	}
	return 0;
}

static int seed_floodplane(struct floodplane *floodplane, struct pnmdata *data, bool *used_, int seedcount) {
	int dimx = data->dimx, dimy = data->dimy;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	double (*values)[dimx][depth] = (double(*)[dimx][depth]) data->rawdata;
	if (seedcount < 0); // pass
	else if (seedcount < floodplane->count) {
		for (int i = 0; i < seedcount; ++i) {
			int r = randint(0, floodplane->count);
			int x = floodplane->pixels[r].x, y = floodplane->pixels[r].y;
			if (used[y][x]) --i, --seedcount; // ignore, but signal that it failed
			else {
				new_color(values[y][x]);
				used[y][x] = true;
			}
		}
	}
	else { // fill the whole thing
		seedcount = floodplane->count;
		for (int i = 0; i < seedcount; ++i) {
			int x = floodplane->pixels[i].x, y = floodplane->pixels[i].y;
			if (used[y][x]) {
				--seedcount;
			}
			else {
				new_color(values[y][x]);
				used[y][x] = true;
			}
		}
	}
	return seedcount;
}

int compute_floodplanes(struct pnmdata *data, bool *blocked_) {
	int dimx = data->dimx, dimy = data->dimy;
	bool (*filled)[dimx] = (bool(*)[dimx]) scalloc(dimy, sizeof(*filled));
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	for (int x = 0; x < dimx; ++x) {
		for (int y = 0; y < dimy; ++y) {
			if (!filled[y][x] && !blocked[y][x]) {
				floodplanes = sreallocarray(floodplanes, ++floodplanecount, sizeof(*floodplanes));
				floodplanes[floodplanecount-1].pixels = scalloc(
					dimx*dimy,
					sizeof(struct pixel)
				); // allocate a buffer that is guaranteed to not overflow ...
				floodplanes[floodplanecount-1].count = flood_iterative(dimx, dimy, x, y, floodplanes[floodplanecount-1].pixels, (bool*) filled, blocked_);
				floodplanes[floodplanecount-1].pixels = sreallocarray(
					floodplanes[floodplanecount-1].pixels,
					floodplanes[floodplanecount-1].count,
					sizeof(struct pixel)
				); // ... then shrink the buffer so as not to waste too much memory
			}
		}
	}
	debug(1, "%d\n", floodplanecount);
	return floodplanecount;
}

static struct pixel *flood_recursive(int dimx, int dimy, int cx, int cy, struct pixel *pixels, bool *filled_, bool *blocked_) {
	// segfaults for images larger than ~300x300
	bool (*filled)[dimx] = (bool(*)[dimx]) filled_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	filled[cy][cx] = true;
	(*pixels).x = cx;
	(*pixels).y = cy;
	pixels++;
	for (int i = 0; i < floodoffsetcount; ++i) {
		int x = cx + floodoffsets[i].dx;
		int y = cy + floodoffsets[i].dy;
		if (x >= 0 && x < dimx && y >= 0 && y < dimy && !filled[y][x] && !blocked[y][x])
			pixels = flood_recursive(dimx, dimy, x, y, pixels, filled_, blocked_);
		x = cx - floodoffsets[i].dx;
		y = cy - floodoffsets[i].dy;
		if (x >= 0 && x < dimx && y >= 0 && y < dimy && !filled[y][x] && !blocked[y][x])
			pixels = flood_recursive(dimx, dimy, x, y, pixels, filled_, blocked_);
	}
	return pixels;
}

static int flood_iterative(int dimx, int dimy, int cx, int cy, struct pixel *pixels, bool *filled_, bool *blocked_) {
	bool (*filled)[dimx] = (bool(*)[dimx]) filled_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	filled[cy][cx] = true;
	(*pixels).x = cx;
	(*pixels).y = cy;
	int found = 1; // first found is cx, cy
	int x, y;
	for (int i = 0; i < found; ++i) {
		debug(4, "%d: %d,%d\t\t\t%d floodoffsets\n", found, pixels[i].x, pixels[i].y, floodoffsetcount);
		// for each pixel, add its adjacents to the list
		// the for loop will end when the last pixel in the list has
		//     no adjacents, i.e. flood fill success
		for (int j = 0; j < floodoffsetcount; ++j) {
			x = pixels[i].x + floodoffsets[j].dx;
			y = pixels[i].y + floodoffsets[j].dy;
			debug(4, "%d %d:%3d,%3d\n\t\t  +  +\n\t\t%3d,%3d\n\t\t  =  =\n\t\t%3d,%3d\n", found, j, pixels[i].x, pixels[i].y, floodoffsets[j].dx, floodoffsets[j].dy, x, y);
			if (x >= 0 && x < dimx && y >= 0 && y < dimy && !filled[y][x] && !blocked[y][x]) {
				pixels[found].x = x;
				pixels[found].y = y;
				found += 1;
				filled[y][x] = true;
				debug(4, "found %d %d\n", x, y);
			}
			x = pixels[i].x - floodoffsets[j].dx;
			y = pixels[i].y - floodoffsets[j].dy;
			debug(4, "%d %d:%3d,%3d\n\t\t  +  +\n\t\t%3d,%3d\n\t\t  =  =\n\t\t%3d,%3d\n", found, j, pixels[i].x, pixels[i].y, floodoffsets[j].dx, floodoffsets[j].dy, x, y);
			if (x >= 0 && x < dimx && y >= 0 && y < dimy && !filled[y][x] && !blocked[y][x]) {
				pixels[found].x = x;
				pixels[found].y = y;
				found += 1;
				filled[y][x] = true;
				debug(4, "found %d %d\n", x, y);
			}
		}
	}
	return found;
}