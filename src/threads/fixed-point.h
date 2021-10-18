#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* 17 bits before the decimal point, 14 bits after */
#define P 17
#define Q 14
#define F 1 << Q

/* Conversion macros */
#define integer_to_fixed_point (n) (n * F)
#define fixed_point_to_integer_zero (x) (x / F)

inline int32_t fixed_point_to_integer_nearest (int32_t x)
{
    return x >= 0 ? (x + F / 2) / F : (x - F / 2) / F;
}

/* Binary operations */
#define add_fixed_points(x, y) (x + y)
#define subtract_fixed_points(x, y) (x - y)
#define add_fixed_and_integer(x, n) (x + n * F)
#define subtract_integer_from_fixed(x, n) (x - n * F)
#define multiply_fixed_and_integer(x, n) (x * n)
#define divide_fixed_by_integer(x, n) (x / n)

inline int64_t multiply_fixed_points (int32_t x, int32_t y)
{
    return ((int64_t) x) * y / F;
}

inline int64_t divide_fixed_points (int32_t x, int32_t y)
{
    return ((int64_t) x) * F / y;
}

#endif /* threads/fixed-point.h */