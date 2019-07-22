#include <cstdio>
#include <cstdlib>
#include <string>
#include <ap_int.h>
#include "host_data_io.h"
#include "datatypes.h"
#include "host_kernel.h"
#include "device_kernel_wrapper.h"
#include "memory_scheduler.h"
#include "CL/opencl.h"

#define READ_LIMIT 0x7FFFFFFF

int main(int argc, char *argv[]) {
    FILE *in, *out;
    if (argc == 2) {
        in = stdin;
        out = stdout;
    } else if (argc == 4) {
        in = fopen(argv[2], "r");
        out = fopen(argv[3], "w");
    } else {
        fprintf(stderr, "ERROR: %s bitstream [infile] [outfile]\n",
                argv[0]);
        return 1;
    }

    std::string kernel(argv[1]);

    bool use_host_kernel = false;
    if (std::getenv("USE_HOST_KERNEL")) {
        use_host_kernel = true;
        fprintf(stderr, "WARN: using host kernel\n");
    }

    if (use_host_kernel) {
        call_t call;
        int count = 0;
        for (; call.n != ANCHOR_NULL && count < READ_LIMIT; count++) {
            call = read_call(in);
            return_t ret;
            host_chain_kernel(call, ret);
            print_return(out, ret);
        }
    } else {
        std::vector<anchor_dt, aligned_allocator<anchor_dt> > device_anchors;
        std::vector<return_dt, aligned_allocator<return_dt> > device_returns;
        int max_dist_x, max_dist_y, bw;
        std::vector<anchor_idx_t> ns;

        scheduler(in, device_anchors, ns,
                READ_LIMIT,  max_dist_x, max_dist_y, bw);

        // compute
        device_chain_kernel_wrapper(kernel,
                device_anchors, device_returns,
                max_dist_x, max_dist_y, bw);

        // format the result back and print
        std::vector<return_t> rets;
        descheduler(device_returns, rets, ns);
        for (auto i = rets.begin(); i != rets.end(); i++) {
            print_return(out, *i);
        }
    }

    return 0;
}
