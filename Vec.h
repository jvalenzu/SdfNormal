// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#pragma once

inline float euclidian_distance(int r, int c, int rp, int cp)
{
    const int rdist = r - rp;
    const int cdist = c - cp;
    return sqrtf(1.0f*(rdist*rdist + cdist*cdist));
}

inline void normalize(float* x, float* y, float* z)
{
    float len = sqrtf(*x**x+*y**y+*z**z);
    *x /= len;
    *y /= len;
    *z /= len;
}

inline void normalize2(float* x, float* y)
{
    float len = sqrtf(*x**x+*y**y);
    *x /= len;
    *y /= len;
}

inline void cross(float* dest_x, float* dest_y, float* dest_z,
                  float a_x, float a_y, float a_z,
                  float b_x, float b_y, float b_z)
{
    *dest_x = a_y*b_z-a_z*b_y;
    *dest_y = a_z*b_x-a_x*b_z;
    *dest_z = a_x*b_y-a_y*b_x;
}

inline float to_zero_one(float x)
{
    return (x + 1.0f) * 0.5f;
}

inline float from_zero_one(float x)
{
    return x * 2.0f - 1.0f;
}

inline void vec_sub(float* v0x, float* v0y, float v1x, float v1y)
{
    *v0x -= v1x;
    *v0y -= v1y;
}
