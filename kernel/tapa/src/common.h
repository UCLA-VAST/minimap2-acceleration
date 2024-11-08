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

typedef ap_uint<sizeof(anchor_dt) * 8 * PE_NUM> anchor_dt_bits;
typedef ap_uint<sizeof(control_dt) * 8 * PE_NUM> control_dt_bits;
typedef ap_uint<sizeof(return_dt) * 8 * PE_NUM> return_dt_bits;

void DeviceChainKernel(tapa::mmap<anchor_dt_bits> anchors,
                       tapa::mmap<control_dt_bits> controls,
                       tapa::mmap<return_dt_bits> returns, int n,
                       int max_dist_x, int max_dist_y, int bw);
