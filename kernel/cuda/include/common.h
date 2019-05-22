#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include "datatypes.h"

#define BACK_SEARCH_COUNT (65)
#define BACK_SEARCH_COUNT_GPU (64)
#define NEG_INF_SCORE (-((score_t)0x3FFFFFFF))
#define NEG_INF_SCORE_GPU (-((score_dt)0x3FFFFFFF))
#define STREAM_NUM (1)
#define BLOCK_NUM (1792)
#define THREAD_FACTOR (1)
#define PE_NUM (STREAM_NUM * BLOCK_NUM * THREAD_FACTOR)
#define TILE_SIZE (1024)
#define TILE_SIZE_ACTUAL (TILE_SIZE + BACK_SEARCH_COUNT)

#endif // COMMON_H
