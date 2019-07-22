#ifndef DEVICE_KERNEL_WRAPPER_H
#define DEVICE_KERNEL_WRAPPER_H

#include "common.h"
#include "datatypes.h"

void device_chain_kernel_wrapper(
        std::string binaryFile,
        std::vector<anchor_dt, aligned_allocator<anchor_dt> >& arg,
        std::vector<return_dt, aligned_allocator<return_dt> >& ret,
        int max_dist_x, int max_dist_y, int bw);

#endif // DEVICE_KERNEL_WRAPPER_H
