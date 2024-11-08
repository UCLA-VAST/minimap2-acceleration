#ifndef HOST_KERNEL_IO_H
#define HOST_KERNEL_IO_H

#include "datatypes.h"
#include <cstdio>

call_t read_call(FILE *fp);
void print_return(FILE *fp, const return_t &data);

#endif // HOST_KERNEL_IO_H
