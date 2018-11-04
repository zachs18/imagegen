#ifndef SEED_SYMMETRIC_H
#define SEED_SYMMETRIC_H

#include "generate_symmetric.h"
#include "pnmlib.h"
#include "debug.h"

extern int floodplanecount_symmetric;

int compute_floodplanes_symmetric(struct pnmdata *data, bool *blocked_);

int seed_image_symmetric(struct pnmdata *data, bool *used_, int seedcount);

#endif // RANDINT_H
