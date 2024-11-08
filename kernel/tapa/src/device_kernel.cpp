#include "common.h"
#include "datatypes.h"
#include <cstdio>

static const score_dt NEG_INF_SCORE = 0x10000;

void ChainDpScore(anchor_dt *active, anchor_dt curr, score_dt *sc,
                  qspan_dt avg_qspan, int max_dist_x, int max_dist_y, int bw) {
#pragma HLS expression_balance

  score_dt sc_temp[BACK_SEARCH_COUNT];
#pragma HLS array_partition variable = sc_temp complete

  [[tapa::unroll]] for (int j = 0; j < BACK_SEARCH_COUNT; j++) {
    score_dt dist_x = active[j].x - curr.x;
    score_dt dist_y = active[j].y - curr.y;

    score_dt dd = dist_x > dist_y ? dist_x - dist_y : dist_y - dist_x;
    score_dt min_d = dist_y < dist_x ? dist_y : dist_x;

    // compute the log
    score_dt log_dd_temp = 8;
    log_dd_temp = dd < 256 ? (score_dt)7 : (score_dt)8;
    log_dd_temp = dd < 128 ? (score_dt)6 : log_dd_temp;
    log_dd_temp = dd < 64 ? (score_dt)5 : log_dd_temp;
    log_dd_temp = dd < 32 ? (score_dt)4 : log_dd_temp;
    log_dd_temp = dd < 16 ? (score_dt)3 : log_dd_temp;
    log_dd_temp = dd < 8 ? (score_dt)2 : log_dd_temp;
    log_dd_temp = dd < 4 ? (score_dt)1 : log_dd_temp;
    log_dd_temp = dd < 2 ? (score_dt)0 : log_dd_temp;
    score_dt log_dd = log_dd_temp;

    // corresponding to alpha(j, i)
    sc_temp[j] = min_d > active[j].w ? (score_dt)active[j].w : min_d;
    sc[j] = sc_temp[j] - ((score_dt)(dd * avg_qspan) + (log_dd >> 1));

    if ((dist_x == 0 || dist_x > max_dist_x) ||
        (dist_y > max_dist_y || dist_y <= 0) || (dd > bw) ||
        (curr.tag != active[j].tag)) {
      sc[j] = NEG_INF_SCORE;
    }
  }
}

void DeviceChain(tapa::istream<anchor_dt> &in,
                 tapa::istream<control_dt> &control,
                 tapa::ostream<return_dt> &out, int n, int max_dist_x,
                 int max_dist_y, int bw) {
  score_dt max_tracker[BACK_SEARCH_COUNT];
  parent_dt j_tracker[BACK_SEARCH_COUNT];
#pragma HLS array_partition variable = max_tracker dim = 1 complete
#pragma HLS array_partition variable = j_tracker dim = 1 complete

  anchor_dt active[BACK_SEARCH_COUNT];
  score_dt sc[BACK_SEARCH_COUNT];
#pragma HLS data_pack variable = active
#pragma HLS array_partition variable = active complete
#pragma HLS array_partition variable = sc complete

  int batch_count = n / PE_NUM / BATCH_SIZE_INPUT;
  for (int batch = 0; batch < batch_count; batch++) {
    control_dt c = control.read();

    // initial setup of active array and trackers
  init_loop:
    [[tapa::pipeline]] //
    for (int i = 0; i < BACK_SEARCH_COUNT; i++) {
      for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++)
        active[j] = active[j + 1];

      parent_dt j_head = j_tracker[0];
      score_dt max_head = max_tracker[0];
      for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++) {
        j_tracker[j] = j_tracker[j + 1];
        max_tracker[j] = max_tracker[j + 1];
      }

      anchor_dt temp = in.read();
      anchor_dt temp_q = tapa::reg(temp);
      anchor_dt temp_q2 = tapa::reg(temp_q);
      active[BACK_SEARCH_COUNT - 1] = temp_q2;
      max_tracker[BACK_SEARCH_COUNT - 1] =
          c.is_new_read ? (score_dt)temp_q2.w : max_head;
      j_tracker[BACK_SEARCH_COUNT - 1] =
          c.is_new_read ? (parent_dt)(-1) : j_head;
    }

    // main loop
  main_loop:
    [[tapa::pipeline]] //
    for (int i = 0; i < BATCH_SIZE_OUTPUT; i++) {
      // compute the score
      anchor_dt curr = active[0];
#pragma HLS data_pack variable = curr
      ChainDpScore(active, curr, sc, c.avg_qspan, max_dist_x, max_dist_y, bw);

      // update the trackers
      score_dt f_curr = max_tracker[0];
      parent_dt p_curr = j_tracker[0];
      for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++) {
        if (sc[j + 1] + f_curr >= max_tracker[j + 1]) {
          max_tracker[j] = sc[j + 1] + f_curr;
          j_tracker[j] = i + c.batch_num * BATCH_SIZE_OUTPUT;
        } else {
          max_tracker[j] = max_tracker[j + 1];
          j_tracker[j] = j_tracker[j + 1];
        }
      }
      max_tracker[BACK_SEARCH_COUNT - 1] = 0;
      j_tracker[BACK_SEARCH_COUNT - 1] = -1;

      // shift the active array
      for (int j = 0; j < BACK_SEARCH_COUNT - 1; j++)
        active[j] = active[j + 1];

      // read in new query into active array
      anchor_dt temp = in.read();
      anchor_dt temp_q = tapa::reg(temp);
      anchor_dt temp_q2 = tapa::reg(temp_q);
      active[BACK_SEARCH_COUNT - 1] = temp_q2;

      // write out the result
      return_dt ret;
      score_dt f_curr_q = tapa::reg(f_curr);
      score_dt f_curr_q2 = tapa::reg(f_curr_q);
      score_dt f_curr_q3 = tapa::reg(f_curr_q2);
      score_dt f_curr_q4 = tapa::reg(f_curr_q3);
      ret.score = f_curr_q4;
      parent_dt p_curr_q = tapa::reg(p_curr);
      parent_dt p_curr_q2 = tapa::reg(p_curr_q);
      parent_dt p_curr_q3 = tapa::reg(p_curr_q2);
      parent_dt p_curr_q4 = tapa::reg(p_curr_q3);
      ret.parent = p_curr_q4;
      out.write(ret);
    }
  }
}

void Mmap2Anchors(tapa::mmap<anchor_dt_bits> anchors,
                  tapa::ostreams<anchor_dt, PE_NUM> &out, int n) {
  int block_count = n / PE_NUM;
  [[tapa::pipeline]] //
  for (int block = 0; block < block_count; block++) {
    auto anchor_block = anchors[block];
    for (int i = 0; i < PE_NUM; i++) {
      ap_uint<sizeof(anchor_dt) * 8> anchor_bits = anchor_block(
          (i + 1) * sizeof(anchor_dt) * 8 - 1, i * sizeof(anchor_dt) * 8);
      out[i].write(*reinterpret_cast<anchor_dt *>(&anchor_bits));
    }
  }
}

void Mmap2Controls(tapa::mmap<control_dt_bits> controls,
                   tapa::ostreams<control_dt, PE_NUM> &out, int n) {
  int batch_count = n / PE_NUM / BATCH_SIZE_INPUT;
  [[tapa::pipeline]] //
  for (int batch = 0; batch < batch_count; batch++) {
    auto control_batch = controls[batch];
    for (int i = 0; i < PE_NUM; i++) {
      ap_uint<sizeof(control_dt) * 8> control_bits = control_batch(
          (i + 1) * sizeof(control_dt) * 8 - 1, i * sizeof(control_dt) * 8);
      out[i].write(*(reinterpret_cast<control_dt *>(&control_bits)));
    }
  }
}

void Returns2Mmap(tapa::mmap<return_dt_bits> returns,
                  tapa::istreams<return_dt, PE_NUM> &in, int n) {
  int block_count = n / PE_NUM / BATCH_SIZE_INPUT * BATCH_SIZE_OUTPUT;
  [[tapa::pipeline]] //
  for (int block = 0; block < block_count; block++) {
    ap_uint<sizeof(return_dt) * 8 * PE_NUM> return_block;
    for (int i = 0; i < PE_NUM; i++) {
      return_dt ret = in[i].read();
      return_block((i + 1) * sizeof(return_dt) * 8 - 1,
                   i * sizeof(return_dt) * 8) =
          *(reinterpret_cast<ap_uint<sizeof(return_dt) * 8> *>(&ret));
    }
    returns[block] = return_block;
  }
}

void DeviceChainKernel(tapa::mmap<anchor_dt_bits> anchors,
                       tapa::mmap<control_dt_bits> controls,
                       tapa::mmap<return_dt_bits> returns, int n,
                       int max_dist_x, int max_dist_y, int bw) {
  tapa::streams<anchor_dt, PE_NUM> anchors_streams("anchors_streams");
  tapa::streams<control_dt, PE_NUM> controls_streams("controls_streams");
  tapa::streams<return_dt, PE_NUM> returns_streams("returns_streams");
  tapa::task()
      .invoke<tapa::join, PE_NUM>(DeviceChain, anchors_streams,
                                  controls_streams, returns_streams, n,
                                  max_dist_x, max_dist_y, bw)
      .invoke(Mmap2Anchors, anchors, anchors_streams, n)
      .invoke(Mmap2Controls, controls, controls_streams, n)
      .invoke(Returns2Mmap, returns, returns_streams, n);
}
