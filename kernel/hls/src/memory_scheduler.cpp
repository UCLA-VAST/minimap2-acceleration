#include <cassert>

#include "memory_scheduler.h"
#include "common.h"
#include "host_data_io.h"
#include "CL/opencl.h"


anchor_dt format_anchor(anchor_t curr, bool init,
        bool backup, bool restore, int pe_num)
{
    anchor_dt temp = 0;

    assert(PE_NUM == 8);
    static tag_t pre_tag[8];
    static tag_t backup_tag[8];
    static tag_dt tag_compressed[8];

    if (backup) {
        backup_tag[pe_num] = pre_tag[pe_num];
    } else if (restore) {
        pre_tag[pe_num] = backup_tag[pe_num];
    }

    if (curr.tag != pre_tag[pe_num] || init) {
        tag_compressed[pe_num] = tag_compressed[pe_num] + 1;
    }
    pre_tag[pe_num] = curr.tag;

    temp |= anchor_dt(tag_compressed[pe_num]);
    temp <<= 16;

    temp |= anchor_dt((loc_dt)curr.x);
    temp <<= 16;

    temp |= anchor_dt((width_dt)curr.w);
    temp <<= 16;

    temp |= anchor_dt((loc_dt)curr.y);

    return temp;
}

/*
 * try to interleave the anchors of PE_NUM reads
 * |-----------|---------------|-----|-----------|---------------|---00|-----------|-----------0000|00000|
 *             |<- TILE_SIZE ->|<-h->|
 * |ctrl info  |   anchors of read 0 |ctrl info  |   anchors of read 1 | ctrl info |  anchors of read 2  |
 *                     case 1                           case 1                           case 2
 *
 * or put all control info together
 *
 * try to interleave the anchors of PE_NUM reads
 * |-----------|---------------|-----|---------------|---00|-----------0000|00000|
 *             |<- TILE_SIZE ->|<-h->|
 * |ctrl info  |   anchors of read 0 |   anchors of read 1 |  anchors of read 2  |
 *                      case 1                case 1                 case 2
 *
 * cases are shown below:
 * |---------------|-----|---   case 1
 * |---------------|---  |      case 1
 * |-------------  |     |      case 2
 * |<- TILE_SIZE ->|<-h->|
 */
void scheduler(FILE *in,
        std::vector<anchor_dt, aligned_allocator<anchor_dt> >& data,
        std::vector<anchor_idx_t> &ns,
        int read_batch_size, int &max_dist_x, int &max_dist_y, int &bw)
{
    bool is_new_read[PE_NUM] = {false};
    qspan_t avg_qspan[PE_NUM] = {0};
    int tile_num[PE_NUM] = {0};
    call_t calls[PE_NUM];

    // initialize PEs with first PE_NUM reads
    int curr_read_id = 0;
    for (; curr_read_id < PE_NUM; curr_read_id++){
        auto temp = read_call(in);
        calls[curr_read_id] = temp;
        ns.push_back(temp.n);
        is_new_read[curr_read_id] = true;
        avg_qspan[curr_read_id] = temp.avg_qspan * (qspan_t)0.01;
        tile_num[curr_read_id] = 0;
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
                tile_num[i] = TILE_NUM_NULL;
            }
        }
        if (is_finished) break;

        // fill in control data
        for (int i = 0; i < PE_NUM; i++){
            anchor_dt control = 0;
            control |= anchor_dt(is_new_read[i]);
            // if this tile is the start of a new read
            control <<= 32;
            control |= anchor_dt(*((int *)(avg_qspan + i)));
            control <<= 16;
            control |= anchor_dt(tile_num[i]);
            data.push_back(control);
        }

        std::vector<anchor_dt> temp_data[PE_NUM];

        // fill in anchor data
        for (int i = 0; i < PE_NUM; i++) {
            if (calls[i].n == ANCHOR_NULL) {
                temp_data[i].clear();
                temp_data[i].resize(TILE_SIZE + BACK_SEARCH_COUNT, 0);
            }

            for (int j = 0; j < TILE_SIZE + BACK_SEARCH_COUNT; j++) {
                if ((unsigned)j < calls[i].anchors.size()) {
                    bool backup = j == TILE_SIZE;
                    bool restore = tile_num[i] != 0 && j == 0;
                    temp_data[i].push_back(
                        format_anchor(calls[i].anchors[j],
                            tile_num[i] == 0 && j == 0,
                            backup, restore, i));
                } else temp_data[i].push_back(0);
            }

            if (calls[i].anchors.size() > (unsigned)TILE_SIZE) {
                calls[i].anchors.erase(calls[i].anchors.begin(),
                    calls[i].anchors.begin() + TILE_SIZE);
            } else {
                calls[i].anchors.clear();
            }

            if (calls[i].anchors.empty()) {
                if (curr_read_id >= read_batch_size) { // ">" will results in read_batch_size+1 reads
                    calls[i].n = ANCHOR_NULL;
                    continue;
                }
                calls[i] = read_call(in);
                ns.push_back(calls[i].n);
                curr_read_id++;
                is_new_read[i] = true;
                avg_qspan[i] = calls[i].avg_qspan * (qspan_t)0.01;
                tile_num[i] = 0;
            } else {
                is_new_read[i] = false;
                tile_num[i]++;
            }
        }

        // re-format data
        for (int i = 0; i < TILE_SIZE + BACK_SEARCH_COUNT; i++) {
            for (int j = 0; j < PE_NUM; j++) {
                data.push_back(temp_data[j][i]);
            }
        }

        for (int j = 0; j < PE_NUM; j++) {
            temp_data[j].clear();
        }
    }
}


void descheduler(
        std::vector<return_dt, aligned_allocator<return_dt> > &device_returns,
        std::vector<return_t> &rets,
        std::vector<anchor_idx_t> &ns)
{
    int batch_size = PE_NUM * RETURN_BLOCK_PER_BATCH;
    int batch_count = device_returns.size() / batch_size;

    int n = 0;
    int read_id[PE_NUM] = {0};

    for (int batch = 0; batch < batch_count; batch++) {
        int batch_base = batch * batch_size;

        // re-format data
        std::vector<anchor_dt> temp_data[PE_NUM];
        for (int i = 0; i < TILE_SIZE; i++) {
            for (int j = 0; j < PE_NUM; j++) {
                temp_data[j].push_back(
                    device_returns[batch_base +
                        (i + 1) * PE_NUM + j]);
            }
        }

        for (int i = 0; i < PE_NUM; i++) {
            anchor_dt control = device_returns[batch_base + i];
            int tile_num = control & 0xFFFF;
            if (tile_num == TILE_NUM_NULL) continue;

            bool is_new_read = control[48];
            if (is_new_read) {
                read_id[i] = n++;
                rets.resize(n);
                rets[read_id[i]].n = ns[read_id[i]];
            }

            for (auto it = temp_data[i].begin();
                    it != temp_data[i].end(); it++) {
                score_t score = (*it) >> 32;
                parent_t par = (*it) & ((1ULL << 32) - 1);
                rets[read_id[i]].scores.push_back(score);
                rets[read_id[i]].parents.push_back(par);
            }
        }
    }
}
