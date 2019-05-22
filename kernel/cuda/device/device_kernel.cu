#include "common.h"
#include "datatypes.h"

__device__
score_dt device_ilog2(score_dt v)
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

__device__
score_dt chain_dp_score(anchor_dt *active, anchor_dt curr,
        float avg_qspan, int max_dist_x, int max_dist_y, int bw, int id)
{
    anchor_dt act;
    *((short4*)&act) = ((short4*)active)[id];

    if (curr.tag != act.tag) return NEG_INF_SCORE_GPU;

    score_dt dist_x = act.x - curr.x;
    if (dist_x == 0 || dist_x > max_dist_x) return NEG_INF_SCORE_GPU;

    score_dt dist_y = act.y - curr.y;
    if (dist_y > max_dist_y || dist_y <= 0) return NEG_INF_SCORE_GPU;

    score_dt dd = dist_x > dist_y ? dist_x - dist_y : dist_y - dist_x;
    if (dd > bw) return NEG_INF_SCORE_GPU;

    score_dt min_d = dist_y < dist_x ? dist_y : dist_x;
    score_dt log_dd = device_ilog2(dd);

    score_dt sc = min_d > act.w ? act.w : min_d;
    sc -= (score_dt)(dd * (0.01 * avg_qspan)) + (log_dd >> 1);

    return sc;
}

//#define USE_LOCAL_BUFFER

__global__
void device_chain_tiled(
        return_dt *ret, int n, const anchor_dt *a,
        control_dt *cont, score_dt **max_tracker_g, parent_dt **j_tracker_g,
        int max_dist_x, int max_dist_y, int bw)
{
    int block = blockIdx.x;
    int id = threadIdx.x % BACK_SEARCH_COUNT_GPU;
    int sub = threadIdx.x / BACK_SEARCH_COUNT_GPU;
    int ofs = block * THREAD_FACTOR + sub;
    auto control = cont[ofs];

    __shared__ anchor_dt active[THREAD_FACTOR][BACK_SEARCH_COUNT_GPU];
    __shared__ score_dt max_tracker[THREAD_FACTOR][BACK_SEARCH_COUNT_GPU];
    __shared__ parent_dt j_tracker[THREAD_FACTOR][BACK_SEARCH_COUNT_GPU];

    ((short4*)active[sub])[id] = ((short4*)a)[ofs * TILE_SIZE_ACTUAL + id];
    if (control.is_new_read) {
        max_tracker[sub][id] = 0;
        j_tracker[sub][id] = -1;
    } else {
        max_tracker[sub][id] = max_tracker_g[ofs][id];
        j_tracker[sub][id] = j_tracker_g[ofs][id];
    }

#ifdef USE_LOCAL_BUFFER
    __shared__ anchor_dt a_local[THREAD_FACTOR][BACK_SEARCH_COUNT_GPU];
    __shared__ return_dt ret_local[THREAD_FACTOR][BACK_SEARCH_COUNT_GPU];
    ((short4*)a_local[sub])[id] = ((short4*)a)[ofs * TILE_SIZE_ACTUAL + BACK_SEARCH_COUNT_GPU + id];
#endif

    for (int i = BACK_SEARCH_COUNT_GPU, curr_idx = 0; curr_idx < n; i++, curr_idx++) {

        __syncthreads();
        anchor_dt curr;
        *((short4*)&curr) = ((short4*)active[sub])[i % BACK_SEARCH_COUNT_GPU];
        score_dt f_curr = max_tracker[sub][i % BACK_SEARCH_COUNT_GPU];
        parent_dt p_curr = j_tracker[sub][i % BACK_SEARCH_COUNT_GPU];
        if (curr.w >= f_curr) {
            f_curr = curr.w;
            p_curr = (parent_dt)-1;
        }

        /* read in new query anchor, put into active array*/
        __syncthreads();
        if (id == i % BACK_SEARCH_COUNT_GPU) {
#ifdef USE_LOCAL_BUFFER
            active[sub][id] = a_local[sub][id];
#else
            ((short4*)active[sub])[id] =
              ((short4*)a)[ofs * TILE_SIZE_ACTUAL + i];
#endif
            max_tracker[sub][id] = 0;
            j_tracker[sub][id] = -1;
        }

        __syncthreads();
        score_dt sc = chain_dp_score(active[sub], curr,
                control.avg_qspan, max_dist_x, max_dist_y, bw, id);

        __syncthreads();
        if (sc + f_curr >= max_tracker[sub][id]) {
            max_tracker[sub][id] = sc + f_curr;
            j_tracker[sub][id] = (parent_dt)curr_idx +
                (parent_dt)control.tile_num * n;
        }

        __syncthreads();
        if (id == curr_idx % BACK_SEARCH_COUNT_GPU) {
#ifdef USE_LOCAL_BUFFER
            ret_local[sub][id].score = f_curr;
            ret_local[sub][id].parent = p_curr;
#else
            return_dt tmp;
            tmp.score = f_curr;
            tmp.parent = p_curr;
            ((short4*)ret)[ofs * TILE_SIZE + curr_idx] = *((short4*)&tmp);
#endif
        }

#ifdef USE_LOCAL_BUFFER
        if ((i + 1) % BACK_SEARCH_COUNT_GPU == 0) {
            ((short4*)a_local[sub])[id] =
                ((short4*)a)[ofs * TILE_SIZE_ACTUAL + i + 1 + id];
            ((short4*)ret)[ofs * TILE_SIZE +
                    curr_idx - BACK_SEARCH_COUNT_GPU + 1 + id] =
                ((short4*)ret_local[sub])[id];
        }
#endif

    }

    __syncthreads();
    max_tracker_g[ofs][id] = max_tracker[sub][id];
    j_tracker_g[ofs][id] = j_tracker[sub][id];
}
