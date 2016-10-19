// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#pragma once

#include "slib/Container/BitVector.h"

void blur_init();

void blur(float* values, size_t length, int kernel_width, const BitVector* mask);
void blur2d(float* values, size_t width, size_t height, int kernel_width, const BitVector* mask);
