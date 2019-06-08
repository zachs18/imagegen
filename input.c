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
				fprintf(stderr, "Failed to read input file.\n");
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
		default:
			return false;
	}
	return true;
}

void input_finalize(struct pnmdata *data, bool *used_, bool *blocked_) {
	int dimx = data->dimx;
	int dimy = data->dimy;
	int depth = data->depth;
	double (*values)[dimx][depth] = (double(*)[dimx][depth]) data->rawdata;
	bool (*used)[dimx] = (bool(*)[dimx]) used_;
	bool (*blocked)[dimx] = (bool(*)[dimx]) blocked_;
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
		double (*invalues)[input->dimx][depth] = (double(*)[input->dimx][depth]) input->rawdata;
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
			double (*currentvalues)[dimx][currentdepth] = (double(*)[dimx][depth]) list->rawdata;
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
			double (*currentvalues)[dimx][currentdepth] = (double(*)[dimx][depth]) list->rawdata;
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					for (int d = 0; d < currentdepth; ++d) {
						if (currentvalues[y][x][d] != 0.) { 
							// if any value is not 0, then the value isn't black, and is allowed
							break;
						}
						used[y][x] = false;
						blocked[y][x] = false;
					}
				}
			}
		}
	}
}



