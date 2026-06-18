/**
 * @file main_rvv.cpp
 * @brief RVV pipeline entry point: benchmarks scalar vs RVV at VLEN=128/256/512.
 *
 * ## Purpose
 *
 * This file replaces `main.cpp` for the RVV build target.  It runs the full
 * 4-stage Edge Detection pipeline twice per image:
 *
 * 1. **Scalar path** (same functions as Phase 4/5 benchmarks).
 * 2. **RVV path** (rvv_gaussian + rvv_magnitude; other stages unchanged).
 *
 * After both runs it performs an **equivalence check**: every output pixel of
 * the RVV Gaussian and RVV magnitude is compared against the scalar baseline
 * with ±1 LSB tolerance (to allow integer rounding differences).
 *
 * The VLEN is set externally via the QEMU `-cpu rv64,v=true,vlen=N` flag —
 * the binary itself never reads or assumes a VLEN value.  Running this same
 * binary at VLEN=128, 256, and 512 is the VLEN sweep.
 *
 * ## Usage
 * @code
 * # build:
 * make rvv
 *
 * # VLEN sweep (run from Makefile or manually):
 * qemu-riscv64 -cpu rv64,v=true,vlen=128 ./build_rv/canny_rvv in.raw out128.raw 720 900
 * qemu-riscv64 -cpu rv64,v=true,vlen=256 ./build_rv/canny_rvv in.raw out256.raw 720 900
 * qemu-riscv64 -cpu rv64,v=true,vlen=512 ./build_rv/canny_rvv in.raw out512.raw 720 900
 * @endcode
 *
 * @author  Adham (The_GOAT branch) — Phase 6 RVV Optimization
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "rvv_gaussian.h"
#include "rvv_magnitude.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

// ─────────────────────────────────────────────────────────────────────────────
// Cycle counter (RISC-V rdcycle CSR)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Read the RISC-V hardware cycle counter.
 *
 * Uses the `rdcycle` pseudo-instruction which reads CSR 0xC00.
 * On QEMU this counts emulated instructions, not wall-clock cycles, but
 * relative comparisons between scalar and RVV are valid because instruction
 * count changes between implementations.
 *
 * @return 64-bit cycle count.
 */
static inline uint64_t read_cycles(void) {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
// Equivalence checker
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Compare two uint8_t buffers with ±1 tolerance per element.
 *
 * The ±1 tolerance accounts for integer division rounding differences between
 * the scalar (which uses `sum / 273`) and the RVV path (which also uses
 * `sum / 273` but may accumulate in a different order at strip boundaries).
 *
 * Reports the first mismatch found and the total number of mismatches.
 *
 * @param a       First buffer (typically scalar output).
 * @param b       Second buffer (typically RVV output).
 * @param size    Number of elements.
 * @param label   Human-readable name for error messages.
 * @return        1 if buffers match within tolerance, 0 if there are errors.
 */
static int check_equiv(const uint8_t* a, const uint8_t* b, int size, const char* label) {
    int errors = 0;
    for (int i = 0; i < size; i++) {
        int diff = (int)a[i] - (int)b[i];
        if (diff < -1 || diff > 1) {
            if (errors == 0)
                printf("[FAIL] %s: first mismatch at pixel %d: scalar=%d rvv=%d\n",
                       label, i, a[i], b[i]);
            errors++;
        }
    }
    if (errors == 0)
        printf("[PASS] %s: outputs match within ±1 tolerance (%d pixels checked)\n",
               label, size);
    else
        printf("[FAIL] %s: %d/%d pixels out of tolerance\n", label, errors, size);
    return (errors == 0) ? 1 : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timing table printer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Print a two-column comparison table: scalar cycles vs RVV cycles.
 *
 * Also computes and prints the speedup ratio for each vectorised stage.
 *
 * @param reps          Number of repetitions used to compute averages.
 * @param cyc_g_scalar  Scalar Gaussian average cycles.
 * @param cyc_g_rvv     RVV Gaussian average cycles.
 * @param cyc_m_scalar  Scalar Magnitude average cycles.
 * @param cyc_m_rvv     RVV Magnitude average cycles.
 */
static void print_comparison(int reps,
                             uint64_t cyc_g_scalar, uint64_t cyc_g_rvv,
                             uint64_t cyc_m_scalar, uint64_t cyc_m_rvv) {
    printf("\n+------------+----------------+----------------+----------+\n");
    printf("|            |   Scalar -O2   |   RVV          | Speedup  |\n");
    printf("|  Stage     | avg %d runs   | avg %d runs   |          |\n", reps, reps);
    printf("+------------+----------------+----------------+----------+\n");

    double sg = (cyc_g_rvv > 0) ? (double)cyc_g_scalar / cyc_g_rvv : 0.0;
    double sm = (cyc_m_rvv > 0) ? (double)cyc_m_scalar / cyc_m_rvv : 0.0;

    printf("| Gaussian   | %14llu | %14llu | %7.2fx |\n",
           (unsigned long long)cyc_g_scalar, (unsigned long long)cyc_g_rvv, sg);
    printf("| Magnitude  | %14llu | %14llu | %7.2fx |\n",
           (unsigned long long)cyc_m_scalar, (unsigned long long)cyc_m_rvv, sm);
    printf("+------------+----------------+----------------+----------+\n\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Entry point for the RVV benchmark binary.
 *
 * Expected arguments:
 * @code
 * ./canny_rvv <input.raw> <output.raw> <width> <height>
 * @endcode
 *
 * Runs:
 * 1. Scalar pipeline (100 reps) — measures per-stage baseline.
 * 2. RVV pipeline (100 reps)    — measures RVV Gaussian and Magnitude.
 * 3. Equivalence checks on blur output and magnitude output.
 * 4. Prints comparison table.
 * 5. Saves the RVV magnitude output to <output.raw>.
 *
 * @param argc  Argument count (must be ≥ 5).
 * @param argv  Argument vector: [binary, input.raw, output.raw, width, height].
 * @return      0 on success, 1 on error, 2 on equivalence failure.
 */
int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <input.raw> <output.raw> <width> <height>\n", argv[0]);
        return 1;
    }

    int W = atoi(argv[3]);
    int H = atoi(argv[4]);
    int size = W * H;

    printf("\n+----------------------------------------------+\n");
    printf("|     Canny RVV Benchmark — Phase 6            |\n");
    printf("|     RISC-V Vector Extension                  |\n");
    printf("+----------------------------------------------+\n");
    printf("|  Input : %-35s|\n", argv[1]);
    printf("|  Output: %-35s|\n", argv[2]);
    printf("|  Size  : %dx%d%-*s|\n",
           W, H, (int)(35 - 1 - snprintf(NULL, 0, "%dx%d", W, H)), "");
    printf("+----------------------------------------------+\n\n");

    // ── Allocate all buffers ─────────────────────────────────────────────────
    uint8_t* img_in       = allocate_buffer(W, H);   ///< Raw input image
    
    // Scalar pipeline intermediate buffers
    uint8_t* blur_scalar  = allocate_buffer(W, H);
    int16_t* gx           = (int16_t*)aligned_alloc(64, (size_t)size * sizeof(int16_t));
    int16_t* gy           = (int16_t*)aligned_alloc(64, (size_t)size * sizeof(int16_t));
    uint8_t* mag_scalar   = allocate_buffer(W, H);
    uint8_t* img_dir      = allocate_buffer(W, H);
    
    // RVV pipeline output buffers
    uint8_t* blur_rvv     = allocate_buffer(W, H);
    uint8_t* mag_rvv      = allocate_buffer(W, H);

    if (!img_in || !blur_scalar || !gx || !gy || !mag_scalar ||
        !img_dir || !blur_rvv || !mag_rvv) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        return 1;
    }

    if (!load_raw(argv[1], img_in, W, H)) return 1;

    const int REPS = 100;
    uint64_t c0;

    // ════════════════════════════════════════════════════════════════════════
    // SCALAR BASELINE (reuse the same -O2 functions from Phase 4/5)
    // ════════════════════════════════════════════════════════════════════════

    printf("--- Scalar baseline (100 reps) ---\n");

    c0 = read_cycles();
    for (int r = 0; r < REPS; r++)
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, blur_scalar, W, H);
    uint64_t cyc_g_scalar = (read_cycles() - c0) / REPS;

    // Run Sobel once (shared input for both paths)
    compute_sobel(blur_scalar, gx, gy, W, H);

    c0 = read_cycles();
    for (int r = 0; r < REPS; r++)
        compute_magnitude_l1(gx, gy, mag_scalar, W, H);
    uint64_t cyc_m_scalar = (read_cycles() - c0) / REPS;

    printf("  Gaussian  : %llu cycles/run\n", (unsigned long long)cyc_g_scalar);
    printf("  Magnitude : %llu cycles/run\n\n", (unsigned long long)cyc_m_scalar);

    // ════════════════════════════════════════════════════════════════════════
    // RVV PATH
    // ════════════════════════════════════════════════════════════════════════

    printf("--- RVV path (100 reps) ---\n");

    c0 = read_cycles();
    for (int r = 0; r < REPS; r++)
        gaussian_blur_rvv(img_in, blur_rvv, W, H);
    uint64_t cyc_g_rvv = (read_cycles() - c0) / REPS;

    // Sobel runs on the RVV blur output (uses the same gx/gy buffers — recompute)
    compute_sobel(blur_rvv, gx, gy, W, H);

    c0 = read_cycles();
    for (int r = 0; r < REPS; r++)
        compute_magnitude_l1_rvv(gx, gy, mag_rvv, W, H);
    uint64_t cyc_m_rvv = (read_cycles() - c0) / REPS;

    printf("  Gaussian  : %llu cycles/run\n", (unsigned long long)cyc_g_rvv);
    printf("  Magnitude : %llu cycles/run\n\n", (unsigned long long)cyc_m_rvv);

    // ════════════════════════════════════════════════════════════════════════
    // EQUIVALENCE CHECKS (±1 tolerance)
    // ════════════════════════════════════════════════════════════════════════

    printf("--- Equivalence checks ---\n");
    int ok_gauss = check_equiv(blur_scalar, blur_rvv,  size, "Gaussian");
    int ok_mag   = check_equiv(mag_scalar,  mag_rvv,   size, "Magnitude");
    printf("\n");

    // ════════════════════════════════════════════════════════════════════════
    // COMPARISON TABLE
    // ════════════════════════════════════════════════════════════════════════

    print_comparison(REPS, cyc_g_scalar, cyc_g_rvv, cyc_m_scalar, cyc_m_rvv);

    // ════════════════════════════════════════════════════════════════════════
    // COMPLETE PIPELINE (using RVV blur + RVV magnitude) → save output
    // ════════════════════════════════════════════════════════════════════════

    // Re-run Sobel on the RVV blur (already done above, but redo cleanly)
    compute_sobel(blur_rvv, gx, gy, W, H);
    compute_magnitude_l1_rvv(gx, gy, mag_rvv, W, H);
    compute_direction(gx, gy, img_dir, W, H);

    // Save the RVV Magnitude buffer so the output is visible!
    save_raw(argv[2], mag_rvv, W, H);
    printf("Magnitude output saved to %s\n\n", argv[2]);

    if (!ok_gauss || !ok_mag) {
        fprintf(stderr, "WARNING: Equivalence check FAILED — "
                        "RVV and scalar outputs differ beyond ±1 tolerance.\n");
    }

    // ── Free all buffers ─────────────────────────────────────────────────────
    free(img_in);   free(blur_scalar); free(gx);      free(gy);
    free(mag_scalar); free(img_dir);  
    free(blur_rvv); free(mag_rvv);

    return (!ok_gauss || !ok_mag) ? 2 : 0;
}