#include <cassert>
#include <ap_int.h>
#include <vector>
#include <ap_fixed.h>
#define OPTIMIZE_DSP

typedef ap_uint<32> anchor_idx_dt;
typedef ap_uint<7> tag_dt;
typedef ap_uint<16> loc_dt;
typedef ap_int<17> loc_dist_dt;
typedef ap_int<17> score_dt;
typedef ap_int<32> parent_dt;
typedef ap_uint<16> width_dt;

const int BITS_PER_DATA = 64;

typedef ap_uint<BITS_PER_DATA> anchor_dt_io;
typedef ap_uint<BITS_PER_DATA> control_dt_io;
typedef ap_uint<BITS_PER_DATA> return_dt_io;
typedef ap_uint<512> block_io;

#ifdef OPTIMIZE_DSP
typedef ap_ufixed<16, 0, AP_RND, AP_SAT> qspan_dt;
#define QSPAN_WIDTH 16
#else
typedef float qspan_dt;
#define QSPAN_WIDTH 32
#endif

struct anchor_dt {
    tag_dt   tag;
    loc_dt   x;
    score_dt w;
    loc_dt   y;
};

struct control_dt {
    bool is_new_read;
    qspan_dt avg_qspan;
    ap_uint<16> tile_num;
};

struct return_dt {
    score_dt score;
    parent_dt parent;
};

template<class T>
T HLS_REG(T in){
#pragma HLS pipeline
#pragma HLS inline off
#pragma HLS interface port=return register
    return in;
}

anchor_dt io_to_anchor(anchor_dt_io a) {
#pragma HLS inline
    anchor_dt ret;
#pragma HLS data_pack variable=ret
    ret.y = a(15, 0);
    ret.w = a(31, 16);
    ret.x = a(47, 32);
    ret.tag = a(54, 48);
    return ret;
}

control_dt io_to_control(control_dt_io c) {
#pragma HLS inline
    control_dt ret;
#pragma HLS data_pack variable=ret
    ret.is_new_read = c[48];
    ap_uint<QSPAN_WIDTH> avg = c(16+QSPAN_WIDTH-1, 16);
    ret.avg_qspan = *((qspan_dt *)(&avg));
    ret.tile_num = c(15, 0);
    return ret;
}

return_dt_io return_to_io(return_dt r) {
    return_dt_io temp = (return_dt_io)r.score;
    temp = (temp << 32) | ((return_dt_io)(r.parent) & 0xFFFFFFFF);
    return temp;
}

static const int BACK_SEARCH_COUNT = 65;
static const score_dt NEG_INF_SCORE = 0x10000;

const int TILE_SIZE = 2048;
const int TILE_SIZE_ACTUAL = TILE_SIZE + BACK_SEARCH_COUNT; // 1089

const int BITS_PER_BLOCK = 512;
const int PE_NUM = BITS_PER_BLOCK / BITS_PER_DATA; // 8
const int DATA_BLOCK_PER_BATCH = TILE_SIZE_ACTUAL + 1;
const int RETURN_BLOCK_PER_BATCH = TILE_SIZE + 1;

/*
 * DEPRECATE!
 * this is a bad way to do log.
 * HLS will generate fully combinational logic
 * don't use this coding style.
 */
score_dt device_ilog2_do_not_use(score_dt v)
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

void chain_dp_score(anchor_dt *active, anchor_dt curr, score_dt *sc,
        qspan_dt avg_qspan, int max_dist_x, int max_dist_y, int bw)
{
#pragma HLS expression_balance

    score_dt sc_temp[BACK_SEARCH_COUNT];
#pragma HLS array_partition variable=sc_temp complete

    for (int j = 0; j < BACK_SEARCH_COUNT; j++) {
#pragma HLS unroll
        score_dt dist_x = active[j].x - curr.x;
        score_dt dist_y = active[j].y - curr.y;

        score_dt dd = dist_x > dist_y ? dist_x - dist_y : dist_y - dist_x;
        score_dt min_d = dist_y < dist_x ? dist_y : dist_x;

        //compute the log
        score_dt log_dd_temp = 8;
        log_dd_temp = dd < 256 ?(score_dt)7 : (score_dt)8;
        log_dd_temp = dd < 128 ?(score_dt)6 : log_dd_temp;
        log_dd_temp = dd < 64 ? (score_dt)5 : log_dd_temp;
        log_dd_temp = dd < 32 ? (score_dt)4 : log_dd_temp;
        log_dd_temp = dd < 16 ? (score_dt)3 : log_dd_temp;
        log_dd_temp = dd < 8 ? (score_dt)2 : log_dd_temp;
        log_dd_temp = dd < 4 ? (score_dt)1 : log_dd_temp;
        log_dd_temp = dd < 2 ? (score_dt)0 : log_dd_temp;
        score_dt log_dd = log_dd_temp;

        // corresponding to alpha(j, i)
        sc_temp[j] = min_d > active[j].w ? active[j].w : min_d;
        sc[j] = sc_temp[j]- ( (score_dt)(dd * avg_qspan) + (log_dd >> 1) );

        if( (dist_x == 0 || dist_x > max_dist_x ) ||
                (dist_y > max_dist_y || dist_y <= 0) ||
                (dd > bw) || (curr.tag != active[j].tag) ){
            sc[j] = NEG_INF_SCORE;
        }
    }
}

/*
    Tiled version of chain_kernel
    f, p: buffer of batch_size
    a: buffer of batch_size + BACK_SEARCH_COUNT. Note that a_buffer
        must contain the current batch_size anchors, and also
        the BACK_SEARCH_COUNT anchors that follow.  In case of reaching
        boundary, don't need to care the ones that go beyond boundary.
    max_tracker and j_tracker should be global to record state
    batch_num: indicates which-th batch currently is
    batch_size: how many anchors per batch

    Note that many parameters are fixed here, e.g.is_cdna, max_dist, etc.
    Splice mode not supported. Only tested on E. coli 40X raw data.
*/
template <int pe_id>
void device_chain_tiled(bool is_new_read, return_dt ret[TILE_SIZE], int n, const anchor_dt a[TILE_SIZE_ACTUAL],
        qspan_dt avg_qspan,
        anchor_idx_dt batch_num, int batch_size, int max_dist_x, int max_dist_y, int bw)
{
#pragma HLS inline off
#ifdef OPTIMIZE_DSP
    // in vary rare case this will results in minor minor difference.
    //avg_qspan = avg_qspan * 0.01;
#endif

    static score_dt max_tracker[BACK_SEARCH_COUNT];
    static parent_dt j_tracker[BACK_SEARCH_COUNT];
#pragma HLS array_partition variable=max_tracker dim=1 complete
#pragma HLS array_partition variable=j_tracker dim=1 complete

    anchor_dt active[BACK_SEARCH_COUNT];
    score_dt sc[BACK_SEARCH_COUNT];
#pragma HLS data_pack variable=active
#pragma HLS array_partition variable=active complete
#pragma HLS array_partition variable=sc complete

    //initial setup
    for (int i = 0; i < BACK_SEARCH_COUNT; i++){
#pragma HLS pipeline II=1
        // active FIFO
        for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++) {
#pragma HLS unroll
            active[j] = active[j + 1];
        }
        anchor_dt temp = a[i];
        anchor_dt temp_q = HLS_REG(temp);
        anchor_dt temp_q2 = HLS_REG(temp_q);
        active[BACK_SEARCH_COUNT-1] = temp_q2;

    }

    for (int i = 0; i < BACK_SEARCH_COUNT; i++){
#pragma HLS pipeline II=1
        // tracker FIFO
        parent_dt j_head = j_tracker[0];
        score_dt max_head = max_tracker[0];
        for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++) {
#pragma HLS unroll
            j_tracker[j] = j_tracker[j + 1];
            max_tracker[j] = max_tracker[j + 1];
        }
        anchor_dt temp = a[i];
        anchor_dt temp_q = HLS_REG(temp);
        anchor_dt temp_q2 = HLS_REG(temp_q);
        max_tracker[BACK_SEARCH_COUNT-1] = is_new_read ? (score_dt)temp_q2.w : max_head ;
        j_tracker[BACK_SEARCH_COUNT-1] = is_new_read ? (parent_dt)(-1) : j_head;
    }

    for (int i = BACK_SEARCH_COUNT, curr_idx = 0; curr_idx < n; i++, curr_idx++) {
#pragma HLS pipeline II=1
        /* read in new ref anchor */
        anchor_dt curr = active[0];
#pragma HLS data_pack variable=curr

        /* parallel pipeline calculating dp score between curr and elements in active array */
        chain_dp_score(active, curr, sc, avg_qspan, max_dist_x, max_dist_y, bw);

        score_dt f_curr = max_tracker[0];
        parent_dt p_curr = j_tracker[0];

        // by pre-assigning max_tracker to corresponding length value,
        // this can be removed with neglectible difference
#ifndef OPTIMIZE_FREQUENCY
        if (curr.w >= max_tracker[0]) { // this part can be moved away
            f_curr = curr.w;
            p_curr = -1;
        }
#endif

        // update trackers
        for (int j = 0; j < BACK_SEARCH_COUNT-1; j++) {
#pragma HLS unroll
            if (sc[j+1] + f_curr >= max_tracker[j+1]) {
                max_tracker[j] = sc[j+1] + f_curr;
                j_tracker[j] = curr_idx + batch_num * batch_size;
            }
            else {
                max_tracker[j] = max_tracker[j+1];
                j_tracker[j] = j_tracker[j+1];
            }
        }
        max_tracker[BACK_SEARCH_COUNT-1] = 0;
        j_tracker[BACK_SEARCH_COUNT-1] = -1;

        // active FIFO
        for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++) {
#pragma HLS unroll
            active[j] = active[j + 1];
        }
        /* read in new query anchor, put into active array*/
        anchor_dt temp = a[i];
        anchor_dt temp_q = HLS_REG(temp);
        anchor_dt temp_q2 = HLS_REG(temp_q);
        active[BACK_SEARCH_COUNT-1] = temp_q2;

        /* update trackers and f_curr, f_curr dependency is the bottleneck */
        score_dt f_curr_q = HLS_REG(f_curr);
        score_dt f_curr_q2 = HLS_REG(f_curr_q);
        score_dt f_curr_q3 = HLS_REG(f_curr_q2);
        score_dt f_curr_q4 = HLS_REG(f_curr_q3);
        ret[curr_idx].score = f_curr_q4;

        parent_dt p_curr_q = HLS_REG(p_curr);
        parent_dt p_curr_q2 = HLS_REG(p_curr_q);
        parent_dt p_curr_q3 = HLS_REG(p_curr_q2);
        parent_dt p_curr_q4 = HLS_REG(p_curr_q3);
        ret[curr_idx].parent = p_curr_q4;
    }
}

/*---------------- scheduler ---------------------------*/

void block_to_controls(block_io block, control_dt controls[PE_NUM])
{
    for (int i = 0; i < PE_NUM; i++) {
#pragma HLS unroll
        control_dt_io temp = (block >> (i * BITS_PER_DATA));
        controls[i] = io_to_control(temp(63, 0));
    }
}

void block_to_anchors(
        block_io block[TILE_SIZE_ACTUAL],
        anchor_dt anchors[PE_NUM][TILE_SIZE_ACTUAL])
{
    for (int t = 0; t < TILE_SIZE_ACTUAL; t++) {
#pragma HLS pipeline II=1
        for (int i = 0; i < PE_NUM; i++) {
#pragma HLS unroll
            block_io temp = (block[t] >> (i * BITS_PER_DATA));
            anchors[i][t] = io_to_anchor(temp(63, 0));
        }
    }
}

/*--------------------------------------------------------------*/

// load
void load_anchors(
        bool enable,
        const block_io *anchors_block, // from DRAM

        control_dt controls_pe[PE_NUM], // local buffer
        anchor_dt anchors_pe[PE_NUM][TILE_SIZE_ACTUAL],
        block_io *control_block)
{

if(enable) {
    // following anchor blocks, i starts from 1
    for (int i = 0; i < DATA_BLOCK_PER_BATCH; i++) {
#pragma HLS pipeline II=1
        block_io curr_block = anchors_block[i];
        block_io curr_block_q = HLS_REG(curr_block); //explicit register
        block_io curr_block_q2 = HLS_REG(curr_block_q); //explicit register
        block_io curr_block_q3 = HLS_REG(curr_block_q2); //explicit register
        block_io curr_block_q4 = HLS_REG(curr_block_q3); //explicit register
        if ( i == 0) {
            // read in first block (control block)
            block_to_controls(curr_block_q4, controls_pe);
            *control_block = curr_block_q4;
        }
        else {
            for (int pe = 0; pe < PE_NUM; pe++) {
#pragma HLS unroll
                block_io temp = (curr_block >> (pe * BITS_PER_DATA));
                anchor_dt_io temp_anchor = temp(63, 0);
                anchor_dt_io temp_q = HLS_REG(temp_anchor);
                anchor_dt_io temp_q2 = HLS_REG(temp_q);
                anchor_dt_io temp_q3 = HLS_REG(temp_q2);
                anchor_dt_io temp_q4 = HLS_REG(temp_q3);
                anchor_dt_io temp_q5 = HLS_REG(temp_q4);
                anchors_pe[pe][i-1] = io_to_anchor(temp_q5);
            }
        }
    }
} // end enable

}

// compute
void pe_array(
        control_dt controls[PE_NUM],
        anchor_dt anchors[PE_NUM][TILE_SIZE_ACTUAL],
        return_dt returns[PE_NUM][TILE_SIZE],
        int max_dist_x, int max_dist_y, int bw)
{
    device_chain_tiled<0>(controls[0].is_new_read, returns[0],
        TILE_SIZE, anchors[0], controls[0].avg_qspan,
        controls[0].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<1>(controls[1].is_new_read, returns[1],
        TILE_SIZE, anchors[1], controls[1].avg_qspan,
        controls[1].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<2>(controls[2].is_new_read, returns[2],
        TILE_SIZE, anchors[2], controls[2].avg_qspan,
        controls[2].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<3>(controls[3].is_new_read, returns[3],
        TILE_SIZE, anchors[3], controls[3].avg_qspan,
        controls[3].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<4>(controls[4].is_new_read, returns[4],
        TILE_SIZE, anchors[4], controls[4].avg_qspan,
        controls[4].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<5>(controls[5].is_new_read, returns[5],
        TILE_SIZE, anchors[5], controls[5].avg_qspan,
        controls[5].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<6>(controls[6].is_new_read, returns[6],
        TILE_SIZE, anchors[6], controls[6].avg_qspan,
        controls[6].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
    device_chain_tiled<7>(controls[7].is_new_read, returns[7],
        TILE_SIZE, anchors[7], controls[7].avg_qspan,
        controls[7].tile_num, TILE_SIZE,
        max_dist_x, max_dist_y, bw);
}

// store
void store_results(
        bool enable,
        // const block_io returns_local[RETURN_BLOCK_PER_BATCH],
        block_io control_block,
        return_dt returns_pe[PE_NUM][TILE_SIZE],
        block_io *returns_block)
{
    if(enable) {
        block_io temp;

        // store result blocks
        for (int i = 0; i < RETURN_BLOCK_PER_BATCH; i++) {
    #pragma HLS pipeline II=1
    #define CONVERT(k) temp((k+1) * BITS_PER_DATA - 1, k * BITS_PER_DATA) \
            = return_to_io(HLS_REG(HLS_REG(returns_pe[k][i-1])));
            block_io temp_q;
            if (i != 0) {
                CONVERT(0); CONVERT(1); CONVERT(2); CONVERT(3);
                CONVERT(4); CONVERT(5); CONVERT(6); CONVERT(7);
                temp_q = HLS_REG(temp);
            }
            else {
                temp_q = control_block;
            }
            block_io temp_q2 = HLS_REG(temp_q);
            block_io temp_q3 = HLS_REG(temp_q2);
            block_io temp_q4 = HLS_REG(temp_q3);
            block_io temp_q5 = HLS_REG(temp_q4);
            block_io temp_q6 = HLS_REG(temp_q5);
            returns_block[i] = temp_q6; // +1 because of control block
        }
    } // end enable
}

void load_compute_store(
        const block_io *anchors,
        block_io *returns,
        long long n,
        int max_dist_x, int max_dist_y, int bw)
{

    control_dt controls_pe[3][PE_NUM];
#pragma HLS array_partition variable=controls_pe dim=1 complete
#pragma HLS array_partition variable=controls_pe dim=2 complete
    anchor_dt anchors_pe[3][PE_NUM][TILE_SIZE_ACTUAL];
#pragma HLS array_partition variable=anchors_pe dim=1 complete
#pragma HLS array_partition variable=anchors_pe dim=2 complete
#ifdef OPTIMIZE_FREQUENCY
    #pragma HLS array_partition variable=anchors_pe dim=3 cyclic factor=2
#endif
    return_dt returns_pe[3][PE_NUM][TILE_SIZE];
#pragma HLS array_partition variable=returns_pe complete dim=1
#pragma HLS array_partition variable=returns_pe complete dim=2
    block_io control_blocks[3];
#pragma HLS array_partition variable=control_blocks complete dim=1

    int batch_count = n / PE_NUM / DATA_BLOCK_PER_BATCH;

    for (int batch = 0; batch < batch_count + 2; batch++) {
#pragma HLS loop_tripcount min=1000 max=10000
        switch (batch % 3) {
            case 0:
                load_anchors( (batch < batch_count),
                        anchors + batch * DATA_BLOCK_PER_BATCH,
                        controls_pe[0], anchors_pe[0],
                        &control_blocks[0]);

                pe_array(controls_pe[2], anchors_pe[2], returns_pe[2],
                        max_dist_x, max_dist_y, bw);

                store_results( (batch-2 >= 0),
                        control_blocks[1], returns_pe[1],
                        returns + (batch - 2) * RETURN_BLOCK_PER_BATCH);
                break;

            case 1:
                load_anchors( (batch < batch_count),
                        anchors + batch * DATA_BLOCK_PER_BATCH,
                        controls_pe[1], anchors_pe[1],
                        &control_blocks[1]);

                pe_array(controls_pe[0], anchors_pe[0], returns_pe[0],
                        max_dist_x, max_dist_y, bw);

                store_results( (batch-2 >= 0),
                        control_blocks[2], returns_pe[2],
                        returns + (batch - 2) * RETURN_BLOCK_PER_BATCH);
                break;

            case 2:
                load_anchors( (batch < batch_count),
                        anchors + batch * DATA_BLOCK_PER_BATCH,
                        controls_pe[2], anchors_pe[2],
                        &control_blocks[2]);

                pe_array(controls_pe[1], anchors_pe[1], returns_pe[1],
                        max_dist_x, max_dist_y, bw);

                store_results( (batch-2 >= 0),
                        control_blocks[0], returns_pe[0],
                        returns + (batch - 2) * RETURN_BLOCK_PER_BATCH);
                break;
        }
    }
}


extern "C" {

void device_chain_kernel(
        block_io *anchors_0,
        block_io *returns_0,
        long long n,
        int max_dist_x, int max_dist_y, int bw)
{
#pragma HLS interface m_axi port=anchors_0 offset=slave bundle=gmem_0
#pragma HLS interface m_axi port=returns_0 offset=slave bundle=gmem_1

#pragma HLS interface s_axilite port=anchors_0 bundle=control
#pragma HLS interface s_axilite port=returns_0 bundle=control
#pragma HLS interface s_axilite port=n bundle=control
#pragma HLS interface s_axilite port=max_dist_x bundle=control
#pragma HLS interface s_axilite port=max_dist_y bundle=control
#pragma HLS interface s_axilite port=bw bundle=control
#pragma HLS interface s_axilite port=return bundle=control

    load_compute_store(anchors_0, returns_0, n, max_dist_x, max_dist_y, bw);
}

} // extern "C"
