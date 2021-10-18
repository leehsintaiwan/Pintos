#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* 17 bits before the decimal point, 14 bits after */
#define P 17
#define Q (31 - P)
#define F (1 << Q)

/* Conversion macros */
#define INTEGER_TO_FIXED_POINT (n) (n * F)
#define FIXED_POINT_TO_INTEGER_ZERO (x) (x / F)

inline int32_t fixed_point_to_integer_nearest (int32_t x)
{
    return x >= 0 ? (x + F / 2) / F : (x - F / 2) / F;
}

/* Binary operations */
#define ADD_FIXED_POINTS(x, y) (x + y)
#define SUBTRACT_FIXED_POINTS(x, y) (x - y)
#define ADD_FIXED_AND_INTEGER(x, n) (x + n * F)
#define SUBTRACT_INTEGER_FROM_FIXED(x, n) (x - n * F)
#define MULTIPLY_FIXED_AND_INTEGER(x, n) (x * n)
#define DIVIDE_FIXED_BY_INTEGER(x, n) (x / n)

inline int64_t multiply_fixed_points (int32_t x, int32_t y)
{
    return ((int64_t) x) * y / F;
}

inline int64_t divide_fixed_points (int32_t x, int32_t y)
{
    return ((int64_t) x) * F / y;
}

#endif /* threads/fixed-point.h */