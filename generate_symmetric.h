#ifndef GENERATE_SYMMETRIC_H
#define GENERATE_SYMMETRIC_H

#include <stdbool.h>

#include "generate_common.h"
#include "pnmlib.h"
#include "debug.h"

extern static void generate_symmetric_inner(struct pnmdata *data, bool *used_, bool *blocked_);
extern static void generate_symmetric_outer(struct pnmdata *data, bool *used_, bool *blocked_);


#endif // GENERATE_SYMMETRIC_H
