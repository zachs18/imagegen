#ifndef SEED_H
#define SEED_H

#include "generate.h"
#include "pnmlib.h"
#include "debug.h"

extern int floodplanecount;

int compute_floodplanes(struct pnmdata *data, bool *blocked_);

int seed_image(struct pnmdata *data, bool *used_, int seedcount);

#endif // RANDINT_H
