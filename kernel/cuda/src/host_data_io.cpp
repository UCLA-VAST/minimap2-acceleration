#include "host_data_io.h"
#include "datatypes.h"

void skip_to_EOR(FILE *fp) {
    const char *loc = "EOR";
    while (*loc != '\0') {
        if (fgetc(fp) == *loc) {
            loc++;
        }
    }
}

call_t read_call(FILE *fp) {
    call_t call;

    long long n;
    float avg_qspan;
    int max_dist_x, max_dist_y, bw;

    int t = fscanf(fp, "%lld%f%d%d%d",
            &n, &avg_qspan, &max_dist_x, &max_dist_y, &bw);
    if (t != 5) {
        call.n = ANCHOR_NULL;
        call.avg_qspan = .0;
        return call;
    }

    call.n = n;
    call.avg_qspan = avg_qspan;
    call.max_dist_x = max_dist_x;
    call.max_dist_y = max_dist_y;
    call.bw = bw;

    call.anchors.resize(call.n);

    for (anchor_idx_t i = 0; i < call.n; i++) {
        unsigned int tag;
        int x, w, y;
        fscanf(fp, "%u%d%d%d", &tag, &x, &w, &y);

        anchor_t t;
        t.tag = tag; t.x = x; t.w = w; t.y = y;

        call.anchors[i] = t;
    }

    skip_to_EOR(fp);
    return call;
}

void print_return(FILE *fp, const return_t &data)
{
    fprintf(fp, "%lld\n", (long long)data.n);
    for (anchor_idx_t i = 0; i < data.n; i++) {
        fprintf(fp, "%d\t%d\n", (int)data.scores[i], (int)data.parents[i]);
    }
    fprintf(fp, "EOR\n");
}
