// summary.cpp
// Reads cycle results saved by each optimization run and prints
// a combined comparison table.
//
// Usage: ./summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Updated to include all 7 stages
static const char* STAGES[] = {
    "Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "Threshold", "Hysteresis"
};
static const int N_STAGES = 7;

// Read 7 cycle values from a file into array
int read_cycles(const char* filename, uint64_t* out) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("  ERROR: Cannot open %s\n", filename);
        return 0;
    }
    for (int i = 0; i < N_STAGES; i++) {
        if (fscanf(f, "%llu", (unsigned long long*)&out[i]) != 1) {
            printf("  ERROR: Bad format in %s at line %d\n", filename, i+1);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        printf("Usage: %s cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt\n", argv[0]);
        return 1;
    }

    // [opt_level][stage] - Array size updated to [5][7]
    uint64_t cyc[5][7];

    for (int i = 0; i < 5; i++) {
        if (!read_cycles(argv[i+1], cyc[i])) return 1;
    }

    // Compute totals
    uint64_t totals[5] = {0};
    for (int i = 0; i < 5; i++)
        for (int s = 0; s < N_STAGES; s++)
            totals[i] += cyc[i][s];

    // Header
    printf("\n  COMPILER OPTIMIZATION SWEEP — Summary Table (7 Stages)\n\n");
    printf("┌────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬─────────────┐\n");
    printf("│ %-10s │ %-12s │ %-12s │ %-12s │ %-12s │ %-12s │ %-11s │\n",
           "Stage", "-O0", "-O2", "-O3", "-Os", "-Ofast", "O0 -> Ofast");
    printf("├────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼─────────────┤\n");

    // Stage rows
    for (int s = 0; s < N_STAGES; s++) {
        double speedup = (cyc[4][s] > 0) ? (double)cyc[0][s] / cyc[4][s] : 0.0;
        printf("│ %-10s │ %12llu │ %12llu │ %12llu │ %12llu │ %12llu │ %-11.1fx │\n",
               STAGES[s], (unsigned long long)cyc[0][s], (unsigned long long)cyc[1][s],
               (unsigned long long)cyc[2][s], (unsigned long long)cyc[3][s],
               (unsigned long long)cyc[4][s], speedup);
    }

    // Total row
    printf("├────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼─────────────┤\n");
    double total_speedup = (totals[4] > 0) ? (double)totals[0] / totals[4] : 0.0;
    printf("│ %-10s │ %12llu │ %12llu │ %12llu │ %12llu │ %12llu │ %-11.1fx │\n",
           "TOTAL", (unsigned long long)totals[0], (unsigned long long)totals[1],
           (unsigned long long)totals[2], (unsigned long long)totals[3],
           (unsigned long long)totals[4], total_speedup);
    printf("└────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴─────────────┘\n");

    // Amdahl's Law
    printf("\n  Amdahl's Law (Priority based on -Ofast):\n");
    for (int s = 0; s < N_STAGES; s++) {
        double pct_best = 100.0 * cyc[4][s] / totals[4];
        printf("  %-10s  %5.1f%% of total\n", STAGES[s], pct_best);
    }
    printf("\n");
    return 0;
}