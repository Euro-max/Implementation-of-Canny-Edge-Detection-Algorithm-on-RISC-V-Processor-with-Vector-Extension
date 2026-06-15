// summary.cpp
// Reads cycle results saved by each optimization run and prints
// a combined comparison table — same data as the sweep, no re-running!
//
// Usage: ./summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Stage names in order (must match order written by main.cpp)
static const char* STAGES[] = {
    "Gaussian", "Sobel", "Magnitude", "Direction"
};
static const int N_STAGES = 4;

// Read 4 cycle values from a file into array
// Returns 1 on success, 0 on failure
int read_cycles(const char* filename, uint64_t* out) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("  ERROR: Cannot open %s\n", filename);
        printf("         Run 'make sweep' first to generate cycle files.\n");
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
        printf("Usage: %s cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt\n",
               argv[0]);
        return 1;
    }

    // ── Read cycle data from files ────────────────────────────────────────────
    uint64_t cyc[5][4];   // [opt_level][stage]

    for (int i = 0; i < 5; i++) {
        if (!read_cycles(argv[i+1], cyc[i])) return 1;
    }

    // ── Compute totals ────────────────────────────────────────────────────────
    uint64_t totals[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5; i++)
        for (int s = 0; s < N_STAGES; s++)
            totals[i] += cyc[i][s];

    // ── Header ───────────────────────────────────────────────────────────────
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("  ║                        COMPILER OPTIMIZATION SWEEP — Summary Table                               ║\n");
    printf("  ║                        (data from last 'make sweep' run — exact values)                          ║\n");
    printf("  ╚══════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // ── Table top border ─────────────────────────────────────────────────────
    printf("┌────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬─────────────┐\n");
    printf("│ %-10s │ %-12s │ %-12s │ %-12s │ %-12s │ %-12s │ %-11s │\n",
           "Stage", "-O0 Cycles", "-O2 Cycles", "-O3 Cycles", "-Os Cycles", "-Ofast Cycle", "O0 -> Ofast");
    printf("├────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼─────────────┤\n");

    // ── Stage rows ────────────────────────────────────────────────────────────
    for (int s = 0; s < N_STAGES; s++) {

        // Compute speedup O0 → Ofast for this stage
        double speedup = (cyc[4][s] > 0)
                         ? (double)cyc[0][s] / cyc[4][s]
                         : 0.0;

        char sp_str[20];
        if      (speedup >= 10.0) snprintf(sp_str, sizeof(sp_str), "%.1fx  >>", speedup);
        else if (speedup >=  5.0) snprintf(sp_str, sizeof(sp_str), "%.1fx  > ", speedup);
        else                      snprintf(sp_str, sizeof(sp_str), "%.1fx    ", speedup);

        printf("│ %-10s │ %12llu │ %12llu │ %12llu │ %12llu │ %12llu │ %-11s │\n",
               STAGES[s],
               (unsigned long long)cyc[0][s],
               (unsigned long long)cyc[1][s],
               (unsigned long long)cyc[2][s],
               (unsigned long long)cyc[3][s],
               (unsigned long long)cyc[4][s],
               sp_str);
    }

    // ── Total row ─────────────────────────────────────────────────────────────
    printf("├────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼─────────────┤\n");

    double total_speedup = (totals[4] > 0)
                           ? (double)totals[0] / totals[4]
                           : 0.0;
    char ts[20];
    snprintf(ts, sizeof(ts), "%.1fx TOTAL", total_speedup);

    printf("│ %-10s │ %12llu │ %12llu │ %12llu │ %12llu │ %12llu │ %-11s │\n",
           "TOTAL",
           (unsigned long long)totals[0],
           (unsigned long long)totals[1],
           (unsigned long long)totals[2],
           (unsigned long long)totals[3],
           (unsigned long long)totals[4],
           ts);

    printf("└────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴─────────────┘\n");

    // ── Amdahl's Law section ──────────────────────────────────────────────────
    printf("\n");
    printf("  Amdahl's Law — Where to focus RVV optimization (based on -Ofast):\n");
    printf("  ──────────────────────────────────────────────────────────────\n");

    for (int s = 0; s < N_STAGES; s++) {
        double pct_best = 100.0 * cyc[4][s] / totals[4];
        double sp      = (cyc[4][s] > 0) ? (double)cyc[0][s] / cyc[4][s] : 0.0;

        const char* verdict;
        if      (pct_best >= 35.0) verdict = "<< HIGH PRIORITY: vectorize with RVV!";
        else if (pct_best >= 15.0) verdict = "<  MEDIUM: worth vectorizing";
        else                       verdict = "   LOW: small gain, skip";

        printf("  %-10s  %5.1f%% of total      speedup so far: %4.1fx   %s\n",
               STAGES[s], pct_best, sp, verdict);
    }

    printf("\n");
    printf("  O0 -> Ofast gave %.1fx total speedup for free (just compiler flags).\n", total_speedup);
    printf("\n");

    return 0;
}
