#include "common.h"
#include "datatypes.h"
#include "host_data_io.h"
#include "memory_scheduler.h"
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char *argv[]) {
  FILE *in, *out;
  if (argc == 2) {
    in = stdin;
    out = stdout;
  } else if (argc == 4) {
    in = fopen(argv[2], "r");
    out = fopen(argv[3], "w");
  } else {
    fprintf(stderr, "ERROR: %s bitstream [infile] [outfile]\n", argv[0]);
    return 1;
  }

  std::string bitstream(argv[1]);

  std::vector<anchor_dt> device_anchors;
  std::vector<control_dt> device_control;
  std::vector<return_dt> device_returns;
  int max_dist_x, max_dist_y, bw;
  std::vector<anchor_idx_t> ns;

  // read input
  scheduler(in, device_anchors, device_control, ns, max_dist_x, max_dist_y, bw);

  // compute
  device_returns.resize(device_anchors.size() / BATCH_SIZE_INPUT *
                        BATCH_SIZE_OUTPUT);
  tapa::invoke(DeviceChainKernel, bitstream,
               tapa::read_only_mmap<anchor_dt_bits>(
                   (anchor_dt_bits *)device_anchors.data(),
                   device_anchors.size() / PE_NUM),
               tapa::read_only_mmap<control_dt_bits>(
                   (control_dt_bits *)device_control.data(),
                   device_control.size() / PE_NUM),
               tapa::write_only_mmap<return_dt_bits>(
                   (return_dt_bits *)device_returns.data(),
                   device_returns.size() / PE_NUM),
               device_anchors.size(), max_dist_x, max_dist_y, bw);

  // format the result back and print
  std::vector<return_t> rets;
  descheduler(device_returns, device_control, rets, ns);
  for (auto i = rets.begin(); i != rets.end(); i++)
    print_return(out, *i);

  return 0;
}
