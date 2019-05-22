#include <vector>
#include "host_kernel.h"
#include "common.h"

static const char LogTable256[256] = {
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)};

static inline int32_t ilog2_32(uint32_t v)
{
    register uint32_t t, tt;
    if ((tt = v >> 16))
        return (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
    return (t = v >> 8) ? 8 + LogTable256[t] : LogTable256[v];
}

void host_chain_kernel(const call_t &arg, return_t &ret)
{
    ret.n = arg.n;
    ret.scores.resize(ret.n);
    ret.parents.resize(ret.n);
    score_t  *f = ret.scores.data();
    parent_t *p = ret.parents.data();

    std::vector<score_t> max_tracker(BACK_SEARCH_COUNT, 0);
    std::vector<loc_t>   j_tracker(BACK_SEARCH_COUNT, 0);

    for (anchor_idx_t i = 0; i < arg.n; i++) {
        anchor_t curr = arg.anchors[i];

        score_t max_f = max_tracker[i % BACK_SEARCH_COUNT];
        loc_t   max_j = j_tracker[i % BACK_SEARCH_COUNT];

        if (curr.w >= max_f) {
            max_f = curr.w;
            max_j = -1;
        }

        f[i] = max_f;
        // [add] p[i] is the current best predecessor of i;
        p[i] = max_j;
        // f[] is the score ending at i, not always the peak

        // "forward" calculate
        for (anchor_idx_t j = i + 1, row = 1;
                j < arg.n && row < BACK_SEARCH_COUNT;
                j++, row++) {
            anchor_t next = arg.anchors[j];

            loc_dist_t dist_x = next.x - curr.x;
            loc_dist_t dist_y = next.y - curr.y;

            loc_dist_t dd = dist_x > dist_y ? dist_x - dist_y : dist_y - dist_x;
            loc_dist_t min_d = dist_y < dist_x ? dist_y : dist_x;

            // [add] corresponding to alpha(j, i)
            score_t sc = min_d > next.w ? next.w : min_d;
            int32_t log_dd = dd ? ilog2_32((uint32_t)dd) : 0;

            // [add] corresponding to beta(j, i)
            sc -= (score_t)(dd * 0.01 * arg.avg_qspan) + (log_dd >> 1);
            if ( (dist_x == 0 || dist_x > arg.max_dist_x) ||
                    (dist_y > arg.max_dist_y || dist_y <= 0) ||
                    (dd > arg.bw) || (curr.tag != next.tag) ){
                sc = NEG_INF_SCORE;
            }

            sc += f[i]; // [note] change to f[i]

            if (sc >= max_tracker[j % BACK_SEARCH_COUNT])
            {
                max_tracker[j % BACK_SEARCH_COUNT] = sc;
                j_tracker[j % BACK_SEARCH_COUNT] = i;
            }
        }
        max_tracker[i % BACK_SEARCH_COUNT] = 0;
        j_tracker[i % BACK_SEARCH_COUNT] = 0;
    }
}
