#ifndef DATATYPES_H
#define DATATYPES_H

#include <vector>
#include <cstdint>

typedef int64_t anchor_idx_t;
typedef uint32_t tag_t;
typedef int32_t loc_t;
typedef int32_t loc_dist_t;
typedef int32_t score_t;
typedef int32_t parent_t;
typedef int32_t width_t;

struct anchor_t {
    tag_t   tag;
    loc_t   x;
    width_t w;
    loc_t   y;
};

typedef uint32_t anchor_idx_dt;
typedef uint16_t tag_dt;
typedef uint16_t loc_dt;
typedef int32_t  loc_dist_dt;
typedef int32_t  score_dt;
typedef int32_t  parent_dt;
typedef uint16_t width_dt;

#pragma pack (2)
struct anchor_dt {
    loc_dt   x;   // 16 bits alignment
    loc_dt   y;   // 16 bits alignment
    width_dt w;   // 16 bits alignment
    tag_dt   tag; // 16 bits alignment
};
static_assert(sizeof(anchor_dt) == 8, // bytes
        "Failed to pack anchor_dt");

#pragma pack (2)
struct control_dt {
    float avg_qspan;
    uint16_t tile_num;
    bool is_new_read;
};
static_assert(sizeof(control_dt) == 8, // bytes
        "Failed to pack control_dt");

#pragma pack (2)
struct return_dt {
    score_dt  score;
    parent_dt parent;
};
static_assert(sizeof(control_dt) == 8, // bytes
        "Failed to pack return_dt");

#define ANCHOR_NULL (anchor_idx_t)(-1)
#define TILE_NUM_NULL (0xFFFF)


struct call_t {
    anchor_idx_t n;
    float avg_qspan;
    int max_dist_x, max_dist_y, bw;
    std::vector<anchor_t> anchors;
};

struct return_t {
    anchor_idx_t n;
    std::vector<score_t> scores;
    std::vector<parent_t> parents;
};

#endif // DATATYPES_H
