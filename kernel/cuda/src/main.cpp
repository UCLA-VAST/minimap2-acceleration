#include <cstdio>
#include <cstdlib>
#include "host_data_io.h"
#include "common.h"
#include "datatypes.h"
#include "host_kernel.h"
#include "device_kernel_wrapper.h"
#include "memory_scheduler.h"

#define READ_BATCH_SIZE 0x7FFFFFFF

int main(int argc, char *argv[]) {
    FILE *in, *out;
    if (argc == 1) {
        in = stdin;
        out = stdout;
    } else if (argc == 3) {
        in = fopen(argv[1], "r");
        out = fopen(argv[2], "w");
    } else {
        fprintf(stderr, "ERROR: %s [infile] [outfile]\n",
                argv[0]);
        return 1;
    }

    bool use_host_kernel = false;
    if (std::getenv("USE_HOST_KERNEL")) {
        use_host_kernel = true;
        fprintf(stderr, "WARN: using host kernel\n");
    }

    if (use_host_kernel) {
        for (call_t call = read_call(in);
                call.n != ANCHOR_NULL;
                call = read_call(in)) {
            return_t ret;
            host_chain_kernel(call, ret);
            print_return(out, ret);
        }
    } else {
        std::vector<control_dt> device_controls;
        std::vector<anchor_dt> device_anchors;
        std::vector<return_dt> device_returns;
        int max_dist_x, max_dist_y, bw;
        std::vector<anchor_idx_t> ns;
        scheduler(in, device_controls, device_anchors, ns,
                READ_BATCH_SIZE,  max_dist_x, max_dist_y, bw);
        device_chain_kernel_wrapper(device_controls, device_anchors,
                device_returns, max_dist_x, max_dist_y, bw);
        std::vector<return_t> rets;
        descheduler(device_controls, device_returns, rets, ns);
        for (auto i = rets.begin(); i != rets.end(); i++) {
            print_return(out, *i);
        }
    }

    return 0;
}
