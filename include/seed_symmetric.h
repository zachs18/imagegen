#ifndef SEED_SYMMETRIC_H
#define SEED_SYMMETRIC_H

#include "seed_common.h"
#include "generate_symmetric.h"
#include "pnmlib.h"
#include "debug.h"

int compute_floodplanes_symmetric(struct pnmdata *data, bool *blocked_); // TODO: blocked_ needs to be checked if it is valid for the current symmetry when flooding, and if not, deal with that

int seed_image_symmetric(struct pnmdata *data, bool *used_, int seedcount);

#endif // SEED_SYMMETRIC_H
