#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "safelib.h"
#include "pnmlib.h"
#include "debug.h"
#include "setup.h" // dimx, dimy

typedef enum {
	WHITELIST,
	BLACKLIST,
	NONETYPE,
} listtype_t;

static struct pnmdata *input = NULL;
static struct pnmdata *list = NULL;
static listtype_t listtype = NONETYPE;
static struct pnmdata *background = NULL;



bool input_option(int c, char *optarg) {
	//int ret;
	switch (c) {
		case 'i':
			(void)0; // label cannot be part of a declaration
			FILE *infile = NULL;
			if (input != NULL) {
				fprintf(stderr, "Repeated input file '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			else if (!strcmp("-", optarg)) {
				infile = stdin;
			}
			else {
				infile = fopen(optarg, "rb");
				if (infile == NULL) {
					fprintf(stderr, "Could not open input file '%s'.\n", optarg);
					exit(EXIT_FAILURE);
				}
			}
			input = allocpnm();
			if (!freadpnm(input, infile)) {
				fprintf(stderr, "Failed to read input file.\n");
				exit(EXIT_FAILURE);
			}
			fclose(infile);
			if (dimx != -1 && dimx < input->dimx) {
				fprintf(stderr, "Input file must fit in image (%dx%d vs %dx%d).\n", dimx, dimy, input->dimx, input->dimy);
				exit(EXIT_FAILURE);
			}
			if (dimy != -1 && dimy < input->dimy) {
				fprintf(stderr, "Input file must fit in image (%dx%d vs %dx%d).\n", dimx, dimy, input->dimx, input->dimy);
				exit(EXIT_FAILURE);
			}
			break;
		case 'W':
		case 'B':
			(void)0; // label cannot be part of declaration
			FILE *listfile = NULL;
			if (list != NULL) {
				fprintf(stderr, "Repeated whitelist/blacklist file '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			else if (!strcmp("-", optarg)) {
				listfile = stdin;
			}
			else {
				listfile = fopen(optarg, "rb");
				if (listfile == NULL) {
					fprintf(stderr, "Could not open %slist file '%s'.\n", c == 'W' ? "white" : "black", optarg);
					exit(EXIT_FAILURE);
				}
			}
			list = allocpnm();
			if (!freadpnm(list, listfile)) {
				fprintf(stderr, "Failed to read %slist file.\n", c == 'W' ? "white" : "black");
				exit(EXIT_FAILURE);
			}
			fclose(listfile);
			if (dimx == -1) dimx = list->dimx;
			if (dimx != list->dimx) {
				fprintf(stderr, "%slist file must have same size as image generated (%dx%d vs %dx%d).\n", (c == 'W' ? "White" : "Black"), dimx, dimy, list->dimx, list->dimy);
				exit(EXIT_FAILURE);
			}
			if (dimy == -1) dimy = list->dimy;
			if (dimy != list->dimy) {
				fprintf(stderr, "%slist file must have same size as image generated (%dx%d vs %dx%d).\n", (c == 'W' ? "White" : "Black"), dimx, dimy, list->dimx, list->dimy);
				exit(EXIT_FAILURE);
			}
			listtype = (c == 'W') ? WHITELIST : BLACKLIST;
			break;
		case 'bkgd':
			(void)0; // label cannot be part of declaration
			FILE *bkgdfile = NULL;
			if (background != NULL) {
				fprintf(stderr, "Repeated background file '%s'.\n", optarg);
				exit(EXIT_FAILURE);
			}
			else if (!strcmp("-", optarg)) {
				bkgdfile = stdin;
			}
			else {
				bkgdfile = fopen(optarg, "rb");
				if (bkgdfile == NULL) {
					fprintf(stderr, "Could not open background file '%s'.\n", optarg);
					exit(EXIT_FAILURE);
				}
			}
			background = allocpnm();
			if (!freadpnm(background, bkgdfile)) {
				fprintf(stderr, "Failed to read background file.\n");
				exit(EXIT_FAILURE);
			}
			fclose(bkgdfile);
			if (dimx != -1 && dimx < background->dimx) {
				fprintf(stderr, "Background file must fit in image (%dx%d vs %dx%d).\n", dimx, dimy, background->dimx, background->dimy);
				exit(EXIT_FAILURE);
			}
			if (dimy != -1 && dimy < background->dimy) {
				fprintf(stderr, "Background file must fit in image (%dx%d vs %dx%d).\n", dimx, dimy, background->dimx, background->dimy);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			return false;
	}
	return true;
}

void input_finalize(struct pnmdata *data, bool *used_, bool *blocked_) {
	int dimx = data->dimx;
	int dimy = data->dimy;
	int depth = data->depth;
	__m256d (*values)[dimx] = (__m256d(*)[dimx]) data->rawdata;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
	if (background != NULL) {
		if (background->dimx > dimx) {
			fprintf(stderr, "Invalid width of background image %d is greater than output image %d.\n", background->dimx, dimx);
			exit(EXIT_FAILURE);
		}
		if (background->dimy > dimy) {
			fprintf(stderr, "Invalid height of background image %d is greater than output image %d.\n", background->dimy, dimy);
			exit(EXIT_FAILURE);
		}
		if (background->depth != depth) {
			fprintf(stderr, "Invalid depth of background image %d is not equal to output image %d.\n", background->depth, depth);
			exit(EXIT_FAILURE);
		}
		int startx = (dimx - background->dimx) / 2;
		//int endx = startx + background->dimx;
		int starty = (dimy - background->dimy) / 2;
		//int endy = starty + background->dimy;
		__m256d (*bkgdvalues)[background->dimx] = (__m256d(*)[background->dimx]) background->rawdata;
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		for (int y = 0; y < background->dimy; ++y) {
			memcpy(&values[starty+y][startx], bkgdvalues[y], sizeof(*bkgdvalues));
		}
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}
	if (input != NULL) {
		if (input->dimx > dimx) {
			fprintf(stderr, "Invalid width of input image %d is greater than output image %d.\n", input->dimx, dimx);
			exit(EXIT_FAILURE);
		}
		if (input->dimy > dimy) {
			fprintf(stderr, "Invalid height of input image %d is greater than output image %d.\n", input->dimy, dimy);
			exit(EXIT_FAILURE);
		}
		if (input->depth != depth) {
			fprintf(stderr, "Invalid depth of input image %d is not equal to output image %d.\n", input->depth, depth);
			exit(EXIT_FAILURE);
		}
		int startx = (dimx - input->dimx) / 2;
		//int endx = startx + input->dimx;
		int starty = (dimy - input->dimy) / 2;
		//int endy = starty + input->dimy;
		__m256d (*invalues)[input->dimx] = (__m256d(*)[input->dimx]) input->rawdata;
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		for (int y = 0; y < input->dimy; ++y) {
			memcpy(&values[starty+y][startx], invalues[y], sizeof(*invalues));
			memset(&used[starty+y][startx], true, input->dimx*sizeof(bool));
		}
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}
	if (list != NULL) {
		if (listtype == WHITELIST) {
			memset(used_, false, dimx*dimy*sizeof(bool));
			memset(blocked_, false, dimx*dimy*sizeof(bool));
			if (list->dimx != dimx) {
				fprintf(stderr, "Invalid width of whitelist image %d is not equal to output image %d.\n", list->dimx, dimx);
				exit(EXIT_FAILURE);
			}
			if (list->dimy != dimy) {
				fprintf(stderr, "Invalid height of whitelist image %d is not equal to output image %d.\n", list->dimy, dimy);
				exit(EXIT_FAILURE);
			}
			int currentdepth = list->depth;
			__m256d (*currentvalues)[dimx] = (__m256d(*)[dimx]) list->rawdata;
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					for (int d = 0; d < currentdepth; ++d) {
						if (currentvalues[y][x][d] != 1.) { 
							// if any value is not 1, then the value isn't white, and not allowed
							used[y][x] = true;
							blocked[y][x] = true;
							break;
						}
					}
				}
			}
		}
		else {
			memset(used_, true, dimx*dimy*sizeof(bool));
			memset(blocked_, true, dimx*dimy*sizeof(bool));
			if (list->dimx != dimx) {
				fprintf(stderr, "Invalid width of blacklist image %d is not equal to output image %d.\n", list->dimx, dimx);
				exit(EXIT_FAILURE);
			}
			if (list->dimy != dimy) {
				fprintf(stderr, "Invalid height of blacklist image %d is not equal to output image %d.\n", list->dimy, dimy);
				exit(EXIT_FAILURE);
			}
			int currentdepth = list->depth;
			__m256d (*currentvalues)[dimx] = (__m256d(*)[dimx]) list->rawdata;
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					bool is_black = true;
					for (int d = 0; d < currentdepth; ++d) {
						if (currentvalues[y][x][d] != 0.) { 
							// if any value is not 0, then the value isn't black, and is allowed
							is_black = false;
							break;
						}
					}
					if (!is_black) {
						used[y][x] = false;
						blocked[y][x] = false;
					}
				}
			}
		}
	}
}



