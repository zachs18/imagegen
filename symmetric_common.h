#ifndef SYMMETRIC_COMMON_H
#define SYMMETRIC_COMMON_H

#include <stdlib.h>

#include "generate_symmetric.h"
#include "seed_symmetric.h"
#include "debug.h"

void get_canonical_pixel_symmetric(int x, int y, int *rx, int *ry); // Return the canonical pixel coordinate of (x,y) in (*rx, *ry) based on the symmetry specification

#endif // SYMMETRIC_COMMON_H
