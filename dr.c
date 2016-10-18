// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#include <math.h>

#include "cda.h"
#include "Common/Util.h"
#include "Common/Vec.h"

#define RC(r,c) ((r)*width+(c))

void dr_init(float* heights, border_pixel_t* border_pixels, const unsigned char* input, const int* islands, int island, size_t width, size_t height, int threshold)
{
    // initialized vector field
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            border_pixel_t p;
            p.m_R = r;
            p.m_C = c;
            
            border_pixels[RC(r,c)] = p;
            heights[RC(r,c)] = 2e20f; // arbitrary but big
        }
    }
    
    // initialize immediate interior & exterior elements
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            int sample_is_alpha = input[4*RC(r,c)+3] < threshold;
            if (!sample_is_alpha)
                continue;
            
            // 0 1 2
            // 3   4
            // 5 6 7
            int populated1 = input[4*RC(maxi(r-1,0),                       c)+3] > threshold;
            int populated6 = input[4*RC(mini(r+1,height-1),                c)+3] > threshold;
            int populated3 = input[4*RC(r,                       maxi(c-1,0))+3] > threshold;
            int populated4 = input[4*RC(r,                 mini(c+1,width-1))+3] > threshold;
            
            int neighborPopulated = populated1 || populated6 || populated3 || populated4;
            if (neighborPopulated)
            {
                heights[RC(r,c)] = 0.0f;
                border_pixel_t* p = &border_pixels[RC(r,c)];
                p->m_R = r;
                p->m_C = c;
            }
        }
    }
}

void dr_iterate(float* heights, border_pixel_t* border_pixels, const unsigned char* input, size_t width, size_t height, int threshold)
{
    float root2 = (float) M_SQRT2;
    float forwardWeightsDr[3][3] =
    {
        {  root2, 1, root2 },
        {      1, 0, 0     },
        {      0, 0, 0     }
    };
    float backwardWeightsDr[3][3] =
    {
        {      0, 0, 0     },
        {      0, 0, 1     },
        {  root2, 1, root2 },
    };
    
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            for (int ri=0; ri<3; ++ri)
            {
                for (int ci=0; ci<3; ++ci)
                {
                    int rowOffset = ri-1;
                    int columnOffset = ci-1;
                    
                    if (r+rowOffset<0    || r+rowOffset>=height)
                        continue;
                    if (c+columnOffset<0 || c+columnOffset>=width)
                        continue;
                    float weight = forwardWeightsDr[ri][ci];
                    if (weight == 0.0f)
                        continue;
                    
                    float v = heights[RC(r+rowOffset,c+columnOffset)];
                    if (v < heights[RC(r,c)]-weight)
                    {
                        border_pixel_t p = border_pixels[RC(r,c)] = border_pixels[RC(r+rowOffset,c+columnOffset)];
                        heights[RC(r,c)] = euclidian_distance(p.m_R, p.m_C, r, c);
                    }
                }
            }
        }
    }
    
    for (int r=height-1; r>=0; --r)
    {
        for (int c=width-1; c>=0; --c)
        {
            for (int ri=0; ri<3; ++ri)
            {
                for (int ci=0; ci<3; ++ci)
                {
                    int rowOffset = ri-1;
                    int columnOffset = ci-1;
                    
                    if (r+rowOffset<0    || r+rowOffset>=height)
                        continue;
                    if (c+columnOffset<0 || c+columnOffset>=width)
                        continue;
                    float weight = backwardWeightsDr[ri][ci];
                    if (weight == 0.0f)
                        continue;
                    
                    float v = heights[RC(r+rowOffset, c+columnOffset)];
                    if (v < heights[RC(r,c)]-weight)
                    {
                        border_pixel_t p = border_pixels[RC(r,c)] = border_pixels[RC(r+rowOffset,c+columnOffset)];
                        heights[RC(r,c)] = euclidian_distance(p.m_R, p.m_C, r, c);
                    }
                }
            }
        }
    }
            
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            int sample_is_alpha = input[4*RC(r,c)+3] < threshold;
            if (sample_is_alpha)
                heights[RC(r,c)] = 0.0f;
        }
    }
}

float dr_normalize(float* height_map, const unsigned char* image, const int* islands, int island, size_t width, size_t height, int threshold)
{
    // normalize
    float max_height = 0.0f;
    for (int i=0; i<width*height; ++i)
    {
        int sample_is_alpha = image[4*i+3] < threshold;
        if (sample_is_alpha)
            continue;
        
        int sample_island = islands[i];
        if (sample_island != island)
            continue;
        
        float height = height_map[i];
        if (height < 0)
            continue;
        max_height = maxf(height, max_height);
    }
    for (int i=0; i<width*height; ++i)
    {
        if (height_map[i] < 0)
            continue;
        int sample_island = islands[i];
        if (sample_island != island)
            continue;
        
        height_map[i] /= max_height;
    }
    
    return max_height;
}
