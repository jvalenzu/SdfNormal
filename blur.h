// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#pragma once

#include "Container/bit.h"

void blur_init();

void blur(float* values, size_t length, int kernel_width, const bitvector_t* mask);
void blur2d(float* values, size_t width, size_t height, int kernel_width, const bitvector_t* mask);
