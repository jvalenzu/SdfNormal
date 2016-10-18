// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#pragma once

#include "cda.h"

void dr_init(float* heights, border_pixel_t* border_pixels, const unsigned char* input, const int* islands, int island, size_t width, size_t height, int threshold);
void dr_iterate(float* heights, border_pixel_t* border_pixels, const unsigned char* input, size_t width, size_t height, int threshold);
float dr_normalize(float* height_map, const unsigned char* image, const int* islands, int island, size_t width, size_t height, int threshold);
