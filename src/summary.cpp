// summary.cpp
// Reads cycle results and prints full pipeline optimization advice.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static const char* STAGES[] = {
    "Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "Threshold", "Hysteresis"
};
static const int N_STAGES = 7;

int read_cycles(const char* filename, uint64_t* out) {
    FILE* f = fopen(filename, "r");
    if (!f) { printf("  ERROR: Cannot open %s\n", filename); return 0; }
    for (int i = 0; i < N_STAGES; i++) {
        if (fscanf(f, "%llu", (unsigned long long*)&out[i]) != 1) {
            printf("  ERROR: Bad format in %s\n", filename);
            fclose(f); return 0;
        }
    }
    fclose(f); return 1;
}

int main(int argc, char** argv) {
    if (argc < 6) return 1;
    uint64_t cyc[5][7];
    uint64_t totals[5] = {0};

    for (int i = 0; i < 5; i++) {
        if (!read_cycles(argv[i+1], cyc[i])) return 1;
        for (int s = 0; s < N_STAGES; s++) totals[i] += cyc[i][s];
    }

    printf("\n  Amdahl's Law — Where to focus RVV optimization (based on -Ofast):\n");
    printf("  ──────────────────────────────────────────────────────────────\n");

    for (int s = 0; s < N_STAGES; s++) {
        double pct_best = 100.0 * cyc[4][s] / totals[4];
        
        // This is the advice logic you were missing:
        const char* verdict;
        if      (pct_best >= 35.0) verdict = "<< HIGH PRIORITY: vectorize with RVV!";
        else if (pct_best >= 15.0) verdict = "<  MEDIUM: worth vectorizing";
        else                       verdict = "   LOW: small gain, skip";

        printf("  %-10s  %5.1f%% of total   %s\n", STAGES[s], pct_best, verdict);
    }
    printf("\n");
    return 0;
}
