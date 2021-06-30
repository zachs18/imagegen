#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "pnmlib.h"
#include "debug.h"

static FILE *outfile = NULL;

bool output_option(int c, char *optarg) {
    switch (c) {
        case 'o':
            if (outfile != NULL) {
                fprintf(stderr, "Repeated output file '%s'.\n", optarg);
                exit(EXIT_FAILURE);
            }
            else if (!strcmp("-", optarg)) {
                outfile = stdout;
            }
            else {
                outfile = fopen(optarg, "wb");
                if (outfile == NULL) {
                    fprintf(stderr, "Could not open output file '%s'.\n", optarg);
                    exit(EXIT_FAILURE);
                }
            }
            break;
        default:
            return false;
    }
    return true;
}

void output(const struct pnmdata *data) {
    //fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
    if (outfile == NULL)
        return;
        //outfile = stdout;
    if (!fwritepnm(data, outfile)) {
        fprintf(stderr, "Failed to output file with dimensions %dx%d, maxval %d, and depth %d.\n", data->dimx, data->dimy, data->maxval, data->depth);
        exit(EXIT_FAILURE);
    }
    //fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
}