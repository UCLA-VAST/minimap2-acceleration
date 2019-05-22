#ifndef DEVICE_KERNEL_WRAPPER_H
#define DEVICE_KERNEL_WRAPPER_H

#include "datatypes.h"

void device_chain_kernel_wrapper(
        std::vector<control_dt> &cont,
        std::vector<anchor_dt> &arg,
        std::vector<return_dt> &ret,
        int max_dist_x, int max_dist_y, int bw);

#endif // DEVICE_KERNEL_WRAPPER_H
