#include "color.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>

#include "setup.h" // depth
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
    color_t start;
    color_t *colors;
    int count;
    int chance;
    enum vectorsettype_t type;
};

struct vectorset *vectorsets = NULL;
int vectorsetcount = 0;
int totalchance = 0;

color_t (*new_color)() = NULL;

static color_t new_color_basic(void);
static color_t new_color_vector(void);

static void newvectorset(void);
static void vectorset_base(char *optarg_);
static void newvector(char *optarg_);

static color_t new_color_vector_helper(struct vectorset *vectorset);
static color_t new_color_vector_full(struct vectorset *vectorset);
static color_t new_color_vector_triangular(struct vectorset *vectorset);
static color_t new_color_vector_sum_one(struct vectorset *vectorset);

bool color_option(int c, char *optarg_) {
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
            newvector(optarg_);
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
            vectorset_base(optarg_);
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

static color_t new_color_basic(void) {
    color_t c = color_set1(0);
    for (int i = 0; i < depth; ++i) {
        color_set_channel(c, i, random() / (double) RAND_MAX);
    }
    return c;
}

static void newvectorset(void) {
    vectorsets = sreallocarray(vectorsets, ++vectorsetcount, sizeof(*vectorsets));
    vectorsets[vectorsetcount-1].colors = NULL;
    vectorsets[vectorsetcount-1].start = color_set1(0);
    vectorsets[vectorsetcount-1].count = 0;
    vectorsets[vectorsetcount-1].chance = 1;
    vectorsets[vectorsetcount-1].type = NONE;
}

static color_t read_color(char const *optarg_) {
    color_t color = color_set1(0);
    if (depth > 0) { // we are using the depth already set
        int ret, index;
        double temp;
        ret = sscanf(optarg_, "%lf%n", &temp, &index);
        if (ret != 1) {
            fprintf(stderr, "Invalid color: '%s'\n", optarg_);
            exit(EXIT_FAILURE);
        }
        color_set_channel(color, 0, temp);
        optarg_ += index;
        for (int i = 1; i < depth; ++i) {
            ret = sscanf(optarg_, ",%lf%n", &temp, &index);
            if (ret != 1) {
                fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
                exit(EXIT_FAILURE);
            }
            color_set_channel(color, i, temp);
            optarg_ += index;
        }
        if (*optarg_ != '\x00') {
            fprintf(stderr, "Invalid color for depth %d: '%s'\n", depth, optarg_);
            exit(EXIT_FAILURE);
        }
    }
    else { // we are setting the depth
        depth = 1;
        int ret, index;
        double temp;
        ret = sscanf(optarg_, "%lf%n", &temp, &index);
        if (ret != 1) {
            fprintf(stderr, "Invalid color: '%s'\n", optarg_);
            exit(EXIT_FAILURE);
        }
        color_set_channel(color, 0, temp);
        optarg_ += index;
        while (*optarg_ != '\x00') {
            ret = sscanf(optarg_, ",%lf%n", &temp, &index);
            if (ret != 1 || depth >= 4) {
                fprintf(stderr, "Invalid color: '%s'\n", optarg_);
                exit(EXIT_FAILURE);
            }
            optarg_ += index;
            ++depth;
            color_set_channel(color, depth-1, temp);
        }
    }
    return color;
}
static void vectorset_base(char *optarg_) {
    struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
    vectorset->start = read_color(optarg_);
}

static void newvector(char *optarg_) {
    struct vectorset *vectorset = &vectorsets[vectorsetcount-1];
    color_t *temp = s_mm_malloc(
        ++(vectorset->count) * sizeof(*(vectorset->colors)),
        alignof(*(vectorset->colors))
    );
    memcpy(temp, vectorset->colors, (vectorset->count-1) * sizeof(*(vectorset->colors)));
    _mm_free(vectorset->colors);
    vectorset->colors = temp;
    vectorset->colors[vectorset->count-1] = read_color(optarg_);
}

static color_t new_color_vector(void) {
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
            break;
        }
    }
    fprintf(stderr, "Logic Error: %s %d\n", __FUNCTION__, __LINE__);
    return color_set1(0.);
}

static color_t new_color_vector_helper(struct vectorset *vectorset) {
    switch (vectorset->type) {
        case FULL:
            return new_color_vector_full(vectorset);
            break;
        case TRIANGULAR:
            return new_color_vector_triangular(vectorset);
            break;
        case SUM_ONE:
            return new_color_vector_sum_one(vectorset);
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

static color_t new_color_vector_full(struct vectorset *vectorset) {
    color_t c = vectorset->start;
    for (int i = 0; i < vectorset->count; ++i) {
        double r = random() / (double) RAND_MAX;
        c = color_sum(c, color_product(color_set1(r), vectorset->colors[i]));
    }
    return c;
}

static color_t new_color_vector_triangular(struct vectorset *vectorset) {
    exit(EXIT_FAILURE);
}

static color_t new_color_vector_sum_one(struct vectorset *vectorset) {
    exit(EXIT_FAILURE);
}
