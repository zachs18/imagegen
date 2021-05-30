/**
 * Copyright (C) 2018 Zachary Sample
 *
 * This file is part of Image Generator
 **/

/**
 * @file pnmlib.h
 */



#include "pnmlib.h"

#include <x86intrin.h>
#include <stdalign.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#include "safelib.h"
#include "debug.h"
#include "color.h"

/**
 * @brief Allocate and initialize pnmdata struct
 */
struct pnmdata *allocpnm(void) {
	struct pnmdata *ret = smalloc(sizeof(*ret));
	initpnm(ret);
	return ret;
}

/**
 * @brief Initialize pnmdata struct
 */
void initpnm(struct pnmdata *data) {
	if (data == NULL) return;
	data->dimx = -1;
	data->dimy = -1;
	data->maxval = -1;
	data->depth = -1;
	data->commentcount = 0;
	data->rawdata = NULL;
	data->comments = NULL;
	data->next = NULL;
}
/**
 * @brief Free the contents of and re-initialize pnmdata struct
 */
void reinitpnm(struct pnmdata *data) {
	if (data == NULL) return;
	data->dimx = -1;
	data->dimy = -1;
	data->maxval = -1;
	data->depth = -1;

	freepnm(data->next);
	data->next = NULL;

	if (data->comments != NULL) {
		for (int i = 0; i < data->commentcount; ++i) {
			free(data->comments[i]);
		}
		free(data->comments);
	}
	data->commentcount = 0;
	data->comments = NULL;

	_mm_free(data->rawdata);
	data->rawdata = NULL;

}

/**
 * @brief Free an allocated pnmdata struct
 */
void freepnm(struct pnmdata *data) {
	while (data != NULL) {
		struct pnmdata *next = data->next;
		if (data->comments != NULL) {
			for (int i = 0; i < data->commentcount; ++i) {
				free(data->comments[i]);
			}
			free(data->comments);
			data->comments = NULL;
		}
		_mm_free(data->rawdata);
		free(data);
		data = next;
	}
}


/**
 * @brief used in fscandim2_comments and fscandim3_comments to read the dimension and comments, and in plain parsing to parse comments in the data
 * @return bool: success
 */
bool fscannum_comments(struct pnmdata *data, FILE *file, int *dim) {
	int c;
	while (true) {
		(void)!fscanf(file, " "); // ignore result
		c = fgetc(file);
		if (c == EOF) {
			return false;
		}
		if (c == '#') {
			data->comments = srealloc(data->comments, sizeof(*(data->comments))*++data->commentcount);
			data->comments[data->commentcount-1] = NULL;
			char *currentcomment = NULL;
			int check = fscanf(file, "%m[^\n]", &currentcomment);
			if (check != 1 || currentcomment != NULL) {
				data->comments[data->commentcount-1] = currentcomment;
			}
			else {
				data->commentcount -= 1;
				// we don't need to realloc comments
				// because it will only ever be one too many
			}
		}
		else {
			ungetc(c, file);
			int ret = fscanf(file, "%d", dim);
			if (ret != 1) return false;
			break;
		}
	}
	return true;
}
/**
 * @brief used in freadpnm to read the dimensions and comments for plain and binary PBM
 * @return bool: success
 */
bool fscandim2_comments(struct pnmdata *data, FILE *file) {
	int c;
	bool ret;
	ret = fscannum_comments(data, file, &data->dimx);
	if (!ret) return false;
	ret = fscannum_comments(data, file, &data->dimy);
	if (!ret) return false;
	c = fgetc(file); // one whitespace character before image data
	if (!isspace(c)) ungetc(c, file);
	return true;
}
/**
 * @brief used in freadpnm to read the dimensions and comments for plain and binary PGM and PPM
 * @return bool: success
 */
bool fscandim3_comments(struct pnmdata *data, FILE *file) {
	int c;
	bool ret;
	ret = fscannum_comments(data, file, &data->dimx);
	if (!ret) return false;
	ret = fscannum_comments(data, file, &data->dimy);
	if (!ret) return false;
	ret = fscannum_comments(data, file, &data->maxval);
	if (!ret) return false;
	c = fgetc(file); // one whitespace character before image data
	if (!isspace(c)) ungetc(c, file);
	return true;
}

//TODO: single and multi-pnm functions
/**
 * @brief Read a pnm from <b>file</b> into <b>data</b>, freeing and allocating as needed
 * @return if image was successfully read
 */
bool freadpnm(struct pnmdata *data, FILE *file) {
	int c;
	if((c = fgetc(file)) != 'P') {
		fprintf(stderr, "Invalid Magic Number: %c\n", c);
		return false;
	}
	int format = fgetc(file);
	if (format == '6' || format == '5') { // PPM / PGM
		int depth = data->depth = (format == '6' ? 3 : 1);
		bool ret = fscandim3_comments(data, file);
		if (!ret) return false;
		int dimx = data->dimx, dimy = data->dimy, maxval = data->maxval;
		color_t (*values)[dimx] = s_mm_malloc(dimy * sizeof(*values), alignof(*values));
		data->rawdata = (color_t*) values;
		int val;
		if (maxval > 0 && maxval < 256) {
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					color_t color = color_set1(0);
					for (int d = 0; d < depth; ++d) {
						val = fgetc(file);
						if (val <= 0) /* 0 */;
						else if (val >= maxval) color_set_channel(color, d, 1);
						else color_set_channel(color, d, ((double)val) / maxval);
					}
					values[y][x] = color;
				}
			}
		}
		else if (maxval >= 256 && maxval < 65536) {
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					color_t color = color_set1(0);
					for (int d = 0; d < depth; ++d) {
						val = fgetc(file) * 256;
						val += fgetc(file); // sequence point between two fgetc calls
						if (val <= 0) /* 0 */;
						else if (val >= maxval) color_set_channel(color, d, 1);
						else color_set_channel(color, d, ((double)maxval) / val);
					}
					values[y][x] = color;
				}
			}
		}
		else return false;
		return true;
	}
	else if (format == '3' || format == '2') { // Plain PPM / PGM
		int depth = data->depth = (format == '3' ? 3 : 1);
		bool ret = fscandim3_comments(data, file);
		if (!ret) return false;
		int dimx = data->dimx, dimy = data->dimy, maxval = data->maxval;
		color_t (*values)[dimx] = s_mm_malloc(dimy * sizeof(*values), alignof(*values));
		data->rawdata = (color_t*) values;
		int val;
		if (maxval > 0) {
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					color_t color = color_set1(0);
					for (int d = 0; d < depth; ++d) {
						ret = fscannum_comments(data, file, &val);
						if (val <= 0) /* 0 */;
						else if (val >= maxval) color_set_channel(color, d, 1);
						else color_set_channel(color, d, ((double)val) / maxval);
					}
					values[y][x] = color;
				}
			}
		}
		else return false;
		return true;
	}
	else if (format == '4') { // PBM TODO
		fprintf(stderr, "UGGH, PBM is a pain.");
		return false;
	}
	else if (format == '1') { // Plain PBM
		// 1 is black, 0 is white
		data->depth = 1;
		bool ret = fscandim2_comments(data, file);
		if (!ret) return false;
		int dimx = data->dimx, dimy = data->dimy;
		data->maxval = 1;
		color_t (*values)[dimx] = s_mm_malloc(dimy * sizeof(*values), alignof(*values));
		data->rawdata = (color_t*) values;
		int val;
		for (int y = 0; y < dimy; ++y) {
			for (int x = 0; x < dimx; ++x) {
				ret = fscannum_comments(data, file, &val);
				if (val) values[y][x] = color_set1(0); // 1 is black
				else values[y][x] = color_set1(1);
			}
		}
		return true;
	}
	else {
		fprintf(stderr, "Invalid Magic Number: P%d\n", format);
		return false;
	}
}

/**
 * @brief Read a multi-image pnm from <b>file</b> into <b>data</b>, freeing and allocating as needed
 * @return number of images successfully read
 */
int freadpnms(struct pnmdata *data, FILE *file) {
	bool success = freadpnm(data, file);
	int c = fgetc(file);
	if (c != EOF) { // maybe just if c == 'P'
		ungetc(c, file);
		data->next = allocpnm();
		(void)!fscanf(file, " "); // ignore result
		return 1 + freadpnms(data->next, file);
	}
	return (int) success;
}

/**
 * @brief Write a single pnm image to <b>file</b> from <b>data</b>
 * @return success
 */
bool fwritepnm(const struct pnmdata *data, FILE *file) {
	int dimx = data->dimx, dimy = data->dimy, maxval = data->maxval, depth = data->depth;
	color_t (*values)[dimx] = (color_t(*)[dimx]) data->rawdata;
	if (depth == 3 || depth == 1) { // PPM / PGM
		fprintf(file, "P%d\n", (depth == 3 ? 6 : 5));
		for (int i = 0; i < data->commentcount; ++i) {
			fprintf(file, "#%s\n", data->comments[i]);
		}
		fprintf(file, "%d %d\n", dimx, dimy);
		fprintf(file, "%d\n", maxval);
		int val;
		if (maxval < 256) {
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					for (int d = 0; d < depth; ++d) {
						val = (int) (maxval * color_get_channel(values[y][x], d));
						if (val <= 0) fputc(0, file);
						else if (val >= maxval) fputc(maxval, file);
						else fputc(val, file);
					}
				}
			}
		}
		else {
			for (int y = 0; y < dimy; ++y) {
				for (int x = 0; x < dimx; ++x) {
					for (int d = 0; d < depth; ++d) {
						val = (int) (maxval * color_get_channel(values[y][x], d));
						if (val <= 0) {
							fputc(0, file);
							fputc(0, file);
						}
						else if (val >= maxval) {
							fputc(maxval/256, file);
							fputc(maxval%256, file);
						}
						else {
							fputc(val/256, file);
							fputc(val%256, file);
						}
					}
				}
			}
		}
		return true;
	}
	else { // unsupported as of yet
		return false;
	}
	fflush(file);
}

/**
 * @brief Write a multi-image pnm to <b>file</b> from <b>data</b>
 * @return number of images successfully written
 */
int fwritepnms(const struct pnmdata *data, FILE *file) {
	if (data != NULL) {
		fwritepnm(data, file);
		return 1 + fwritepnms(data->next, file);
	}
	return 0;
}
