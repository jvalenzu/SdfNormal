float maxf(float a, float b)
{
    if (a<b)
        return b;
    return a;
}

float minf(float a, float b)
{
    if (a<b)
        return a;
    return b;
}

float clampf(float value, float m0, float m1)
{
    if (m0 < m1)
    {
        value = max(m0, value);
        value = min(m1, value);
    }
    else
    {
        value = max(m1, value);
        value = min(m0, value);
    }
    return value;
}

__kernel void borderPixels(__global int* emitters,
                           unsigned int num_positive_emitters,
                           unsigned int num_negative_emitters,
                           __global unsigned char* input, __global int* output,
                           unsigned int width, unsigned int height,
                           __global int* islands, unsigned int num_islands,
                           int island)
{
    int index = get_global_id(0);
    int pixel_r = (int) (index / width);
    int pixel_c = index - (pixel_r*width);
    int pixel_alpha = index*4+3;
    
    if (input[pixel_alpha] < 1)
    {
        return;
    }
    
    if (islands[index] != island)
        return;
    
    int pos_r = pixel_r;
    int pos_c = pixel_c;
    float min_dist = 10e23f;
    for (int i=0; i<num_positive_emitters; ++i)
    {
        int r = emitters[i]>>16;
        int c = emitters[i];
        r &= 0xffff;
        c &= 0xffff;
        
        int dr = r - pixel_r;
        int dc = c - pixel_c;
        int dr2 = dr*dr;
        int dc2 = dc*dc;
        
        float dist = sqrt(1.0f*dr2+1.0f*dc2);
        if (dist < min_dist)
        {
            pos_r = r;
            pos_c = c;
            
            min_dist = dist;
        }
    }
    
    output[index] = pos_r<<16 | pos_c;
}

int maxi(int a, int b)
{
    if (a<b)
        return b;
    return a;
}

int mini(int a, int b)
{
    if (a<b)
        return a;
    return b;
}

int bitvector_get(__global unsigned int* bitvector, unsigned int index)
{
    unsigned int word_index = index>>5;
    unsigned int bit_index = index&31;
    return (bitvector[word_index] >> bit_index)&1;
}

#include "gen_src/pvrt.cl.inc"

__kernel void finalize_image(__global unsigned char* output, __global unsigned char* input,
                             __global float* intensity_r, __global float* intensity_g,
                             unsigned int width, unsigned int height,
                             __global int* island_mask, int island,
                             __global unsigned char* original_image)
{
    const int index = get_global_id(0);
    
    int pixel_offset = 4*index;
    
    float rC = clampf(intensity_r[index], 0.0f, 1.0f);
    float gC = clampf(intensity_g[index], 0.0f, 1.0f);
    float bC = 0.0f;
    
    if (island_mask[index] == island)
    {
        output[pixel_offset+0] = (unsigned char) (255.0f * rC);
        output[pixel_offset+1] = (unsigned char) (255.0f * gC);
        output[pixel_offset+2] = (unsigned char) (255.0f * bC);
        output[pixel_offset+3] = input[pixel_offset+3];
    }
    else
    {
        output[pixel_offset+0] = original_image[pixel_offset+0];
        output[pixel_offset+1] = original_image[pixel_offset+1];
        output[pixel_offset+2] = original_image[pixel_offset+2];
        output[pixel_offset+3] = original_image[pixel_offset+3];
    }
}
