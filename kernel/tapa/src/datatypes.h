#pragma once

#include <ap_fixed.h>
#include <ap_int.h>
#include <cstdint>
#include <vector>

// Native data types
typedef ap_ufixed<16, 0, AP_RND, AP_SAT> qspan_t;
typedef int64_t anchor_idx_t;
typedef uint32_t tag_t;
typedef int32_t loc_t;
typedef int32_t loc_dist_t;
typedef int32_t score_t;
typedef uint32_t parent_t;
typedef int32_t width_t;

struct anchor_t {
  tag_t tag;
  loc_t x;
  width_t w;
  loc_t y;
};

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

#define ANCHOR_NULL (anchor_idx_t)(-1)

// Data types for hardware
typedef ap_ufixed<16, 0, AP_RND, AP_SAT> qspan_dt;
typedef ap_uint<32> anchor_idx_dt;
typedef ap_uint<7> tag_dt;
typedef ap_uint<16> loc_dt;
typedef ap_int<17> loc_dist_dt;
typedef ap_int<17> score_dt;
typedef ap_uint<32> parent_dt;
typedef ap_uint<16> width_dt;
typedef ap_uint<1> bool_dt;
typedef ap_uint<15> batch_num_dt;

// Only POD types can be packed
struct anchor_dt {
  int x : 16;
  int y : 16;
  unsigned int tag : 7;
  int padding_0 : 1;
  int w : 17;
  int padding_1 : 7;
};

struct control_dt {
  bool is_new_read : 1;
  unsigned int batch_num : 15;
  qspan_dt avg_qspan;
};

struct return_dt {
  int score : 17;
  int padding_0 : 15;
  unsigned int parent : 32;
};
