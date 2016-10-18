// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#pragma once

#include <sys/types.h>
#include <stdint.h>

struct border_pixel_t
{
    short m_R;
    short m_C;
};
typedef struct border_pixel_t border_pixel_t;

void  cda_init(float* heights, const unsigned char* input, const int* islands, int island, size_t width, size_t height, int test_dir, int threshold);
void  cda_iterate(float* heights, const unsigned char* input, size_t width, size_t height, int threshold);

