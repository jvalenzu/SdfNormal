// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "blur.h"
#include "Common/Util.h"

struct pascal_entry_t
{
    int m_Entries;
    float m_Data[16];
    int m_NumCoeffs;
    float m_Coeffs[16];
};

static struct pascal_entry_t s_PascalTriangle[] =
{
    {  0, { } },
    {  1, { 1 } },
    {  2, { 1, 1 } },
    {  3, { 1, 2, 1} },
    {  4, { 1, 3, 3, 1} },
    {  5, { 1, 4, 6, 4, 1} },
    {  6, { 1, 5, 10, 10, 5, 1} },
    {  7, { 1, 6, 15, 20, 15, 6, 1} },
    {  8, { 1, 7, 21, 35, 35, 21, 7, 1} },
    {  9, { 1, 8, 28, 56, 70, 56, 28, 8, 1} },
    { 10, { 1, 9, 36, 84, 126, 126, 84, 36, 9, 1} },
    { 11, { 1, 10, 45, 120, 210, 252, 210, 120, 45, 10, 1} },
    { 12, { 1, 11, 55, 165, 330, 462, 462, 330, 165, 55, 11, 1} },
    { 13, { 1, 12, 66, 220, 495, 792, 924, 792, 495, 220, 66, 12, 1} }
};

static unsigned char* s_ScratchMemRaw;
static unsigned char* s_ScratchMemEnd;
static unsigned char* s_ScratchMem;

void blur_init()
{
    for (int i=0; i<ARRAY_SIZE(s_PascalTriangle); ++i)
    {
        const size_t entries = s_PascalTriangle[i].m_Entries;
        float total = 0.0f;
        for (int j=0; j<entries; ++j)
            total += s_PascalTriangle[i].m_Data[j];
        
        int half = entries/2;
        if (entries&1)
            half++;
        s_PascalTriangle[i].m_NumCoeffs = half;
        for (int j=0; j<=half; ++j)
            s_PascalTriangle[i].m_Coeffs[j] = s_PascalTriangle[i].m_Data[half-j-1] / total;
    }
    
    // jiv fixme
    s_ScratchMemRaw = (unsigned char*) malloc(64*1024+15);
    s_ScratchMemEnd = s_ScratchMemRaw+(64*1024+15);
    s_ScratchMem = (unsigned char*)(((uintptr_t) s_ScratchMemRaw)+15 & ~15);
}

void blur2d(float* values, size_t width, size_t height, int kernel_width, const bitvector_t* mask)
{
    bitvector_t* row_mask = bitvector_init_heap(width);
    bitvector_t* column_mask = bitvector_init_heap(height);
    
    // blur row
    for (int r=0; r<height; ++r)
    {
        bitvector_copy(row_mask, mask, r*width, width);
        blur(&values[r*width], width, kernel_width, row_mask);
    }
    
    float* temp = (float*) malloc(height*sizeof(float));
    
    for (int c=0; c<width; ++c)
    {
        for (int r=0; r<height; ++r)
        {
            temp[r] = values[r*width+c];
            bitvector_set(column_mask, r, BITVECTOR_GET(mask, r*width+c));
        }
        
        // blur column
        blur(temp, height, kernel_width, column_mask);
        
        for (int r=0; r<height; ++r)
            values[r*width+c] = temp[r];
    }
    free(temp);
    
    bitvector_destroy_heap(row_mask);
    bitvector_destroy_heap(column_mask);
}

static float sample(float* values, const size_t values_length, float value_index, float weight, int default_index, const bitvector_t* mask)
{
    if (value_index < 0 || value_index > values_length-1 || !BITVECTOR_GET(mask, value_index))
        value_index = default_index;
    int next_index = (int) (value_index+1);
    if (next_index < 0 || next_index > values_length-1 || !BITVECTOR_GET(mask, next_index))
        next_index = default_index;
    
    const float base_index = floorf(value_index);
    const float frac = value_index - base_index;
    
    const float sample = values[(int)base_index]*(1.0f-frac) + values[next_index]*frac;
    return sample*weight;
}

void blur(float* values, size_t length, int kernel_width, const bitvector_t* mask)
{
    kernel_width = mini(kernel_width, 12);
    if (kernel_width <= 0)
        return;
    
    float* temp = (float*)s_ScratchMem;
    memcpy(temp, values, sizeof(float)*length);
    
    const float* coeffs = s_PascalTriangle[kernel_width].m_Coeffs;
    const int num_coeffs = s_PascalTriangle[kernel_width].m_NumCoeffs;
    const int is_odd = (s_PascalTriangle[kernel_width].m_Entries & 1) == 1;
    const float initial_weight = is_odd ? coeffs[0] : 0;
    const int   start_itr      = is_odd ? 1         : 0;
    
    // printf("kernel_width: %d num_coeffs %d is_odd %d\n", kernel_width, num_coeffs, is_odd);
    
    for (int i=0; i<length; ++i)
    {
        if (BITVECTOR_GET(mask, i))
        {
            float val = values[i] * initial_weight;
            
            for (int j=start_itr; j<num_coeffs; ++j)
            {
                const float offset = ((is_odd&1) == 1) ? 0.0f : 0.5f;
                val += sample(values, length, i+j-offset, coeffs[j], i, mask);
                val += sample(values, length, i-j+offset, coeffs[j], i, mask);
            
            }
            temp[i] = val;
        }
        else
        {
            temp[i] = values[i];
        }
        assert((int*)&temp[i] < (int*)s_ScratchMemEnd);
    }
    
    memcpy(values, temp, sizeof(float)*length);
}
