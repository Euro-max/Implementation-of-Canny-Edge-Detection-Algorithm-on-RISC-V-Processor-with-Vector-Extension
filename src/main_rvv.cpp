/**
 * @file main_rvv.cpp
 * @brief RVV pipeline entry point: benchmarks scalar vs RVV at VLEN=128/256/512.
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "nms_threshold.h"
#include "rvv_gaussian.h"
#include "rvv_magnitude.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define REPS 100

// ─────────────────────────────────────────────────────────────────────────────
// Hardware Counters
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Stopwatch: Reads the RISC-V hardware cycle counter.
 * WARNING: On QEMU user-mode emulation, this measures host wall-clock time, 
 * which heavily penalizes software-simulated vector instructions.
 * True scaling is proven by comparing across different VLEN sweeps.
 */
static inline uint64_t read_cycles(void) {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c) :: "memory");
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
// Verification Helper
// ─────────────────────────────────────────────────────────────────────────────

static bool verify_equivalence(const uint8_t* scalar_out, const uint8_t* rvv_out, int size, const char* stage_name) {
    int errors = 0;
    int max_diff = 0;
    
    for (int i = 0; i < size; i++) {
        int diff = abs((int)scalar_out[i] - (int)rvv_out[i]);
        if (diff > max_diff) max_diff = diff;
        
        if (diff > 1) {
            if (errors < 5) {
                fprintf(stderr, "Mismatch at pixel %d: Scalar=%d, RVV=%d\n", 
                        i, scalar_out[i], rvv_out[i]);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("[PASS] %s: outputs match within ±1 tolerance (%d pixels checked)\n", stage_name, size);
        return true;
    } else {
        printf("[FAIL] %s: %d mismatches found! (Max diff: %d)\n", stage_name, errors, max_diff);
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pretty Print Table
// ─────────────────────────────────────────────────────────────────────────────

static void print_comparison(int reps, 
                             uint64_t cyc_g_s, uint64_t cyc_g_r, 
                             uint64_t cyc_m_s, uint64_t cyc_m_r) {
    
    // Calculate Averages
    uint64_t c_gs_avg = cyc_g_s / reps;
    uint64_t c_gr_avg = cyc_g_r / reps;
    uint64_t c_ms_avg = cyc_m_s / reps;
    uint64_t c_mr_avg = cyc_m_r / reps;

    // Calculate Apparent QEMU Speedups
    double g_cyc_speedup = (double)c_gs_avg / (double)c_gr_avg;
    double m_cyc_speedup = (double)c_ms_avg / (double)c_mr_avg;

    printf("\n+------------+----------------+----------------+----------+\n");
    printf("| Stage      | Scalar -Ofast  | RVV Pipeline   | Apparent |\n");
    printf("|            |  avg %3d runs  |  avg %3d runs  | Speedup  |\n", reps, reps);
    printf("+------------+----------------+----------------+----------+\n");
    printf("| Gaussian   | %14llu | %14llu |   %4.2fx |\n", 
           (unsigned long long)c_gs_avg, (unsigned long long)c_gr_avg, g_cyc_speedup);
    printf("| Magnitude  | %14llu | %14llu |   %4.2fx |\n", 
           (unsigned long long)c_ms_avg, (unsigned long long)c_mr_avg, m_cyc_speedup);
    printf("+------------+----------------+----------------+----------+\n");
    printf("  * Note: RVV speedup artifacts are expected under QEMU User-Mode.\n");
    printf("    Verify true VLA scaling via 'make vlen_sweep'.\n\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Pipeline
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <input.raw> <output.raw> <width> <height>\n", argv[0]);
        return 1;
    }

    const char* in_file = argv[1];
    int W = atoi(argv[3]);
    int H = atoi(argv[4]);
    int size = W * H;

    uint8_t* img_in      = (uint8_t*)malloc(size);
    uint8_t* blur_scalar = (uint8_t*)malloc(size);
    uint8_t* blur_rvv    = (uint8_t*)malloc(size);
    int16_t* gx          = (int16_t*)malloc(size * sizeof(int16_t));
    int16_t* gy          = (int16_t*)malloc(size * sizeof(int16_t));
    uint8_t* mag_scalar  = (uint8_t*)malloc(size);
    uint8_t* mag_rvv     = (uint8_t*)malloc(size);
    uint8_t* img_dir     = (uint8_t*)malloc(size);
    uint8_t* img_nms     = (uint8_t*)malloc(size);
    uint8_t* img_edges   = (uint8_t*)malloc(size);

    if (!img_in || !blur_scalar || !blur_rvv || !gx || !gy || !mag_scalar || !mag_rvv || !img_dir || !img_nms || !img_edges) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }

    load_raw(in_file, img_in, W, H);

    printf("\n+----------------------------------------------+\n");
    printf("|     Canny RVV Benchmark — Phase 6            |\n");
    printf("|     RISC-V Vector Extension                  |\n");
    printf("+----------------------------------------------+\n");
    printf("|  Input : %-35s |\n", in_file);
    printf("|  Output: %-35s |\n", argv[2]);
    printf("|  Size  : %dx%-30d |\n", W, H);
    printf("+----------------------------------------------+\n\n");

    uint64_t t0, t1;

    // ════════════════════════════════════════════════════════════════════════
    // 1. SCALAR BASELINE (The Control Group)
    // ════════════════════════════════════════════════════════════════════════

    uint64_t cyc_g_scalar = 0;
    for (int i = 0; i < REPS; i++) {
        t0 = read_cycles();
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, blur_scalar, W, H);
        asm volatile("": : :"memory"); // Compiler barrier
        t1 = read_cycles();
        cyc_g_scalar += (t1 - t0);
    }

    compute_sobel(blur_scalar, gx, gy, W, H);

    uint64_t cyc_m_scalar = 0;
    for (int i = 0; i < REPS; i++) {
        t0 = read_cycles();
        compute_magnitude_l1(gx, gy, mag_scalar, W, H);
        asm volatile("": : :"memory"); // Compiler barrier
        t1 = read_cycles();
        cyc_m_scalar += (t1 - t0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // 2. RVV IMPLEMENTATION (The Experimental Group)
    // ════════════════════════════════════════════════════════════════════════

    uint64_t cyc_g_rvv = 0;
    for (int i = 0; i < REPS; i++) {
        t0 = read_cycles();
        gaussian_blur_rvv(img_in, blur_rvv, W, H);
        asm volatile("": : :"memory"); // Compiler barrier
        t1 = read_cycles();
        cyc_g_rvv += (t1 - t0);
    }

    uint64_t cyc_m_rvv = 0;
    for (int i = 0; i < REPS; i++) {
        t0 = read_cycles();
        compute_magnitude_l1_rvv(gx, gy, mag_rvv, W, H);
        asm volatile("": : :"memory"); // Compiler barrier
        t1 = read_cycles();
        cyc_m_rvv += (t1 - t0);
    }

    // ════════════════════════════════════════════════════════════════════════
    // 3. EQUIVALENCE CHECK
    // ════════════════════════════════════════════════════════════════════════
    
    printf("--- Equivalence checks ---\n");
    bool ok_gauss = verify_equivalence(blur_scalar, blur_rvv, size, "Gaussian");
    bool ok_mag   = verify_equivalence(mag_scalar,  mag_rvv,   size, "Magnitude");

    // ════════════════════════════════════════════════════════════════════════
    // COMPARISON TABLE
    // ════════════════════════════════════════════════════════════════════════

    print_comparison(REPS, cyc_g_scalar, cyc_g_rvv, cyc_m_scalar, cyc_m_rvv);

    // ════════════════════════════════════════════════════════════════════════
    // COMPLETE PIPELINE (using RVV blur + RVV magnitude) → save output
    // ════════════════════════════════════════════════════════════════════════

    compute_sobel(blur_rvv, gx, gy, W, H);
    compute_magnitude_l1_rvv(gx, gy, mag_rvv, W, H);
    compute_direction(gx, gy, img_dir, W, H);
    apply_nms(mag_rvv, img_dir, img_nms, W, H);
    apply_double_threshold(img_nms, img_edges, W, H, 30, 90);
    apply_hysteresis(img_edges, W, H);

    save_raw(argv[2], img_edges, W, H);
    printf("Edge output saved to %s\n\n", argv[2]);

    if (!ok_gauss || !ok_mag) {
        fprintf(stderr, "WARNING: RVV implementation produced incorrect results.\n");
    }

    free(img_in); free(blur_scalar); free(blur_rvv);
    free(gx); free(gy);
    free(mag_scalar); free(mag_rvv);
    free(img_dir); free(img_nms); free(img_edges);

    return 0;
}
