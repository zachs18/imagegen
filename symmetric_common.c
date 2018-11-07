#include "symmetric_common.h"

#include <stdlib.h>

#include "generate_symmetric.h"
#include "seed_symmetric.h"
#include "debug.h"


void get_canonical_pixel_symmetric(int x, int y, int *rx, int *ry) {
	if (sym_sharedcolumn) {
		debug(-20, "Shared columns are not suported yet; Make sure your dimensions are a multiple of your symmetry counts\n");
		exit(EXIT_FAILURE);
	}
	else {
		int xblock = x / sym_maxx;
		x = x % sym_maxx;
		if (xblock % 2 == 1 && sym_h_hflip) { // Horizontal flip between horizontal adjacent sections
			x = sym_maxx - x - 1;
		}
		if (xblock % 2 == 1 && sym_h_vflip) { // Vertical flip between horizontal adjacent sections
			int yblock = y / sym_maxy;
			y = y % sym_maxy;
			y = (yblock+1)*sym_maxy - y - 1;
		}
	}
	
	if (sym_sharedrow) {
		debug(-20, "Shared columns are not suported yet; Make sure your dimensions are a multiple of your symmetry counts\n");
		exit(EXIT_FAILURE);
	}
	else {
		int yblock = y / sym_maxy;
		y = y % sym_maxy;
		if (yblock % 2 == 1 && sym_v_vflip) { // Vertical flip between vertical adjacent sections
			y = sym_maxy - y - 1;
		}
		if (yblock % 2 == 1 && sym_v_hflip) { // Horizontal flip between vertical adjacent sections
			//int xblock = x / sym_maxx; // x is already in [0,sym_maxx)
			//x = x % sym_maxx;
			//x = (xblock+1)*sym_maxx - x - 1;
			x = sym_maxx - x - 1;
		}
	}
}

int place_pixel_symmetric(struct pnmdata *data, bool *used_, int x, int y, double *color);
