// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#include <OpenCL/opencl.h>
#include <assert.h>
#include <stdio.h>

#include "cl.h"

void cl_pfn_notify(const char *errinfo, const void *private_info, size_t cb, void *user_data)
{
    printf("pfn_notify: %s\n", errinfo);
    assert(false);
}

const char* cl_error_to_string(int err)
{
    switch (err)
    {
        case CL_INVALID_PROGRAM_EXECUTABLE: return "if there is no successfully built program executable available for device associated with command_queue.";
        case CL_INVALID_COMMAND_QUEUE: return "if command_queue is not a valid command-queue.";
        case CL_INVALID_KERNEL: return "if kernel is not a valid kernel object.";
        case CL_INVALID_CONTEXT: return "if context associated with command_queue and kernel is not the same or if the context associated with command_queue and events in event_wait_list are not the same.";
        case CL_INVALID_KERNEL_ARGS: return "if the kernel argument values have not been specified.";
        case CL_INVALID_WORK_DIMENSION: return "if work_dim is not a valid value (i.e. a value between 1 and 3).";
        case CL_INVALID_WORK_GROUP_SIZE: return "if local_work_size is specified and number of work-items specified by global_work_size is not evenly divisable by size of work-group given by local_work_size or does not match the work-group size specified for kernel using the __attribute__((reqd_work_group_size(X, Y, Z))) qualifier in program source.";
            // case CL_INVALID_WORK_GROUP_SIZE: return "if local_work_size is specified and the total number of work-items in the work-group computed as local_work_size[0] *... local_work_size[work_dim - 1] is greater than the value specified by CL_DEVICE_MAX_WORK_GROUP_SIZE in the table of OpenCL Device Queries for clGetDeviceInfo.";
            // case CL_INVALID_WORK_GROUP_SIZE: return "if local_work_size is NULL and the __attribute__((reqd_work_group_size(X, Y, Z))) qualifier is used to declare the work-group size for kernel in the program source.";
        case CL_INVALID_WORK_ITEM_SIZE: return "if the number of work-items specified in any of local_work_size[0], ... local_work_size[work_dim - 1] is greater than the corresponding values specified by CL_DEVICE_MAX_WORK_ITEM_SIZES[0], .... CL_DEVICE_MAX_WORK_ITEM_SIZES[work_dim - 1].";
        case CL_INVALID_GLOBAL_OFFSET: return "if global_work_offset is not NULL.";
        case CL_OUT_OF_RESOURCES: return "if there is a failure to queue the execution instance of kernel on the command-queue because of insufficient resources needed to execute the kernel. For example, the explicitly specified local_work_size causes a failure to execute the kernel because of insufficient resources such as registers or local memory. Another example would be the number of read-only image args used in kernel exceed the CL_DEVICE_MAX_READ_IMAGE_ARGS value for device or the number of write-only image args used in kernel exceed the CL_DEVICE_MAX_WRITE_IMAGE_ARGS value for device or the number of samplers used in kernel exceed CL_DEVICE_MAX_SAMPLERS for device.";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "if there is a failure to allocate memory for data store associated with image or buffer objects specified as arguments to kernel.";
        case CL_INVALID_EVENT_WAIT_LIST: return "if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.";
        case CL_OUT_OF_HOST_MEMORY: return "if there is a failure to allocate resources required by the OpenCL implementation on the host.";
    }
    return "unknown";
}
