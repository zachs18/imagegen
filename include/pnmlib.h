#ifndef PNMLIB_H
#define PNMLIB_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "color.h"

struct pnmdata {
	int dimx; /** Width */
	int dimy; /** Height */
	int maxval; /** Highest sample value */
	int depth; /** Samples per pixel */
	int commentcount; /** Number of comments */
	color_t *rawdata; /** actually a  color_t(*)[dimx] */
	char **comments; /** NULL or pointer to commentcount-length array of C-strings */
	struct pnmdata *next; /** pnm format supports multiple images per file */
};

struct pnmdata *allocpnm(void);
void initpnm(struct pnmdata *data);
void reinitpnm(struct pnmdata *data);
void freepnm(struct pnmdata *data);

bool freadpnm(struct pnmdata *data, FILE *file);
int freadpnms(struct pnmdata *data, FILE *file);
bool fwritepnm(const struct pnmdata *data, FILE *file);
int fwritepnms(const struct pnmdata *data, FILE *file);

#endif // PNMLIB_H
