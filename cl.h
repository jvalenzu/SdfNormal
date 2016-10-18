// -*- mode: c++; tab-width: 4; c-basic-offset: 4; -*-
#include <sys/types.h>

void        cl_pfn_notify(const char *errinfo, const void *private_info, size_t cb, void *user_data);
const char* cl_error_to_string(int err);

