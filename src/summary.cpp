/**
 * @file summary.cpp
 * @brief Cycle results analyzer and pipeline optimization advisor.
 * * Reads performance cycle counts from various compiler optimization levels 
 * (e.g., -O0 through -Ofast) across the 7 stages of the Canny Edge Detection 
 * pipeline. Applies Amdahl's Law to calculate the percentage of total execution 
 * time spent in each stage and outputs prioritized advice on which stages 
 * would benefit most from RISC-V Vector (RVV) Extension vectorization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief String labels for the 7 stages of the Canny pipeline.
 */
static const char* STAGES[] = {
    "Gaussian", "Sobel", "Magnitude", "Direction", "NMS", "Threshold", "Hysteresis"
};

/**
 * @brief Total number of pipeline stages to be analyzed.
 */
static const int N_STAGES = 7;

/**
 * @brief Reads cycle counts for all pipeline stages from a given file.
 * * @param filename Path to the text file containing sequential cycle counts.
 * @param out Array of uint64_t to store the parsed cycle counts. Must have space for N_STAGES.
 * @return int 1 on successful parsing, 0 on failure (file not found or bad format).
 */
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

/**
 * @brief Main execution entry point.
 * * Expects exactly 5 input files corresponding to different optimization 
 * runs (e.g., cycles_O0.txt to cycles_Ofast.txt). Computes the optimization 
 * priority based on the 5th file (index 4, typically the -Ofast benchmark run).
 * * @param argc Argument count. Expects at least 6 (program name + 5 file paths).
 * @param argv Argument vector containing paths to the 5 cycle count files.
 * @return int 0 on success, 1 on invalid arguments or read failure.
 */
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