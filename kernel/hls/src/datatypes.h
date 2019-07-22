#ifndef DATATYPES_H
#define DATATYPES_H

#include <vector>
#include <cstdint>
#include <ap_int.h>
#include <ap_fixed.h>

//typedef float qspan_t;
typedef ap_ufixed<16, 0, AP_RND, AP_SAT> qspan_t;
typedef int64_t anchor_idx_t;
typedef uint32_t tag_t;
typedef int32_t loc_t;
typedef int32_t loc_dist_t;
typedef int32_t score_t;
typedef uint32_t parent_t;
typedef int32_t width_t;

struct anchor_t {
    tag_t   tag;
    loc_t   x;
    width_t w;
    loc_t   y;
};

typedef ap_uint<32> anchor_idx_dt;
typedef ap_uint<7>  tag_dt;
typedef ap_uint<16> loc_dt;
typedef ap_int<17> loc_dist_dt;
typedef ap_uint<16> score_dt;
typedef ap_uint<32> parent_dt;
typedef ap_uint<16> width_dt;

typedef ap_uint<64> anchor_dt;
typedef ap_uint<64> return_dt;

#define ANCHOR_NULL (anchor_idx_t)(-1)
#define TILE_NUM_NULL (0xFFFF)
#define DRAM_NUM 4

struct call_t {
    anchor_idx_t n;
    qspan_t avg_qspan;
    int max_dist_x, max_dist_y, bw;
    std::vector<anchor_t> anchors;
};

struct return_t {
    anchor_idx_t n;
    std::vector<score_t> scores;
    std::vector<parent_t> parents;
};

#endif // DATATYPES_H
