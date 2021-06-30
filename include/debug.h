#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef NO_DEBUG
# define debug(n, format, ...) (void)(dbglevel > (n) && fprintf(stderr, "%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__) && fflush(stderr))
#else
# define debug(...) (void)0
#endif // NO_DEBUG

#define debug_0 debug(0, "\n")
#define debug_1 debug(1, "\n")
#define debug_2 debug(2, "\n")

#define DEBUG_SHORTOPTS "dD"

#define DEBUG_LONGOPTS \
    {"debug", no_argument, NULL, 'd'}, \
    {"undebug", no_argument, NULL, 'D'}, \

#define DEBUG_HELP \
    "Debug Option\n" \
    "    -d --debug    Increase debug level by 1 (can be repeated).\n"\
    "    -D --undebug  Decrease debug level by 1 (can be repeated).\n"\
    ""

extern int dbglevel;

bool debug_option(int c, char *optarg);

#endif // DEBUG_H

