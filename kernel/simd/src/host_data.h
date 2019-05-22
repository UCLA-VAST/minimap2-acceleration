#ifndef HOST_INPUT_H
#define HOST_INPUT_H

#include <vector>
#include <cstdint>

typedef int64_t anchor_idx_t;
typedef uint32_t tag_t;
typedef int32_t loc_t;
typedef int32_t loc_dist_t;
typedef int32_t score_t;
typedef int32_t parent_t;

#define ANCHOR_NULL (anchor_idx_t)(-1)

struct anchor_t {
    tag_t   tag;
    loc_t   x;
    score_t w;
    loc_t   y;
};

struct call_t {
    anchor_idx_t n;
    float avg_qspan;
    int max_dist_x, max_dist_y, bw;
    std::vector<anchor_t> anchors;
    tag_t *tags;
    loc_t *xs;
    score_t *ws;
    loc_t *ys;
};

struct return_t {
    anchor_idx_t n;
    std::vector<score_t> scores;
    std::vector<parent_t> parents;
};

#endif // HOST_INPUT_H
