#include <cassert>

#include "host_data_io.h"
#include "memory_scheduler.h"

anchor_dt compress_anchor(anchor_t curr, bool init, bool backup, bool restore,
                          int pe_num) {
  static tag_t pre_tag[PE_NUM];
  static tag_t backup_tag[PE_NUM];
  static tag_dt tag_compressed[PE_NUM];

  if (backup)
    backup_tag[pe_num] = pre_tag[pe_num];
  else if (restore)
    pre_tag[pe_num] = backup_tag[pe_num];

  if (curr.tag != pre_tag[pe_num] || init)
    tag_compressed[pe_num] = tag_compressed[pe_num] + 1;
  pre_tag[pe_num] = curr.tag;

  anchor_dt temp;
  temp.tag = tag_compressed[pe_num];
  temp.x = curr.x;
  temp.y = curr.y;
  temp.w = curr.w;
  return temp;
}

void scheduler(FILE *in, std::vector<anchor_dt> &anchors,
               std::vector<control_dt> &controls, std::vector<anchor_idx_t> &ns,
               int &max_dist_x, int &max_dist_y, int &bw) {
  bool is_new_read[PE_NUM] = {false};
  qspan_t avg_qspan[PE_NUM] = {0};
  int batch_num[PE_NUM] = {0};
  call_t calls[PE_NUM];

  // initialize PEs with first PE_NUM reads
  for (int pe = 0; pe < PE_NUM; pe++) {
    calls[pe] = read_call(in);
    ns.push_back(calls[pe].n);
    is_new_read[pe] = true;
    avg_qspan[pe] = calls[pe].avg_qspan * (qspan_t)0.01;
    batch_num[pe] = 0;
    // FIXME: assume all max_dist_x, max_dist_y and bw are the same
    max_dist_x = calls[pe].max_dist_x;
    max_dist_y = calls[pe].max_dist_y;
    bw = calls[pe].bw;
  }

  // each loop generate one batch of data (PE_NUM controls and PE_NUM *
  // BATCH_SIZE_INPUT anchor block)
  while (true) {
    bool is_finished = true; // indicate if all anchors are processed
    for (int pe = 0; pe < PE_NUM; pe++)
      if (calls[pe].n != ANCHOR_NULL)
        is_finished = false;
      else
        batch_num[pe] = BATCH_NUM_NULL;
    if (is_finished)
      break;

    // fill in control data
    for (int pe = 0; pe < PE_NUM; pe++) {
      control_dt control;
      control.is_new_read = is_new_read[pe];
      control.avg_qspan = avg_qspan[pe];
      control.batch_num = batch_num[pe];
      controls.push_back(control);
    }

    std::vector<anchor_dt> batch_data[PE_NUM];

    // fill in anchor data
    for (int pe = 0; pe < PE_NUM; pe++) {
      // clear the batch data if it is a new null batch
      if (calls[pe].n == ANCHOR_NULL) {
        batch_data[pe].clear();
        batch_data[pe].resize(BATCH_SIZE_INPUT, anchor_dt());
      }

      // fill in the anchor data
      for (int block = 0; block < BATCH_SIZE_INPUT; block++) {
        if ((unsigned)block < calls[pe].anchors.size()) {
          bool init = batch_num[pe] == 0 && block == 0;
          bool backup = block == BATCH_SIZE_OUTPUT;
          bool restore = batch_num[pe] != 0 && block == 0;
          batch_data[pe].push_back(compress_anchor(calls[pe].anchors[block],
                                                   init, backup, restore, pe));
        } else
          batch_data[pe].push_back(anchor_dt());
      }

      // remove the filled anchors from the original call
      if (calls[pe].anchors.size() > (unsigned)BATCH_SIZE_OUTPUT)
        calls[pe].anchors.erase(calls[pe].anchors.begin(),
                                calls[pe].anchors.begin() + BATCH_SIZE_OUTPUT);
      else
        calls[pe].anchors.clear();

      if (calls[pe].anchors.empty()) {
        // if the call is finished, read in a new call
        calls[pe] = read_call(in);
        ns.push_back(calls[pe].n);
        is_new_read[pe] = true;
        avg_qspan[pe] = calls[pe].avg_qspan * (qspan_t)0.01;
        batch_num[pe] = 0;
      } else {
        // if the call is not finished, continue with the current call
        is_new_read[pe] = false;
        batch_num[pe]++;
      }
    }

    // transpose the anchor data
    for (int block = 0; block < BATCH_SIZE_INPUT; block++)
      for (int pe = 0; pe < PE_NUM; pe++)
        anchors.push_back(batch_data[pe][block]);

    // prepare for the next batch
    for (int pe = 0; pe < PE_NUM; pe++)
      batch_data[pe].clear();
  }
}

void descheduler(std::vector<return_dt> &device_returns,
                 std::vector<control_dt> &device_controls,
                 std::vector<return_t> &rets, std::vector<anchor_idx_t> &ns) {
  int batch_count = device_returns.size() / PE_NUM / BATCH_SIZE_OUTPUT;

  int pe_current_n[PE_NUM] = {0};
  int current_n = 0;

  for (int batch = 0; batch < batch_count; batch++) {
    for (int pe = 0; pe < PE_NUM; pe++) {
      control_dt control = device_controls[batch * PE_NUM + pe];

      // Skip if the control is for a null batch
      if (control.batch_num == BATCH_NUM_NULL)
        continue;

      // Points to a new read
      if (control.is_new_read) {
        pe_current_n[pe] = current_n;
        rets.resize(current_n + 1, return_t());
        rets[current_n].n = ns[current_n];
        current_n++;
      }

      // Fill in the scores and parents
      int pe_n = pe_current_n[pe];
      for (auto block = 0; block < BATCH_SIZE_OUTPUT; block++) {
        int ret_idx = batch * BATCH_SIZE_OUTPUT * PE_NUM + block * PE_NUM + pe;
        rets[pe_n].scores.push_back(device_returns[ret_idx].score);
        rets[pe_n].parents.push_back(device_returns[ret_idx].parent);
      }
    }
  }
}
