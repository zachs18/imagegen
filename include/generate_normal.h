#ifndef GENERATE_NORMAL_H
#define GENERATE_NORMAL_H

#include <stdbool.h>

#include "generate_common.h"
#include "pnmlib.h"
#include "debug.h"

void generate_inner_normal(struct pnmdata *data, bool *used_, bool *blocked_);
void generate_outer_normal(struct pnmdata *data, bool *used_, bool *blocked_);

#endif // GENERATE_NORMAL_H
