// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-

#include <assert.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <OpenCL/opencl.h>

#include "External/lodepng.h"
#include "Container/Bit.h"
#include "Common/Vec.h"
#include "Common/Util.h"
#include "Tool/Queue.h"
#include "cl.h"
#include "dr.h"

float poly(float t)
{
    float t3 = t*t*t;
    float t4 = t3*t;
    float t5 = t4*t;
    
    return 6*t5-15*t4+10*t3;
}

static int island_init(int* islands, int* islands_count, unsigned char* image, size_t width, size_t height, int threshold);

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    const char* options[] =
    {
        "--input_file=%s",
        "--binary_program=%s",
        "--kernel_width=%s",
        "--blur_passes=%s",
        "--alpha_threshold=%s",
        "--islands=%s"
    };
    char values[6][64];
    memset(values, 0, sizeof values);
    
    for (int j=1; j<argc; ++j)
    {
        int c=0;
        for (int i=0; i<ARRAY_SIZE(options); ++i)
        {
            c+=sscanf(argv[j], options[i], values[i]);
        }
        if (c==0)
        {
            printf("Unhandled command line argument %s\n", argv[j]);
        }
    }
    
    const char* input_file = values[0][0] == 0 ? "Test0.png" : values[0];
    const char* bin_program = values[1][0] == 0 ? "pvrt.cl.gpu_64.bc" : values[1];
    int kernel_width = values[2][0] == 0 ? 7 : atoi(values[2]);
    const int blur_passes = values[3][0] == 0 ? 32 : atoi(values[3]);
    const int alpha_threshold = values[4][0] == 0 ? 1 : atoi(values[4]);
    const int visualize_islands = values[5][0] == 0 ? 0 : atoi(values[5]);
    
    struct stat buf;
    if (stat(input_file, &buf) < 0 || stat(bin_program, &buf) < 0)
    {
        printf("invalid one of input_file: %s bin_program: %s\n", input_file, bin_program);
        exit(1);
    }
    
    char output_file[256];
    strncpy(output_file, input_file, sizeof output_file);
    char* ext = strstr(output_file, ".png");
    if (ext)
        strcpy(ext, "_OUTPUT.png");
    else
        exit(1);
    
    unsigned char* image;
    unsigned int width, height;
    {
        // get image data
        unsigned char* png;
        size_t png_size;
        lodepng_load_file(&png, &png_size, input_file);
        unsigned int error = lodepng_decode32(&image, &width, &height, png, png_size);
        if (error)
        {
            printf("error %u: %s\n", error, lodepng_error_text(error));
            exit(1);
        }
        else
        {
            printf("%s width/height/png_size %d/%d/%zu\n", input_file, width, height, png_size);
        }
    }
    
    if ((kernel_width & 1) == 0)
        kernel_width++;
    kernel_width = mini(kernel_width, MAX_KERNEL_WIDTH);
    
    char blur_kernel_x[64];
    snprintf(blur_kernel_x, sizeof blur_kernel_x, "blur2d_%dx", kernel_width);
    char blur_kernel_y[64];
    snprintf(blur_kernel_y, sizeof blur_kernel_y, "blur2d_%dy", kernel_width);
    
    printf("kernel_width: %d %s %s\n", kernel_width, blur_kernel_x, blur_kernel_y);
    
    // height map data (and CPU size verification)
    // generate islands
    int* islands = (int*) malloc(sizeof(int)*width*height);
    int* islands_count = (int*) malloc(sizeof(int)*width*height);
    memset(islands, 0xffffffff, sizeof(int)*width*height);
    memset(islands_count, 0, sizeof(int)*width*height);
    
    // Connect to a compute device
    //
    cl_device_id device_ids[2];
    cl_uint num_devices;
    int err = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 2, device_ids, &num_devices);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to create a device group!\n");
        return EXIT_FAILURE;
    }
    // cl_device_id device_id = device_ids[0];
    cl_device_id device_id = device_ids[1];
    printf("num_devices: %d\n", num_devices);
    
    // Create a compute context 
    //
    cl_context context = clCreateContext(0, 1, &device_id, cl_pfn_notify, NULL, &err);
    if (!context)
    {
        printf("Error: Failed to create a compute context!\n");
        return EXIT_FAILURE;
    }
    
    // Create a command commands
    //
    cl_command_queue commands = clCreateCommandQueue(context, device_id, 0, &err);
    if (!commands)
    {
        printf("Error: Failed to create a command commands!\n");
        return EXIT_FAILURE;
    }
    
    unsigned char* binary = NULL;
    size_t binarySize = 0;
    
    {
        // Get binary
        FILE* fh = fopen(bin_program, "rb");
        if (!fh)
        {
            fprintf(stderr, "missing %s\n", bin_program);
            exit(1);
        }
        
        fseek(fh, 0, SEEK_END);
        binarySize = (size_t) ftell(fh);
        
        fseek(fh, 0, SEEK_SET);
        binary = (unsigned char*) malloc(binarySize);
        fread(binary, binarySize, 1, fh);
        fclose(fh);
    }
    
    // Create the compute program from the binary
    //
    err = CL_SUCCESS;
    cl_int status = 0;
    cl_program program = clCreateProgramWithBinary(context, 1, &device_id, &binarySize, (const unsigned char **) &binary, &status, &err);
    if (!program || err != CL_SUCCESS)
    {
        printf("Error: Failed to create compute program: deviceId: %p size %ld status: %d error: %d\n", device_id, binarySize, status, err);
        return EXIT_FAILURE;
    }
    
    free(binary);
    
    switch (status)
    {
        case CL_INVALID_VALUE:
        {
            printf("invalid value\n");
            break;
        }
        case CL_INVALID_BINARY:
        {
            printf("invalid binary\n");
            break;
        }
    }
    
    // Build the program executable
    err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];
        
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof buffer, buffer, &len);
        printf("%s error: %d\n", buffer, err);
        exit(1);
    }
    
    // Create the compute kernel in the program we wish to run
    cl_kernel kernel = clCreateKernel(program, "borderPixels", &err);
    if (!kernel || err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];
        
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("Error: Failed to create compute kernel: str: %s err: %d\n", buffer, err);
        exit(1);
    }
    
    int* border_pixels = (int*)malloc(sizeof(int)*width*height);
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            border_pixels[r*width+c] = r<<16|c;
        }
    }
    
    int num_islands = island_init(islands, islands_count, image, width, height, alpha_threshold);
    printf("%d islands identified\n", num_islands);
    
    // use dead reckoning to determine the most appropriate normal per pixel
    if (!visualize_islands)
    {
        const int min_pixels = 1;//jiv fixmewidth*height/20;
        
        // save off temporary
        unsigned char* verify_height_map_image_data = (unsigned char*) malloc(width*height*4);
        memset(verify_height_map_image_data, 0, width*height*4);
        
        for (int i=0; i<num_islands; ++i)
        {
            // printf("island %d has %d pixels\n", i, islands_count[i]);
            if (islands_count[i] < min_pixels)
            {
                printf("reject island %d had %d pixels\n", i, islands_count[i]);
                continue;
            }
            
            int island = i;
            
            float* height_map = (float*) malloc(sizeof(float)*width*height);
            border_pixel_t* border_pixels = (border_pixel_t*) malloc(sizeof(border_pixel_t)*width*height);
            
            dr_init(height_map, border_pixels, image, islands, island, width, height, alpha_threshold);
            dr_iterate(height_map, border_pixels, image, width, height, alpha_threshold);
            dr_normalize(height_map, image, islands, island, width, height, alpha_threshold);
            
            float* intensity_r = (float*) malloc(sizeof(float)*width*height);
            float* intensity_g = (float*) malloc(sizeof(float)*width*height);
            for (int r=0; r<height; ++r)
            {
                for (int c=0; c<width; ++c)
                {
                    int index = r*width+c;
                    int pixel_offset = 4*index;
                    
                    if (islands[index] != island)
                    {
                        intensity_r[index] = intensity_g[index] = 0.0f;
                        continue;
                    }
                    
                    float rC, gC, bC;
                    bC = image[pixel_offset+2] / 255.0f;
                    
                    border_pixel_t p = border_pixels[index];
                    rC = p.m_C;
                    gC = p.m_R;
                    vec_sub(&rC, &gC, c, r);
                    normalize2(&rC, &gC);
                    
                    rC = to_zero_one(rC);
                    gC = 1.0f - to_zero_one(gC);
                    bC = 0.0f;
                    
                    const float bone = 0.0;
                    // const float btwo = 0.6f;
                    const float btwo = 0.9f;
                    
                    float sample = height_map[index];
                    if (sample >= btwo)
                    {
                        rC = 0.5f;
                        gC = 0.5f;
                    }
                    else if (sample >= bone)
                    {
                        float t = (sample - bone) / (btwo-bone);
                        t = poly(t);
                        rC = (1.0f-t)*rC + t*0.5f;
                        gC = (1.0f-t)*gC + t*0.5f;
                    }
                    
                    intensity_r[index] = rC;
                    intensity_g[index] = gC;
                }
            }
            
            // break up the lines by adding noise and blurring.
            for (int i=0; blur_passes>0 && i<width*height; ++i)
            {
                float sample = height_map[i];
                
                float r0 = 0.1f - 0.2f * (rand() / (float) RAND_MAX);
                float r1 = 0.1f - 0.2f * (rand() / (float) RAND_MAX);
                
                intensity_r[i] += r0;
                intensity_g[i] += r1;
            }
            
            // make alpha mask
            bitvector_t* mask = bitvector_init_heap(width*height);
            for (int r=0; r<height; ++r)
            {
                for (int c=0; c<width; ++c)
                {
                    const int index = (r*width + c);
                    const int address = 4*index;
                    const int alpha = image[address+3];
                    
                    const int value = alpha >= alpha_threshold ? 1 : 0;
                    bitvector_set(mask, index, value);
                }
            }
            
            // opencl blur
            cl_kernel blur2dx = clCreateKernel(program, blur_kernel_x, &err);
            cl_kernel blur2dy = clCreateKernel(program, blur_kernel_y, &err);
            cl_kernel finalize_image = clCreateKernel(program, "finalize_image", &err);
            if (!blur2dx || !blur2dy || !finalize_image || err != CL_SUCCESS)
            {
                size_t len;
                char buffer[2048];
                
                clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
                printf("Error: Failed to create compute kernel: str: %s err: %d\n", buffer, err);
                exit(1);
            }
            
            cl_event y_sync[2];
            
            cl_mem input0 = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*width*height, NULL, NULL);
            cl_mem input1 = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*width*height, NULL, NULL);
            cl_mem output0 = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*width*height, NULL, NULL);
            cl_mem output1 = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*width*height, NULL, NULL);
            cl_mem alpha_mask = clCreateBuffer(context, CL_MEM_READ_ONLY, bitvector_rawsize(mask), NULL, NULL);
            cl_mem island_mask = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int)*width*height, NULL, NULL);
            cl_mem original_image = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int)*width*height, NULL, NULL);
            
            if (!output0 || !output1 || !input0 || !input1 || !alpha_mask)
            {
                printf("Error: Failed to allocate device memory!\n");
                exit(1);
            }
            
            err = clEnqueueWriteBuffer(commands, alpha_mask, CL_TRUE, 0, bitvector_rawsize(mask), bitvector_getraw(mask), 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to bitvector mask\n");
                exit(1);
            }
            
            err = clEnqueueWriteBuffer(commands, island_mask, CL_TRUE, 0, sizeof(int)*width*height, islands, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to island mask\n");
                exit(1);
            }
            
            err = clEnqueueWriteBuffer(commands, original_image, CL_TRUE, 0, sizeof(int)*width*height, verify_height_map_image_data, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to original image\n");
                exit(1);
            }
            
            // execute kernel for each index in the image data
            size_t global_work_offset = 0;
            size_t local_work_size = 256;
            
            // provide image data input
            err = clEnqueueWriteBuffer(commands, input0, CL_TRUE, 0, sizeof(float)*width*height, intensity_r, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to input0\n");
                exit(1);
            }
            err = clEnqueueWriteBuffer(commands, input1, CL_TRUE, 0, sizeof(float)*width*height, intensity_g, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to input1\n");
                exit(1);
            }
            
            for (int i=0; i<blur_passes; ++i)
            {
                float* intensity_table[] = { intensity_r, intensity_g };
                cl_mem input_table[] = { input0, input1 };
                cl_mem output_table[] = { output0, output1 };
                cl_event x_sync[2];
                
                for (int j=0; j<2; ++j)
                {
                    float* intensity = intensity_table[j];
                    cl_mem input = input_table[j];
                    cl_mem output = output_table[j];
                    
                    //  __   __
                    // /\ \ /\ \
                    // \ `\`\/'/'
                    //  `\/ > <
                    //     \/'/\`\
                    //     /\_\\ \_\
                    //     \/_/ \/_/
                    //
                    // setup arguments
                    err = 0;
                    err |= clSetKernelArg(blur2dx, 0, sizeof(cl_mem), &input);
                    err |= clSetKernelArg(blur2dx, 1, sizeof(cl_mem), &output);
                    err |= clSetKernelArg(blur2dx, 2, sizeof(cl_mem), &alpha_mask);
                    err |= clSetKernelArg(blur2dx, 3, sizeof(unsigned int), &width);
                    err |= clSetKernelArg(blur2dx, 4, sizeof(unsigned int), &height);
                    if (err != CL_SUCCESS)
                    {
                        printf("Error: Failed to set kernel arguments! %d\n", err);
                        exit(1);
                    }
                    
                    size_t spill_size = (height*width) & 511;
                    size_t global_work_size = (height*width) & ~511;
                    size_t local_spill_size = 2;
                    
                    if (spill_size)
                    {
                        err = clEnqueueNDRangeKernel(commands, blur2dx, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, NULL);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                        err = clEnqueueNDRangeKernel(commands, blur2dx, 1, &global_work_size, &spill_size, &local_spill_size, 0, NULL, &x_sync[j]);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                    }
                    else
                    {
                        err = clEnqueueNDRangeKernel(commands, blur2dx, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &x_sync[j]);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                        
                    }
                }
                
                for (int j=0; j<2; ++j)
                {
                    float* intensity = intensity_table[j];
                    cl_mem input = input_table[j];
                    cl_mem output = output_table[j];
                    
                    // 
                    //  __    __
                    // /\ \  /\ \
                    // \ `\`\\/'/
                    //  `\ `\ /'
                    //    `\ \ \
                    //      \ \_\
                    //       \/_/
                    //
                    
                    clEnqueueWaitForEvents(commands, 1, &x_sync[j]);
                    
                    // setup arguments
                    err = 0;
                    err |= clSetKernelArg(blur2dy, 0, sizeof(cl_mem), &output);
                    err |= clSetKernelArg(blur2dy, 1, sizeof(cl_mem), &input);
                    err |= clSetKernelArg(blur2dy, 2, sizeof(cl_mem), &alpha_mask);
                    err |= clSetKernelArg(blur2dy, 3, sizeof(unsigned int), &width);
                    err |= clSetKernelArg(blur2dy, 4, sizeof(unsigned int), &height);
                    if (err != CL_SUCCESS)
                    {
                        printf("Error: Failed to set kernel arguments! %d\n", err);
                        exit(1);
                    }
                    
                    // execute kernel for each index in the image data
                    size_t spill_size = (height*width) & 511;
                    size_t global_work_size = (height*width) & ~511;
                    size_t local_spill_size = 2;
                    
                    if (spill_size)
                    {
                        err = clEnqueueNDRangeKernel(commands, blur2dy, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, NULL);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                        
                        err = clEnqueueNDRangeKernel(commands, blur2dy, 1, &global_work_size, &spill_size, &local_spill_size, 0, NULL, &y_sync[j]);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                    }
                    else
                    {
                        err = clEnqueueNDRangeKernel(commands, blur2dy, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &y_sync[j]);
                        if (err)
                        {
                            printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                            return EXIT_FAILURE;
                        }
                    }
                }
            }
            
            // now that we have all our data, assemble the r/g intensities into one image map
            cl_event image_sync;
            err = clEnqueueWriteBuffer(commands, output1, CL_TRUE, 0, sizeof(int)*width*height, image, 0, NULL, &image_sync);
            if (err != CL_SUCCESS)
            {
                printf("Error: Failed to write to input1\n");
                exit(1);
            }
            
            err = 0;
            err |= clSetKernelArg(finalize_image, 0, sizeof(cl_mem), &output0);
            err |= clSetKernelArg(finalize_image, 1, sizeof(cl_mem), &output1);
            err |= clSetKernelArg(finalize_image, 2, sizeof(cl_mem), &input0);
            err |= clSetKernelArg(finalize_image, 3, sizeof(cl_mem), &input1);
            err |= clSetKernelArg(finalize_image, 4, sizeof(unsigned int), &width);
            err |= clSetKernelArg(finalize_image, 5, sizeof(unsigned int), &height);
            err |= clSetKernelArg(finalize_image, 6, sizeof(cl_mem), &island_mask);
            err |= clSetKernelArg(finalize_image, 7, sizeof(unsigned int), &island);
            err |= clSetKernelArg(finalize_image, 8, sizeof(cl_mem), &original_image);
            
            if (blur_passes > 0)
                clEnqueueWaitForEvents(commands, 2, y_sync);
            clEnqueueWaitForEvents(commands, 1, &image_sync);
            
            size_t spill_size = (height*width) & 511;
            size_t global_work_size = (height*width) & ~511;
            size_t local_spill_size = 2;
            
            // execute kernel for each index in the image data
            err = clEnqueueNDRangeKernel(commands, finalize_image, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, NULL);
            if (err)
            {
                printf("Error: Failed to execute kernel: %s\n", cl_error_to_string(err));
                return EXIT_FAILURE;
            }
            if (spill_size)
            {
                err = clEnqueueNDRangeKernel(commands, finalize_image, 1, &global_work_size, &spill_size, &local_spill_size, 0, NULL, NULL);
                assert(err == CL_SUCCESS);
            }
            
            err = clEnqueueReadBuffer(commands, output0, CL_TRUE, 0, sizeof(int)*width*height, verify_height_map_image_data, 0, NULL, NULL);
            assert(err == CL_SUCCESS);
            
            clFinish(commands);
            printf("finalizing\n");
            fflush(stdout);
            
            clReleaseMemObject(input0);
            clReleaseMemObject(input1);
            clReleaseMemObject(output0);
            clReleaseMemObject(output1);
            clReleaseMemObject(alpha_mask);
            clReleaseKernel(blur2dx);
            clReleaseKernel(blur2dy);
            clReleaseKernel(finalize_image);
            
            bitvector_destroy_heap(mask);
            
            free(intensity_r);
            free(intensity_g);
            
            free(height_map);
            free(border_pixels);
        }
        
        free(islands);
        
        // write out final image.
        printf("Writing %s\n", output_file);
        
        int encode_error;
        if ((encode_error = lodepng_encode32_file(output_file, verify_height_map_image_data, width, height)))
        {
            printf("error %u: %s\n", encode_error, lodepng_error_text(encode_error));
        }
        free(verify_height_map_image_data);
    }
    else
    {
        printf("Visualizing islands\n");
        
        unsigned char* island_visualization = (unsigned char*) malloc(width*height*4);
        
        uint8_t colors[] =
        {
            255,
            0,
            80,
            160,
            128
        };
        
        for (int r=0; r<height; ++r)
        {
            for (int c=0; c<width; ++c)
            {
                const int index = r*width+c;
                const int pixel_offset = 4*index;
                const int island = islands[index];
                
                int r=0,g=0,b=0;
                
                if (image[pixel_offset+3] > alpha_threshold)
                {
                    r = colors[island%ARRAY_SIZE(colors)];
                    g = colors[(2*island+1)%ARRAY_SIZE(colors)];
                    b = colors[(3*island+1)%ARRAY_SIZE(colors)];
                }
                
                island_visualization[pixel_offset+0] = r;
                island_visualization[pixel_offset+1] = g;
                island_visualization[pixel_offset+2] = b;
                island_visualization[pixel_offset+3] = image[pixel_offset+3];
            }
        }
        
        
        int encode_error;
        if ((encode_error = lodepng_encode32_file(output_file, island_visualization, width, height)))
        {
            printf("error %u: %s\n", encode_error, lodepng_error_text(encode_error));
        }
        free(island_visualization);
    }
    
    // Shutdown and cleanup
    //
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);
    
    return 0;
}

static int island_init(int* islands, int* islands_count, unsigned char* image, size_t width, size_t height, int threshold)
{
    int rSteps[] = { -1, 1,  0, 0, -1, -1,  1, 1 };
    int cSteps[] = {  0, 0, -1, 1, -1,  1, -1, 1 };
    
    memset(islands, 0xffffffff, width*height*sizeof(int));
    
    // identify islands
    int islandCount = 0;
    for (int r=0; r<height; ++r)
    {
        for (int c=0; c<width; ++c)
        {
            int index = r*width+c;
            int offset = 4*index;
            int sample_alpha = image[offset+3];
            if (sample_alpha < threshold)
                continue;
            
            // check immediate neighbors
            for (int i=0; islands[index]<0 && i<ARRAY_SIZE(rSteps); ++i)
            {
                int r_t = r + rSteps[i];
                int c_t = c + cSteps[i];
                
                if (r_t >= 0 && r_t < height && c_t >= 0 && c_t < width)
                {
                    int index_t = r_t * width + c_t;
                    int offset_t = index_t*4;
                    
                    int sample_alpha = image[offset_t+3];
                    if (sample_alpha < threshold)
                        continue;
                    
                    // if immediate neighbor has an island, we must share it
                    if (islands[index_t] != -1)
                    {
                        islands[r*width+c] = islands[index_t];
                        break;
                    }
                }
            }
            
            // if we didn't find an island value, we need to create a new one and flood fill.
            if (islands[index] < 0)
            {
                struct pixel_location
                {
                    int m_R;
                    int m_C;
                } pl;
                
                queue_t pixels_to_fill;
                queue_init(&pixels_to_fill, sizeof pl);
                
                pl.m_R = r; pl.m_C = c;
                queue_enqueue(&pixels_to_fill, &pl);
                
                // assign a new island
                int island = islandCount++;
                islands[index] = island;
                
                int pixels_colored = 1;
                
                while (queue_count(&pixels_to_fill)>0)
                {
                    // pop off our current pixel location
                    queue_dequeue(&pixels_to_fill, &pl);
                    
                    // check immediate neighbors
                    for (int i=0; i<ARRAY_SIZE(rSteps); ++i)
                    {
                        int neighbor_r = pl.m_R + rSteps[i];
                        int neighbor_c = pl.m_C + cSteps[i];
                        
                        // skip locations that are outside the image
                        if (neighbor_r < 0 || neighbor_r >= height)
                            continue;
                        if (neighbor_c < 0 || neighbor_c >= width)
                            continue;
                        
                        int index_t = neighbor_r*width+neighbor_c;
                        int offset_t = 4*index_t;
                        
                        int sample_alpha = image[offset_t+3];
                        if (sample_alpha >= threshold && islands[index_t] == -1)
                        {
                            // we've hit a pixel, and it's uncolored, so populate it with our island
                            islands[index_t] = island;
                            
                            pixels_colored++;
                            
                            // we'll need to visit this pixel's neighbors.
                            pl.m_R = neighbor_r; pl.m_C = neighbor_c;
                            queue_enqueue(&pixels_to_fill, &pl);
                        }
                    }
                }
                
                queue_destroy(&pixels_to_fill);
                islands_count[island] = pixels_colored;
            }
        }
    }
    
    return islandCount;
}
