#include "color.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>

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
	__m128 start;
	__m128 *colors;
	int count;
	int chance;
	enum vectorsettype_t type;
};

struct vectorset *vectorsets = NULL;
int vectorsetcount = 0;
int totalchance = 0;

__m128 (*new_color)(void) = NULL;

static __m128 new_color_basic(void);
static __m128 new_color_vector(void);

static void newvectorset(void);
static void vectorset_base(char *optarg_);
static void newvector(char *optarg);

static __m128 new_color_vector_helper(struct vectorset *vectorset);
static __m128 new_color_vector_full(struct vectorset *vectorset);
static __m128 new_color_vector_triangular(struct vectorset *vectorset);
static __m128 new_color_vector_sum_one(struct vectorset *vectorset);

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

static __m128 new_color_basic() {
	__m128 c = _mm_set1_ps(0.);
	for (int i = 0; i < depth; ++i) {
		c[i] = random() / (float) RAND_MAX;
	}
	return c;
}

static void newvectorset(void) {
	struct vectorset *temp = s_mm_malloc(++vectorsetcount * sizeof(*vectorsets), alignof(*vectorsets));
	memcpy(temp, vectorsets, (vectorsetcount-1) * sizeof(*vectorsets));
	vectorsets = temp;

	vectorsets[vectorsetcount-1].colors = NULL;
	vectorsets[vectorsetcount-1].start = _mm_set1_ps(0.);
	vectorsets[vectorsetcount-1].count = 0;
	vectorsets[vectorsetcount-1].chance = 1;
	vectorsets[vectorsetcount-1].type = NONE;
}

static void vectorset_base(char *optarg_) {
	char *optarg = optarg_;
	struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
	if (depth > 0) { // we are using the depth already set // start color space is already allocated
		int ret, index;
		ret = sscanf(optarg, "%f%n", &(vectorset->start[0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		for (int i = 1; i < depth; ++i) {
			ret = sscanf(optarg, ",%f%n", &(vectorset->start[i]), &index);
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
		vectorset->start = _mm_set1_ps(0.);
		int ret, index;
		ret = sscanf(optarg, "%f%n", &(vectorset->start[0]), &index);
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
			vectorset->start[depth-1] = temp;
		}
	}
}

static void newvector(char *optarg_) {
	char *optarg = optarg_;
	struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
	__m128 *temp = s_mm_malloc(++(vectorset->count) * sizeof(*(vectorset->colors)), alignof(*(vectorset->colors)));
	memcpy(temp, vectorset->colors, (vectorset->count-1) * sizeof(*(vectorset->colors)));
	vectorset->colors = temp;
	if (depth > 0) { // we are using the depth already set
		vectorset->colors[vectorset->count-1] = _mm_set1_ps(0.);
		int ret, index;
		ret = sscanf(optarg, "%f%n", &(vectorset->colors[vectorset->count-1][0]), &index);
		if (ret != 1) {
			fprintf(stderr, "Invalid color: '%s'\n", optarg_);
			exit(EXIT_FAILURE);
		}
		optarg += index;
		for (int i = 1; i < depth; ++i) {
			ret = sscanf(optarg, ",%f%n", &(vectorset->colors[vectorset->count-1][i]), &index);
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
		vectorset->colors[0] = _mm_set1_ps(0.);
		int ret, index;
		ret = sscanf(optarg, "%f%n", &(vectorset->colors[0][0]), &index);
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
			vectorset->colors[0][depth-1] = temp;
		}
		vectorset->start = _mm_set1_ps(0.);
	}
}

static __m128 new_color_vector(void) {
	if (vectorsetcount == 1) {
		return new_color_vector_helper(&vectorsets[0]);
	}
	else {
		int r = randint(0, totalchance);
		for (int i = 0; i < vectorsetcount; ++i) {
			if (r >= vectorsets[i].chance) {
				r -= vectorsets[i].chance;
				continue;
			}
			return new_color_vector_helper(&vectorsets[i]);
		}
		return _mm_set1_ps(0.);
	}
}

static __m128 new_color_vector_helper(struct vectorset *vectorset) {
	switch (vectorset->type) {
		case FULL:
			return new_color_vector_full(vectorset);
		case TRIANGULAR:
			return new_color_vector_triangular(vectorset);
		case SUM_ONE:
			return new_color_vector_sum_one(vectorset);
		default:
			fprintf(
				stderr,
				"Runtime error: %s:%d\n%d\n",
				__FILE__, __LINE__, vectorset->type
				);
			exit(EXIT_FAILURE);
	}
}

static __m128 new_color_vector_full(struct vectorset *vectorset) {
	__m128 color = vectorset->start;
	for (int i = 0; i < vectorset->count; ++i) {
		float r = random() / (float) RAND_MAX;
		__m128 temp = r * vectorset->colors[i];
		color += temp;
	}
	return color;
}

static __m128 new_color_vector_triangular(struct vectorset *vectorset) {
	exit(EXIT_FAILURE);
}

static __m128 new_color_vector_sum_one(struct vectorset *vectorset) {
	exit(EXIT_FAILURE);
}
