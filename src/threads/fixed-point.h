#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/* 17 bits before the decimal point, 14 bits after */
#define P 17
#define Q 14
#define F 2 ^ Q

/* Conversion macros */
#define integer_to_fixed_point (n) (n * f)
#define fixed_point_to_integer_zero (x) (x / f)

inline uint32_t fixed_point_to_integer_nearest (uint32_t x)
{
    x >= 0 ? return (x + f / 2) / f : return (x - f / 2) / f;
}

#endif /* threads/fixed-point.h */