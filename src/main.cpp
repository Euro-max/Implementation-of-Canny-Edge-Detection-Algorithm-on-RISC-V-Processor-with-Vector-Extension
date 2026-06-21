/**
 * @file main.cpp
 * @brief Main execution entry point for Canny Edge Detection (7-stage pipeline).
 *
 * MODIFIED: measures BOTH metrics for every stage, side by side:
 *   1. rdcycle  -> RISC-V hardware cycle counter (target-instruction-count proxy)
 *   2. clock_gettime(CLOCK_MONOTONIC) -> real host wall-clock time
 * Requires the _clock_gettime syscall stub added to syscalls.cpp
 * (syscall 113 on rv64 Linux) -- see accompanying syscalls.cpp patch.
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "nms_threshold.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ── Metric 1: RISC-V hardware cycle counter ─────────────────────────────
static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC ((clockid_t)4)
#endif
extern "C" int clock_gettime(clockid_t clk_id, struct timespec* tp);

// ── Metric 2: wall-clock time via clock_gettime(CLOCK_MONOTONIC) ────────
static inline struct timespec now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

static inline uint64_t elapsed_ns(const struct timespec& start, const struct timespec& end) {
    uint64_t sec_diff  = (uint64_t)(end.tv_sec - start.tv_sec);
    int64_t  nsec_diff = (int64_t)end.tv_nsec - (int64_t)start.tv_nsec;
    return sec_diff * 1000000000ULL + (uint64_t)nsec_diff;
}

// 7-arg bottleneck finder, used twice (once for cycles, once for time)
static uint64_t find_max(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f, uint64_t g) {
    uint64_t m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    if (d > m) m = d;
    if (e > m) m = e;
    if (f > m) m = f;
    if (g > m) m = g;
    return m;
}

static void print_table(uint64_t cyc_gaussian, uint64_t cyc_sobel,
                        uint64_t cyc_mag,      uint64_t cyc_dir,
                        uint64_t cyc_nms,      uint64_t cyc_thresh,
                        uint64_t cyc_hyst,
                        uint64_t ns_gaussian,  uint64_t ns_sobel,
                        uint64_t ns_mag,       uint64_t ns_dir,
                        uint64_t ns_nms,       uint64_t ns_thresh,
                        uint64_t ns_hyst,      int reps) {

    uint64_t cyc_total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir + cyc_nms + cyc_thresh + cyc_hyst;
    uint64_t cyc_hot    = find_max(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, cyc_nms, cyc_thresh, cyc_hyst);

    uint64_t ns_total  = ns_gaussian + ns_sobel + ns_mag + ns_dir + ns_nms + ns_thresh + ns_hyst;
    uint64_t ns_hot    = find_max(ns_gaussian, ns_sobel, ns_mag, ns_dir, ns_nms, ns_thresh, ns_hyst);

    printf("\n+--------------+----------------+--------+-------------+--------+--------------+\n");
    printf("|                  Canny Pipeline - Performance Results                        |\n");
    printf("|                    (average over %d runs)                                    |\n", reps);
    printf("+--------------+----------------+--------+-------------+--------+--------------+\n");
    printf("| Stage        | Cycles         |  %%     | Time (us)   |  %%     | Bottleneck?  |\n");
    printf("+--------------+----------------+--------+-------------+--------+--------------+\n");

    #define ROW(name, cyc, ns) \
        printf("| %-12s | %14llu | %5.1f%% | %11.3f | %5.1f%% | %-12s |\n", \
                name, \
                (unsigned long long)(cyc), (cyc_total > 0) ? 100.0*(cyc)/cyc_total : 0.0, \
                (double)(ns) / 1000.0, (ns_total > 0) ? 100.0*(ns)/ns_total : 0.0, \
                (((cyc)==cyc_hot) || ((ns)==ns_hot)) ? "< HOT" : "")

    ROW("Gaussian",   cyc_gaussian, ns_gaussian);
    ROW("Sobel",      cyc_sobel,    ns_sobel);
    ROW("Magnitude",  cyc_mag,      ns_mag);
    ROW("Direction",  cyc_dir,      ns_dir);
    ROW("NMS",        cyc_nms,      ns_nms);
    ROW("Threshold",  cyc_thresh,   ns_thresh);
    ROW("Hysteresis", cyc_hyst,     ns_hyst);
    #undef ROW

    printf("+--------------+----------------+--------+-------------+--------+--------------+\n");
    printf("| %-12s | %14llu | %5.1f%% | %11.3f | %5.1f%% |              |\n",
            "TOTAL", (unsigned long long)cyc_total, (cyc_total > 0) ? 100.0 : 0.0,
            (double)ns_total / 1000.0, (ns_total > 0) ? 100.0 : 0.0);
    printf("+--------------+----------------+--------+-------------+--------+--------------+\n");
    printf("| Cycles = RISC-V rdcycle CSR (instruction-count proxy, not wall time)         |\n");
    printf("| Time   = host wall-clock via clock_gettime(CLOCK_MONOTONIC), real seconds    |\n");
    printf("+--------------------------------------------------------------------------------+\n\n");
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ./canny <input.raw> <output.raw> <width> <height> [cycles.txt]\n");
        return 1;
    }

    int W = atoi(argv[3]);
    int H = atoi(argv[4]);

    printf("\n+----------------------------------------------+\n");
    printf("|        Canny Edge Detection Pipeline         |\n");
    printf("|        Running on RISC-V via QEMU            |\n");
    printf("+----------------------------------------------+\n");
    printf("|  Input : %-35s|\n", argv[1]);
    printf("|  Output: %-35s|\n", argv[2]);
    printf("|  Size  : %dx%d%-*s|\n", W, H, (int)(35 - 1 - snprintf(NULL,0,"%dx%d",W,H)), "");
    printf("+----------------------------------------------+\n\n");

    uint8_t* img_in    = allocate_buffer(W, H);
    uint8_t* img_blur  = allocate_buffer(W, H);
    int16_t* gx        = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    int16_t* gy        = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    uint8_t* img_mag   = allocate_buffer(W, H);
    uint8_t* img_dir   = allocate_buffer(W, H);
    uint8_t* img_nms   = allocate_buffer(W, H);
    uint8_t* img_edges = allocate_buffer(W, H);

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir || !img_nms || !img_edges) {
        fprintf(stderr, "ERROR: Memory allocation failed!\n");
        return 1;
    }

    if (!load_raw(argv[1], img_in, W, H)) return 1;

    const int REPS = 100;
    uint64_t c0;
    struct timespec t0, t1;

    // ── Gaussian ─────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
    uint64_t cyc_gaussian = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_gaussian = elapsed_ns(t0, t1) / REPS;

    // ── Sobel ────────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) compute_sobel(img_blur, gx, gy, W, H);
    uint64_t cyc_sobel = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_sobel = elapsed_ns(t0, t1) / REPS;

    // ── Magnitude ────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) compute_magnitude_l1(gx, gy, img_mag, W, H);
    uint64_t cyc_mag = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_mag = elapsed_ns(t0, t1) / REPS;

    // ── Direction ────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) compute_direction(gx, gy, img_dir, W, H);
    uint64_t cyc_dir = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_dir = elapsed_ns(t0, t1) / REPS;

    // ── NMS ──────────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) apply_nms(img_mag, img_dir, img_nms, W, H);
    uint64_t cyc_nms = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_nms = elapsed_ns(t0, t1) / REPS;

    // ── Threshold ────────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) apply_double_threshold(img_nms, img_edges, W, H, 30, 90);
    uint64_t cyc_thresh = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_thresh = elapsed_ns(t0, t1) / REPS;

    // ── Hysteresis ───────────────────────────────────────────────────────
    c0 = read_cycles();
    t0 = now();
    for (int i = 0; i < REPS; i++) apply_hysteresis(img_edges, W, H);
    uint64_t cyc_hyst = (read_cycles() - c0) / REPS;
    t1 = now();
    uint64_t ns_hyst = elapsed_ns(t0, t1) / REPS;

    save_raw(argv[2], img_edges, W, H);

    print_table(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, cyc_nms, cyc_thresh, cyc_hyst,
                ns_gaussian,  ns_sobel,  ns_mag,  ns_dir,  ns_nms,  ns_thresh,  ns_hyst,
                REPS);

    if (argc >= 6) {
        FILE* cf = fopen(argv[5], "w");
        if (cf) {
            // First 7 lines: cycles (unchanged format, existing tools keep working)
            // Next 7 lines: nanoseconds (new)
            fprintf(cf, "%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n",
                    (unsigned long long)cyc_gaussian, (unsigned long long)cyc_sobel,
                    (unsigned long long)cyc_mag, (unsigned long long)cyc_dir,
                    (unsigned long long)cyc_nms, (unsigned long long)cyc_thresh,
                    (unsigned long long)cyc_hyst);
            fprintf(cf, "%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n",
                    (unsigned long long)ns_gaussian, (unsigned long long)ns_sobel,
                    (unsigned long long)ns_mag, (unsigned long long)ns_dir,
                    (unsigned long long)ns_nms, (unsigned long long)ns_thresh,
                    (unsigned long long)ns_hyst);
            fclose(cf);
        }
    }

    free(img_in); free(img_blur); free(gx); free(gy); 
    free(img_mag); free(img_dir); free(img_nms); free(img_edges);

    return 0;
}