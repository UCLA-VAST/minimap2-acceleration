#pragma once

#include "datatypes.h"
#include <tapa.h>

#define BACK_SEARCH_COUNT (65)
#define BITS_PER_BLOCK (512)
#define BITS_PER_DATA (64)
#define PE_NUM (BITS_PER_BLOCK / BITS_PER_DATA)
#define BATCH_NUM_NULL (0xFFFF)
#define BATCH_SIZE_OUTPUT (2048)
#define BATCH_SIZE_INPUT (BATCH_SIZE_OUTPUT + BACK_SEARCH_COUNT)

void DeviceChainKernel(tapa::mmap<tapa::vec_t<anchor_dt, PE_NUM>> anchors,
                       tapa::mmap<tapa::vec_t<control_dt, PE_NUM>> controls,
                       tapa::mmap<tapa::vec_t<return_dt, PE_NUM>> returns,
                       int n, int max_dist_x, int max_dist_y, int bw);
