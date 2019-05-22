#include <vector>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include "host_kernel.h"
#include "common.h"

static inline int32_t ilog2_32(uint32_t v)
{
    if (v < 2) return 0;
    else if (v < 4) return 1;
    else if (v < 8) return 2;
    else if (v < 16) return 3;
    else if (v < 32) return 4;
    else if (v < 64) return 5;
    else if (v < 128) return 6;
    else if (v < 256) return 7;
    else return 8;
}

void host_chain_kernel(std::vector<call_t> &args, std::vector<return_t> &rets)
{
#pragma omp parallel for schedule(guided)
    for (size_t i = 0; i < args.size(); i++) {
        args[i].anchors.resize(args[i].n + BACK_SEARCH_COUNT);
        args[i].xs = (loc_t *)aligned_alloc(64, sizeof(loc_t) * (args[i].n + BACK_SEARCH_COUNT) + 64);
        args[i].ys = (loc_t *)aligned_alloc(64, sizeof(loc_t) * (args[i].n + BACK_SEARCH_COUNT) + 64);
        args[i].ws = (score_t *)aligned_alloc(64, sizeof(score_t) * (args[i].n + BACK_SEARCH_COUNT) + 64);
        args[i].tags = (tag_t *)aligned_alloc(64, sizeof(tag_t) * (args[i].n + BACK_SEARCH_COUNT) + 64);
        rets[i].n = args[i].n;
        rets[i].scores.resize(rets[i].n + BACK_SEARCH_COUNT);
        rets[i].parents.resize(rets[i].n + BACK_SEARCH_COUNT);
        for (int32_t j = 0; j < args[i].n; j++) {
            args[i].xs[j] = args[i].anchors[j].x;
            args[i].ys[j] = args[i].anchors[j].y;
            args[i].ws[j] = args[i].anchors[j].w;
            args[i].tags[j] = args[i].anchors[j].tag;
        }
    }

    struct timespec start, end;
    clock_gettime(CLOCK_BOOTTIME, &start);

#pragma omp parallel for schedule(guided)
    for (size_t batch = 0; batch < args.size(); batch++) {
        auto &arg = args[batch];
        auto &ret = rets[batch];
        score_t  *f = ret.scores.data();
        parent_t *p = ret.parents.data();

        __declspec(align(64)) score_t max_tracker[BACK_SEARCH_COUNT] = {0};
        __declspec(align(64)) loc_t   j_tracker[BACK_SEARCH_COUNT] = {0};
        __declspec(align(64)) loc_t   x_tracker[BACK_SEARCH_COUNT] = {0};
        __declspec(align(64)) loc_t   y_tracker[BACK_SEARCH_COUNT] = {0};
        __declspec(align(64)) score_t w_tracker[BACK_SEARCH_COUNT] = {0};
        __declspec(align(64)) tag_t   tag_tracker[BACK_SEARCH_COUNT] = {0};

        auto curr_x = arg.xs[0];
        auto curr_y = arg.ys[0];
        auto curr_w = arg.ws[0];
        auto curr_tag = arg.tags[0];
        score_t max_f = max_tracker[0];
        loc_t   max_j = j_tracker[0];

        __assume_aligned(arg.xs, 64);
#pragma simd
        for (int32_t i = 0; i < BACK_SEARCH_COUNT; i++) { x_tracker[i] = arg.xs[i + 1]; }
        __assume_aligned(arg.ys, 64);
#pragma simd
        for (int32_t i = 0; i < BACK_SEARCH_COUNT; i++) { y_tracker[i] = arg.ys[i + 1]; }
        __assume_aligned(arg.ws, 64);
#pragma simd
        for (int32_t i = 0; i < BACK_SEARCH_COUNT; i++) { w_tracker[i] = arg.ws[i + 1]; }
        __assume_aligned(arg.tags, 64);
#pragma simd
        for (int32_t i = 0; i < BACK_SEARCH_COUNT; i++) { tag_tracker[i] = arg.tags[i + 1]; }

        for (int32_t i = 0; i < arg.n; i++) {
            if (curr_w >= max_f) { max_f = curr_w; max_j = -1; }
            f[i] = max_f; p[i] = max_j;

            int32_t end = 0;
            for (; end < BACK_SEARCH_COUNT - 1; end++) {
                if (curr_tag != tag_tracker[end]) break;
            }
            end += 8; if (end > BACK_SEARCH_COUNT) end = BACK_SEARCH_COUNT;
            __assume(end % 8 == 0);

            // "forward" calculate
#pragma simd
            for (int32_t j = 0; j < end; j++) {
                auto next_x = x_tracker[j];
                auto next_y = y_tracker[j];
                auto next_w = w_tracker[j];

                if (curr_tag != tag_tracker[j]) continue;
                loc_dist_t dist_x = next_x - curr_x;
                if ((dist_x == 0 || dist_x > arg.max_dist_x)) continue;
                loc_dist_t dist_y = next_y - curr_y;
                if ((dist_y > arg.max_dist_y || dist_y <= 0)) continue;
                loc_dist_t dd = dist_x > dist_y ? dist_x - dist_y : dist_y - dist_x;
                if (dd > arg.bw) { continue; }
                loc_dist_t min_d = dist_y < dist_x ? dist_y : dist_x;
                score_t sc = min_d > next_w ? next_w : min_d;
                int32_t log_dd = dd ? ilog2_32((uint32_t)dd) : 0;
                sc -= (score_t)(dd * 0.01 * arg.avg_qspan) + (log_dd >> 1);
                sc += f[i];
                if (sc >= max_tracker[j]) { max_tracker[j] = sc; j_tracker[j] = i; }
            }

            curr_x = x_tracker[0];
            curr_y = y_tracker[0];
            curr_w = w_tracker[0];
            curr_tag = tag_tracker[0];
            max_f = max_tracker[0];
            max_j = j_tracker[0];
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { max_tracker[j] = max_tracker[j+1]; }
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { j_tracker[j] = j_tracker[j+1]; }
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { x_tracker[j] = x_tracker[j+1]; }
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { y_tracker[j] = y_tracker[j+1]; }
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { w_tracker[j] = w_tracker[j+1]; }
#pragma simd
            for (int32_t j = 0; j < BACK_SEARCH_COUNT - 1; j++) { tag_tracker[j] = tag_tracker[j+1]; }
            max_tracker[BACK_SEARCH_COUNT - 1] = 0;
            x_tracker[BACK_SEARCH_COUNT - 1] = arg.xs[i + BACK_SEARCH_COUNT + 1];
            y_tracker[BACK_SEARCH_COUNT - 1] = arg.ys[i + BACK_SEARCH_COUNT + 1];
            w_tracker[BACK_SEARCH_COUNT - 1] = arg.ws[i + BACK_SEARCH_COUNT + 1];
            tag_tracker[BACK_SEARCH_COUNT - 1] = arg.tags[i + BACK_SEARCH_COUNT + 1];
        }
    }

    clock_gettime(CLOCK_BOOTTIME, &end);
    fprintf(stderr, " ***** kernel took %f seconds to finish\n",
            ( end.tv_sec - start.tv_sec ) + ( end.tv_nsec - start.tv_nsec ) / 1E9);
}
