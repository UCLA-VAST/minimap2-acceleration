#include <cassert>
#include <vector>

#include "memory_scheduler.h"
#include "common.h"
#include "host_data_io.h"


anchor_dt format_anchor(anchor_t curr, bool init, int pe_num)
{
    anchor_dt temp;

    static std::vector<tag_t>  pre_tag(PE_NUM);
    static std::vector<tag_dt> tag_compressed(PE_NUM);

    if (curr.tag != pre_tag[pe_num] || init) {
        tag_compressed[pe_num] = tag_compressed[pe_num] + 1;
    }
    pre_tag[pe_num] = curr.tag;

    temp.x = (loc_dt)curr.x;
    temp.y = (loc_dt)curr.y;
    temp.w = (width_dt)curr.w;
    temp.tag = tag_compressed[pe_num];

    return temp;
}

void scheduler(FILE *in,
        std::vector<control_dt> &control,
        std::vector<anchor_dt> &data,
        std::vector<anchor_idx_t> &ns,
        int read_batch_size, int &max_dist_x, int &max_dist_y, int &bw)
{
    control_dt pe_controls[PE_NUM];
    call_t calls[PE_NUM];

    // initialize PEs with first PE_NUM reads
    int curr_read_id = 0;
    for (; curr_read_id < PE_NUM; curr_read_id++) {
        auto temp = read_call(in);
        calls[curr_read_id] = temp;
        ns.push_back(temp.n);
        pe_controls[curr_read_id].avg_qspan = temp.avg_qspan;
        pe_controls[curr_read_id].tile_num = 0;
        pe_controls[curr_read_id].is_new_read = true;
        // FIXME: assume all max_dist_x, max_dist_y and bw are the same
        max_dist_x = temp.max_dist_x;
        max_dist_y = temp.max_dist_y;
        bw = temp.bw;
    }

    while (true) { // each loop generate one tile of data (1 control block and PE_NUM anchor block)

        bool is_finished = true; // indicate if all anchors are processed
        for (int i = 0; i < PE_NUM; i++) {
            if (calls[i].n != ANCHOR_NULL) {
                is_finished = false;
            } else {
                pe_controls[i].tile_num = TILE_NUM_NULL;
            }
        }
        if (is_finished) break;

        // fill in control data
        for (int i = 0; i < PE_NUM; i++){
            control.push_back(pe_controls[i]);
        }

        // fill in anchor data
        for (int i = 0; i < PE_NUM; i++) {
            if (calls[i].n == ANCHOR_NULL) {
                data.resize(data.size() + TILE_SIZE_ACTUAL);
                continue;
            }

            for (int j = 0; j < TILE_SIZE_ACTUAL; j++) {
                if ((unsigned)j < calls[i].anchors.size()) {
                    data.push_back(format_anchor(calls[i].anchors[j],
                            pe_controls[i].tile_num == 0 && j == 0, i));
                } else data.push_back(anchor_dt());
            }

            if (calls[i].anchors.size() > (unsigned)TILE_SIZE) {
                calls[i].anchors.erase(calls[i].anchors.begin(),
                    calls[i].anchors.begin() + TILE_SIZE);
            } else {
                calls[i].anchors.clear();
            }

            if (calls[i].anchors.empty()) {
                if (curr_read_id > read_batch_size) {
                    calls[i].n = ANCHOR_NULL;
                    continue;
                }
                calls[i] = read_call(in);
                ns.push_back(calls[i].n);
                curr_read_id++;
                pe_controls[i].tile_num = 0;
                pe_controls[i].avg_qspan = calls[i].avg_qspan;
                pe_controls[i].is_new_read = true;
            } else {
                pe_controls[i].tile_num++;
                pe_controls[i].is_new_read = false;
            }
        }
    }
}


void descheduler(
        std::vector<control_dt> &control,
        std::vector<return_dt> &device_returns,
        std::vector<return_t> &rets,
        std::vector<anchor_idx_t> &ns)
{
    int batch_size = PE_NUM * TILE_SIZE;
    int batch_count = device_returns.size() / batch_size;
    int control_size = PE_NUM;

    int n = 0;
    int read_id[PE_NUM] = {0};

    for (int batch = 0; batch < batch_count; batch++) {
        int batch_base = batch * batch_size;
        int control_base = batch * control_size;

        // re-format data
        std::vector<return_dt> temp_data[PE_NUM];
        for (int j = 0; j < PE_NUM; j++) {
            for (int i = 0; i < TILE_SIZE; i++) {
                temp_data[j].push_back(
                    device_returns[batch_base +
                        j * TILE_SIZE + i]);
            }
        }

        for (int i = 0; i < PE_NUM; i++) {
            control_dt cont = control[control_base + i];
            if (cont.tile_num == TILE_NUM_NULL) continue;

            if (cont.is_new_read) {
                read_id[i] = n++;
                rets.resize(n);
                rets[read_id[i]].n = ns[read_id[i]];
            }

            for (auto it = temp_data[i].begin();
                    it != temp_data[i].end(); it++) {
                rets[read_id[i]].scores.push_back(it->score);
                rets[read_id[i]].parents.push_back(it->parent);
            }
        }
    }
}
