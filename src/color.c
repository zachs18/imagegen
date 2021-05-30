#include "color.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "safelib.h"
#include "pnmlib.h"
#include "randint.h"
#include "debug.h"

enum vectorsettype_t {
	NONE,
	FULL,
	TRIANGULAR,
	SUM_ONE
};

struct vectorset {
	double **colors;
	double *start;
	int count;
	int chance;
	enum vectorsettype_t type;
};

struct vectorset *vectorsets = NULL;
int vectorsetcount = 0;
int totalchance = 0;

void (*new_color)(double *c) = NULL;

static void new_color_basic(double *c);
static void new_color_vector(double *c);

static void newvectorset(void);
static void vectorset_base(char *optarg_);
static void newvector(char *optarg);

static void new_color_vector_helper(struct vectorset *vectorset, double *c);
static void new_color_vector_full(struct vectorset *vectorset, double *c);
static void new_color_vector_triangular(struct vectorset *vectorset, double *c);
static void new_color_vector_sum_one(struct vectorset *vectorset, double *c);

bool color_option(int c, char *optarg) {
	switch (c) {
		case 'N':
			if (new_color != NULL) {
				fprintf(stderr, "Cannot change color type after choosing.\n");
				exit(EXIT_FAILURE);
			}
			new_color = new_color_basic;
			break;
		case 'n':
			if (new_color != NULL && new_color != new_color_vector) {
				fprintf(stderr, "Cannot change color type after choosing.\n");
				exit(EXIT_FAILURE);
			}
			new_color = new_color_vector;
			newvectorset();
			break;
		case 'v':
			if (new_color == NULL) {
				new_color = new_color_vector;
				newvectorset();
			}
			else if (new_color != new_color_vector) {
				fprintf(stderr, "Cannot change color type after choosing.\n");
				exit(EXIT_FAILURE);
			}
			newvector(optarg);
			break;
		case 'b':
			if (new_color == NULL) {
				new_color = new_color_vector;
				newvectorset();
			}
			else if (new_color != new_color_vector) {
				fprintf(stderr, "Cannot change color type after choosing.\n");
				exit(EXIT_FAILURE);
			}
			vectorset_base(optarg);
			break;
		case 'hues':
			if (new_color == NULL) {
				new_color = new_color_vector;
			}
			else if (new_color != new_color_vector) {
				fprintf(stderr, "Cannot change color type after choosing.\n");
				exit(EXIT_FAILURE);
			}
			// Red to Yellow
			newvectorset();
			vectorset_base("1,0,0");
			newvector("0,1,0");
			// Green to Yellow
			newvectorset();
			vectorset_base("0,1,0");
			newvector("1,0,0");
			// Green to Cyan
			newvectorset();
			vectorset_base("0,1,0");
			newvector("0,0,1");
			// Blue to Cyan
			newvectorset();
			vectorset_base("0,0,1");
			newvector("0,1,0");
			// Blue to Magenta
			newvectorset();
			vectorset_base("0,0,1");
			newvector("1,0,0");
			// Red to Magenta
			newvectorset();
			vectorset_base("1,0,0");
			newvector("0,0,1");
			break;
		default:
			return false;
	}
	return true;
}

void color_initialize(void) {
	if (new_color == NULL) 
		new_color = new_color_basic;
	if (new_color == new_color_vector) {
		for (int i = 0; i < vectorsetcount; ++i) {
			totalchance += vectorsets[i].chance;
		}
		for (int i = 0; i < vectorsetcount; ++i)
			if (vectorsets[i].type == NONE)
				vectorsets[i].type = FULL;
	}
}

static void new_color_basic(double *c) {
	for (int i = 0; i < depth; ++i) {
		c[i] = random() / (double) RAND_MAX;
	}
}

static void newvectorset(void) {
	vectorsets = sreallocarray(vectorsets, ++vectorsetcount, sizeof(*vectorsets));
	vectorsets[vectorsetcount-1].colors = NULL;
	if (depth > 0)
		vectorsets[vectorsetcount-1].start = scalloc(depth, sizeof(double)); // scalloc initializes to 0
	else
		vectorsets[vectorsetcount-1].start = NULL;
	vectorsets[vectorsetcount-1].count = 0;
	vectorsets[vectorsetcount-1].chance = 1;
	vectorsets[vectorsetcount-1].type = NONE;
}

static void vectorset_base(char *optarg_) {
	char *optarg = optarg_;
	struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
	if (depth > 0) { // we are using the depth already set // start color space is already allocated
		int ret, index;
		ret = sscanf(optarg, "%lf%n", &(vectorset->start[0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		for (int i = 1; i < depth; ++i) {
			ret = sscanf(optarg, ",%lf%n", &(vectorset->start[i]), &index);
			if (ret != 1) {
				fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
				exit(EXIT_FAILURE);
			}
			optarg += index;
		}
		if (*optarg != '\x00') {
			fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
			exit(EXIT_FAILURE);
		}
	}
	else { // we are setting the depth here, we need to initialize vectorset->start
		depth = 1;
		vectorset->start = scalloc(depth, sizeof(*(vectorset->start)));
		int ret, index;
		ret = sscanf(optarg, "%lf%n", &(vectorset->start[0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		while (*optarg != '\x00') {
			double temp;
			ret = sscanf(optarg, ",%lf%n", &temp, &index);
			if (ret != 1) {
				fprintf(stderr, "Invalid color: '%s'\n", optarg_);
				exit(EXIT_FAILURE);
			}
			optarg += index;
			++depth;
			vectorset->start = sreallocarray(vectorset->start, depth, sizeof(*(vectorset->start)));
			vectorset->start[depth-1] = temp;
		}
	}
	
}

static void newvector(char *optarg_) {
	char *optarg = optarg_;
	struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
	vectorset->colors = sreallocarray(
		vectorset->colors,
		++(vectorset->count),
		sizeof(*(vectorset->colors))
		);
	if (depth > 0) { // we are using the depth already set
		vectorset->colors[vectorset->count-1] = scalloc(depth, sizeof(*(vectorset->colors[0])));
		int ret, index;
		ret = sscanf(optarg, "%lf%n", &(vectorset->colors[vectorset->count-1][0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		for (int i = 1; i < depth; ++i) {
			ret = sscanf(optarg, ",%lf%n", &(vectorset->colors[vectorset->count-1][i]), &index);
			if (ret != 1) {
				fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
				exit(EXIT_FAILURE);
			}
			optarg += index;
		}
		if (*optarg != '\x00') {
			fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
			exit(EXIT_FAILURE);
		}
	}
	else { // we are setting the depth here, we also need to initialize vectorset->start
		depth = 1;
		vectorset->colors[0] = scalloc(depth, sizeof(*(vectorset->colors[0])));
		int ret, index;
		ret = sscanf(optarg, "%lf%n", &(vectorset->colors[0][0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		while (*optarg != '\x00') {
			double temp;
			ret = sscanf(optarg, ",%lf%n", &temp, &index);
			if (ret != 1) {
				fprintf(stderr, "Invalid color: '%s'\n", optarg_);
				exit(EXIT_FAILURE);
			}
			optarg += index;
			++depth;
			vectorset->colors[0] = sreallocarray(vectorset->colors[0], depth, sizeof(*(vectorset->colors[0])));
			vectorset->colors[0][depth-1] = temp;
		}
		vectorset->start = scalloc(depth, sizeof(*(vectorset->start)));
	}
}

static void new_color_vector(double *c) {
	if (vectorsetcount == 1) {
		new_color_vector_helper(&vectorsets[0], c);
	}
	else {
		int r = randint(0, totalchance);
		for (int i = 0; i < vectorsetcount; ++i) {
			if (r >= vectorsets[i].chance) {
				r -= vectorsets[i].chance;
				continue;
			}
			new_color_vector_helper(&vectorsets[i], c);
			break;
		}
	}
}

static void new_color_vector_helper(struct vectorset *vectorset, double *c) {
	switch (vectorset->type) {
		case FULL:
			new_color_vector_full(vectorset, c);
			break;
		case TRIANGULAR:
			new_color_vector_triangular(vectorset, c);
			break;
		case SUM_ONE:
			new_color_vector_sum_one(vectorset, c);
			break;
		default:
			fprintf(
				stderr,
				"Runtime error: %s:%d\n%d\n",
				__FILE__, __LINE__, vectorset->type
				);
			exit(EXIT_FAILURE);
	}
}

static void new_color_vector_full(struct vectorset *vectorset, double *c) {
	memcpy(c, vectorset->start, depth*sizeof(*c));
	for (int i = 0; i < vectorset->count; ++i) {
		double r = random() / (double) RAND_MAX;
		for (int d = 0; d < depth; ++d) {
			double temp;
			temp = r * vectorset->colors[i][d];
			c[d] += temp;
		}
	}
}

static void new_color_vector_triangular(struct vectorset *vectorset, double *c) {
	exit(EXIT_FAILURE);
}

static void new_color_vector_sum_one(struct vectorset *vectorset, double *c) {
	exit(EXIT_FAILURE);
}