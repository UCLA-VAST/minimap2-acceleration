#include <cstdio>
#include <vector>
#include "host_data_io.h"
#include "host_data.h"
#include "host_kernel.h"

int main(int argc, char **argv) {
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

    std::vector<call_t> calls;
    std::vector<return_t> rets;

    for (call_t call = read_call(in);
            call.n != ANCHOR_NULL;
            call = read_call(in)) {
        calls.push_back(call);
    }

    rets.resize(calls.size());
    host_chain_kernel(calls, rets);

    for (auto it = rets.begin(); it != rets.end(); it++) {
        print_return(out, *it);
    }

    return 0;
}
