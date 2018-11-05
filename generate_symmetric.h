#ifndef GENERATE_SYMMETRIC_H
#define GENERATE_SYMMETRIC_H

#include <stdbool.h>

#include "generate_common.h"
#include "pnmlib.h"
#include "debug.h"

extern int sym_hcount;
extern int sym_vcount;
extern bool sym_h_hflip;
extern bool sym_h_vflip;
extern bool sym_v_hflip;
extern bool sym_v_vflip;

extern bool sym_h_hflip; // horizontal flip from one section to the next horizontally
extern bool sym_h_vflip; // vertical flip from one section to the next horizontally
extern bool sym_v_hflip; // horizontal flip from one section to the next vertically
extern bool sym_v_vflip; // vertical flip from one section to the next vertically

extern bool sym_sharedrow; // Is the row between sections shared between them
extern bool sym_sharedcolumn; // Is the column between sections shared between them

extern int sym_maxx;
extern int sym_maxy;


extern void get_canonical_pixel_symmetric(int x, int y, int *rx, int *ry);

extern void generate_inner_symmetric(struct pnmdata *data, bool *used_, bool *blocked_);
extern void generate_outer_symmetric(struct pnmdata *data, bool *used_, bool *blocked_);


#endif // GENERATE_SYMMETRIC_H
