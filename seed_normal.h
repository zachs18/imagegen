#ifndef SEED_NORMAL_H
#define SEED_NORMAL_H

#include "seed_common.h"
#include "generate_normal.h"
#include "pnmlib.h"
#include "debug.h"

int compute_floodplanes_normal(struct pnmdata *data, bool *blocked_);

int seed_image_normal(struct pnmdata *data, bool *used_, int seedcount);

#endif // SEED_NORMAL_H
