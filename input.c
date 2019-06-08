#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "safelib.h"
#include "pnmlib.h"
#include "debug.h"

typedef enum {
	WHITELIST,
	BLACKLIST,
	NONETYPE,
} listtype_t;

static FILE *infile = NULL;
static FILE *listfile = NULL;
static listtype_t listtype = NONETYPE;



bool input_option(int c, char *optarg) {
	//int ret;
	switch (c) {
		case 'i':
			if (infile != NULL) {
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
			break;
		case 'W':
		case 'B':
			listfile = fopen(optarg, "rb");
			if (listfile == NULL) {
				fprintf(stderr, "Could not open %slist file '%s'.\n", c == 'W' ? "white" : "black", optarg);
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
	if (infile != NULL) {
		struct pnmdata indata;
		initpnm(&indata);
		bool success = freadpnm(&indata, infile);
		if (!success) {
			fprintf(stderr, "Invalid input file.\n");
			exit(EXIT_FAILURE);
		}
		if (indata.dimx > dimx) {
			fprintf(stderr, "Invalid width of input image %d is greater than output image %d.\n", indata.dimx, dimx);
			exit(EXIT_FAILURE);
		}
		if (indata.dimy > dimy) {
			fprintf(stderr, "Invalid height of input image %d is greater than output image %d.\n", indata.dimy, dimy);
			exit(EXIT_FAILURE);
		}
		if (indata.depth != depth) {
			fprintf(stderr, "Invalid depth of input image %d is not equal to output image %d.\n", indata.depth, depth);
			exit(EXIT_FAILURE);
		}
		int startx = (dimx - indata.dimx) / 2;
		//int endx = startx + indata.dimx;
		int starty = (dimy - indata.dimy) / 2;
		//int endy = starty + indata.dimy;
		double (*invalues)[indata.dimx][depth] = (double(*)[indata.dimx][depth]) indata.rawdata;
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		for (int y = 0; y < indata.dimy; ++y) {
			memcpy(&values[starty+y][startx], invalues[y], sizeof(*invalues));
			memset(&used[starty+y][startx], true, indata.dimx*sizeof(bool));
		}
	//fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}
	if (listfile != NULL) {
		struct pnmdata *listdata;
		listdata = allocpnm();
		struct pnmdata *currentdata = listdata;
		if (!freadpnm(listdata, listfile)) {
			fprintf(stderr, "Invalid whitelist file.\n");
			exit(EXIT_FAILURE);
		}
		fclose(listfile);
		if (listtype == WHITELIST) {
			memset(used_, false, dimx*dimy*sizeof(bool));
			memset(blocked_, false, dimx*dimy*sizeof(bool));
			if (currentdata->dimx != dimx) {
				fprintf(stderr, "Invalid width of whitelist image %d is not equal to output image %d.\n", currentdata->dimx, dimx);
				exit(EXIT_FAILURE);
			}
			if (currentdata->dimy != dimy) {
				fprintf(stderr, "Invalid height of whitelist image %d is not equal to output image %d.\n", currentdata->dimy, dimy);
				exit(EXIT_FAILURE);
			}
			double (*currentvalues)[dimx][depth] = (double(*)[dimx][depth]) currentdata->rawdata;
			int currentdepth = currentdata->depth;
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
			currentdata = currentdata->next;
		}
		else {
			memset(used_, true, dimx*dimy*sizeof(bool));
			memset(blocked_, true, dimx*dimy*sizeof(bool));
			if (currentdata->dimx != dimx) {
				fprintf(stderr, "Invalid width of whitelist image %d is not equal to output image %d.\n", currentdata->dimx, dimx);
				exit(EXIT_FAILURE);
			}
			if (currentdata->dimy != dimy) {
				fprintf(stderr, "Invalid height of whitelist image %d is not equal to output image %d.\n", currentdata->dimy, dimy);
				exit(EXIT_FAILURE);
			}
			double (*currentvalues)[dimx][depth] = (double(*)[dimx][depth]) currentdata->rawdata;
			int currentdepth = currentdata->depth;
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
			currentdata = currentdata->next;
		}
	}
}



