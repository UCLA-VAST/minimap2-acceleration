#ifndef MEMORY_SCHEDULER_H
#define MEMORY_SCHEDULER_H

#include "common.h"
#include "datatypes.h"
#include <vector>

void scheduler(FILE *in, std::vector<anchor_dt> &data,
               std::vector<control_dt> &controls, std::vector<anchor_idx_t> &ns,
               int &max_dist_x, int &max_dist_y, int &bw);
void descheduler(std::vector<return_dt> &device_returns,
                 std::vector<control_dt> &device_controls,
                 std::vector<return_t> &rets, std::vector<anchor_idx_t> &ns);

#endif // MEMORY_SCHEDULER_H
