#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

int dbglevel = 0;
#ifdef NO_DEBUG
bool debugging = false;
#else
bool debugging = true;
#endif

bool debug_option(int c, char *optarg) {
    if (c == 'd') {
        dbglevel++;
        if (!debugging)
            fprintf(stderr, "Warning: debugging is disabled.\n");
        else
            debug(0, "Debugging level %d.\n", dbglevel);
        return true;
    }
    else if (c == 'D') {
        dbglevel--;
        if (!debugging)
            fprintf(stderr, "Warning: debugging is disabled.\n");
        else
            debug(0, "Debugging level %d.\n", dbglevel);
        return true;
    }
    return false;
}
