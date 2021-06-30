#ifndef COLOR_H
#define COLOR_H

#include <stdbool.h>
#include <x86intrin.h>

#include "debug.h"

#define COLOR_SHORTOPTS "Nnv:t:b:"

#define COLOR_LONGOPTS \
    {"normal", no_argument, NULL, 'N'}, \
    {"newvectorset", no_argument, NULL, 'n'}, \
    {"vector", required_argument, NULL, 'v'}, \
    {"basecolor", required_argument, NULL, 'b'}, \
    {"vectortype", required_argument, NULL, 't'}, \
    {"hues", no_argument, NULL, 'hues'},

#define COLOR_HELP \
    "Color Options\n" \
    "    Normal Colors - Full range of each channel\n" \
    "        -N --normal    Default\n" \
    "    Vectorized Colors - The first of these options used implies -n \n" \
    "        -n             Start a new vectorset\n" \
    "        -v <vector>    Add <vector> to the current vectorset\n" \
    "        -b <vector>    Make <vector> the starting color for the current vectorset\n" \
    "        -t <type>      Change type of vectorset to <type>: full, triangular, or sum_one\n" \
    "        --hues         All full-intensity hues\n" \
    ""

#ifdef SSE
typedef __m128 color_t;
typedef float channel_t;

#define color_get_channel(color, channel) (color)[(channel)]
#define color_set_channel(color, channel, value) ((color)[(channel)] = (value))

#define color_set1(value) _mm_set1_ps(value)
#define SIMD

#elif defined AVX
typedef __m256d color_t;
typedef double channel_t;

#define color_get_channel(color, channel) (color)[(channel)]
#define color_set_channel(color, channel, value) ((color)[(channel)] = (value))

#define color_set1(value) _mm256_set1_pd(value)
#define SIMD

#elif defined FLOAT
typedef struct {
    float _rawcolor[4];
} color_t;
typedef float channel_t;

#define color_get_channel(color, channel) (color)._rawcolor[(channel)]
#define color_set_channel(color, channel, value) ((color)._rawcolor[(channel)] = (value))

#else
typedef struct {
    double _rawcolor[4];
} color_t;
typedef double channel_t;

#define color_get_channel(color, channel) (color)._rawcolor[(channel)]
#define color_set_channel(color, channel, value) ((color)._rawcolor[(channel)] = (value))

#endif // color_t, channel_t and related macros


#ifdef SIMD

#define color_sum(a, b) ((a) + (b))
#define color_difference(a, b) ((a) - (b))
#define color_product(a, b) ((a) * (b))

#else

#define color_set1(value) \
   ({color_t _color; \
     __auto_type _value = (value); \
     color_set_channel(_color, 0, _value); \
     color_set_channel(_color, 1, _value); \
     color_set_channel(_color, 2, _value); \
     color_set_channel(_color, 3, _value); \
     _color;})

#define color_sum(a, b) \
   ({color_t _a = (a); \
     color_t _b = (b); \
     color_t _sum; \
     color_set_channel(_sum, 0, color_get_channel(_a, 0) + color_get_channel(_b, 0)); \
     color_set_channel(_sum, 1, color_get_channel(_a, 1) + color_get_channel(_b, 1)); \
     color_set_channel(_sum, 2, color_get_channel(_a, 2) + color_get_channel(_b, 2)); \
     color_set_channel(_sum, 3, color_get_channel(_a, 3) + color_get_channel(_b, 3)); \
     _sum;})
#define color_difference(a, b) \
   ({color_t _a = (a); \
     color_t _b = (b); \
     color_t _difference; \
     color_set_channel(_difference, 0, color_get_channel(_a, 0) - color_get_channel(_b, 0)); \
     color_set_channel(_difference, 1, color_get_channel(_a, 1) - color_get_channel(_b, 1)); \
     color_set_channel(_difference, 2, color_get_channel(_a, 2) - color_get_channel(_b, 2)); \
     color_set_channel(_difference, 3, color_get_channel(_a, 3) - color_get_channel(_b, 3)); \
     _difference;})
#define color_product(a, b) \
   ({color_t _a = (a); \
     color_t _b = (b); \
     color_t _product; \
     color_set_channel(_product, 0, color_get_channel(_a, 0) * color_get_channel(_b, 0)); \
     color_set_channel(_product, 1, color_get_channel(_a, 1) * color_get_channel(_b, 1)); \
     color_set_channel(_product, 2, color_get_channel(_a, 2) * color_get_channel(_b, 2)); \
     color_set_channel(_product, 3, color_get_channel(_a, 3) * color_get_channel(_b, 3)); \
     _product;})

#endif // !defined(SIMD)


#define color_horizontal_sum(c) \
   ({color_t _c = (c); \
     channel_t _hs = color_get_channel(_c, 0) + \
                     color_get_channel(_c, 1) + \
                     color_get_channel(_c, 2) + \
                     color_get_channel(_c, 3); \
     _hs;})


bool color_option(int c, char *optarg);
void color_initialize(void);

extern color_t (*new_color)(void);

#endif // COLOR_H


