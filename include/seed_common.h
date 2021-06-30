#ifndef SEED_COMMON_H
#define SEED_COMMON_H

#include "generate_common.h"
#include "debug.h"

struct floodplane {
    struct pixel *pixels;
    int count;
};

extern struct floodplane *floodplanes;
extern int floodplanecount;

#endif // SEED_COMMON_H
