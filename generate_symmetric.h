#ifndef GENERATE_SYMMETRIC_H
#define GENERATE_SYMMETRIC_H

#include <stdbool.h>

#include "generate_common.h"
#include "pnmlib.h"
#include "debug.h"

extern void generate_inner_symmetric(struct pnmdata *data, bool *used_, bool *blocked_);
extern void generate_outer_symmetric(struct pnmdata *data, bool *used_, bool *blocked_);


#endif // GENERATE_SYMMETRIC_H
