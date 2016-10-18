// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-

#include "cda.h"
#include "Common/util.h"

// test enum:
// 0 -> left
// 1 -> top
// 
// 
void cda_init(float* heights, const unsigned char* input, const int* islands, int island, size_t width, size_t height, int test_dir, int threshold)
{
#define RC(r,c) ((r)*width+(c))
    
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            int sample_is_alpha = input[4*RC(r,c)+3] < threshold;
            
            int sample_is_alpha_right = input[4*RC(r,mini(width-1, c+1))+3] < threshold;
            int island_right = islands[RC(r,mini(width-1, c+1))];
            if (island_right != island)
                sample_is_alpha_right = 1;
            
            int sample_is_alpha_top = input[4*RC(maxi(0,r-1),c)+3] < threshold;
            int island_top = islands[RC(maxi(0,r-1),c)];
            if (island_top != island)
                 sample_is_alpha_top = 1;
            
            if (test_dir == 0 || test_dir == 2)
            {
                if (sample_is_alpha && !sample_is_alpha_right)
                    heights[RC(r,c)] = 0.0f;
                else
                    heights[RC(r,c)] = 40000; //FLT_MAX;
            }
            if (test_dir == 1 || test_dir == 2)
            {
                if (sample_is_alpha && !sample_is_alpha_top)
                    heights[RC(r,c)] = 0.0f;
                else
                    heights[RC(r,c)] = 40000; //FLT_MAX;
            }
        }
    }
#undef RC
}

void cda_iterate(float* heights, const unsigned char* input, size_t width, size_t height, int threshold)
{
#define RC(r,c) ((r)*width+(c))
    
    int forwardWeightsCda[7][7] =
    {
        {  0, 43, 38,  0, 38, 43,  0 },
        { 43,  0, 27,  0, 27,  0, 43 },
        { 38, 27, 17, 12, 17, 27, 38 },
        {  0,  0, 12,  0,  0,  0,  0 },
        {  0,  0,  0,  0,  0,  0,  0 },
        {  0,  0,  0,  0,  0,  0,  0 },
        {  0,  0,  0,  0,  0,  0,  0 }
    };
    int backwardWeightsCda[7][7] =
    {
        {  0,  0,  0,  0,  0,  0,  0 },
        {  0,  0,  0,  0,  0,  0,  0 },
        {  0,  0,  0,  0,  0,  0,  0 },
        {  0,  0,  0,  0, 12,  0,  0 },
        { 38, 27, 17, 12, 17, 27, 38 },
        { 43,  0, 27,  0, 27,  0, 43 },
        {  0, 43, 38,  0, 38, 43,  0 }
    };
    
    int dimr = ARRAY_SIZE_2R(forwardWeightsCda);
    int dimc = ARRAY_SIZE_2C(forwardWeightsCda);
    
    // forwards
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            if (input[4*RC(r,c)+3] < threshold)
                continue;
            
            for (int i=0; i<dimr; ++i)
            {
                for (int j=0; j<dimc; ++j)
                {
                    int rowOffset = i-dimr/2;
                    int columnOffset = j-dimc/2;
                    
                    if (r+rowOffset < 0 || r+rowOffset >= height)
                        continue;
                    if (c+columnOffset < 0 || c+columnOffset >= width)
                        continue;
                    
                    int weight = forwardWeightsCda[i][j];
                    if (weight == 0)
                        continue;
                    
                    int index_me = RC(r,c);
                    int index_test = RC(r+rowOffset,c+columnOffset);
                    
                    if (heights[index_test] < heights[index_me] - weight)
                    {
                        heights[index_me] = heights[index_test] + weight;
                    }
                }
            }
        }
    }
    
    // backwards
    for (int r=height-1; r>=0; --r)
    {
        for (int c=width-1; c>=0; --c)
        {
            if (input[4*RC(r,c)+3] < threshold)
                continue;
            
            for (int i=0; i<dimr; ++i)
            {
                for (int j=0; j<dimc; ++j)
                {
                    int rowOffset = i-dimr/2;
                    int columnOffset = j-dimc/2;
                    
                    if (r+rowOffset < 0 || r+rowOffset >= height)
                        continue;
                    if (c+columnOffset < 0 || c+columnOffset >= width)
                        continue;
                    
                    int weight = backwardWeightsCda[i][j];
                    if (weight == 0)
                        continue;
                    
                    int index_me = RC(r,c);
                    int index_test = RC(r+rowOffset,c+columnOffset);
                    
                    if (heights[index_test] < heights[index_me] - weight)
                        heights[index_me] = heights[index_test] + weight;
                }
            }
        }
    }
#undef RC
}

float cda_normalize(float* height_map, const unsigned char* image, const int* islands, int island, size_t width, size_t height, int threshold)
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

