/**
 * @file rvv_gaussian.cpp
 * @brief RVV-accelerated 5×5 Gaussian blur for the Canny edge detection pipeline.
 *
 * ## Design Overview
 *
 * The scalar baseline (`gaussian_blur_5x5` in gaussian.ipp) processes one output
 * pixel at a time and has a boundary-check conditional inside the innermost loop,
 * which prevents any compiler auto-vectorization.
 *
 * This file replaces that with two regions:
 *
 * - **Interior region**: pixels where the full 5×5 kernel fits inside the image
 * (rows 2..H-3, cols 2..W-3). No boundary check is needed here, so the inner
 * column loop is fully strip-mined with RVV intrinsics.
 *
 * - **Border region**: the 2-pixel-wide border around the image. Handled with a
 * scalar fallback that does zero-padding exactly like the baseline.
 *
 * ## Data-widening chain
 *
 * uint8_t pixels  →  widen to int16_t  →  multiply by int16_t kernel coefficient
 * →  accumulate into int32_t  →  divide by 273  →  clamp  →  uint8_t output
 *
 * Widening doubles LMUL at each step, so with LMUL=m1 for the int16 load the
 * int32 accumulator is m2. This caps the practical LMUL at m2 (widened result
 * would be m4, which is still legal but leaves fewer registers for temporaries).
 *
 * @author  Adham (The_GOAT branch) — Phase 6 RVV Optimization
 */

#include "rvv_gaussian.h"
#include <riscv_vector.h>
#include <stdint.h>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Kernel constants (identical to scalar baseline)
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Flattened 5×5 Gaussian kernel coefficients (row-major). Sum = 273. */
static const int16_t KERNEL_FLAT[25] = {
     1,  4,  7,  4,  1,
     4, 16, 26, 16,  4,
     7, 26, 41, 26,  7,
     4, 16, 26, 16,  4,
     1,  4,  7,  4,  1
};

/** @brief Kernel normalisation divisor. */
static const int32_t KERNEL_SUM = 273;

/** @brief Half-width of the 5×5 kernel (= (5-1)/2). */
static const int RADIUS = 2;

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Scalar fallback for a single output pixel (zero-padding boundary).
 */
static inline uint8_t scalar_pixel(const uint8_t* __restrict src, int W, int H, int y, int x) {
    int32_t sum = 0;
    for (int ky = -RADIUS; ky <= RADIUS; ky++) {
        for (int kx = -RADIUS; kx <= RADIUS; kx++) {
            int iy = y + ky;
            int ix = x + kx;
            if (iy >= 0 && iy < H && ix >= 0 && ix < W) {
                sum += (int32_t)src[iy * W + ix]
                     * KERNEL_FLAT[(ky + RADIUS) * 5 + (kx + RADIUS)];
            }
        }
    }
    int32_t v = sum / KERNEL_SUM;
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

/**
 * @brief Process one interior row using RVV strip-mining.
 */
static void rvv_process_interior_row(const uint8_t* __restrict src, uint8_t* __restrict dst, int W, int y) {

    int n = W - 2 * RADIUS;          // = W - 4
    int x_start = RADIUS;            // first interior column (index 2)

    // Strip-mine across interior columns
    for (int x = 0; x < n; ) {

        size_t vl = __riscv_vsetvl_e8m1((size_t)(n - x));
        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(n - x));
        
        vint32m4_t vacc = __riscv_vmv_v_x_i32m4(0, vl32);

        // ── Kernel accumulation: 5 rows × 5 cols = 25 multiply-accumulate passes ──
        for (int ky = -RADIUS; ky <= RADIUS; ky++) {
            const uint8_t* src_row = src + (y + ky) * W + x_start + x;

            for (int kx = -RADIUS; kx <= RADIUS; kx++) {
                int16_t coeff = KERNEL_FLAT[(ky + RADIUS) * 5 + (kx + RADIUS)];

                // Load vl consecutive uint8_t pixels
                vuint8m1_t vpix = __riscv_vle8_v_u8m1(src_row + kx, vl);

                // Zero-extend uint8 to uint16
                vuint16m2_t vpix16 = __riscv_vwcvtu_x_x_v_u16m2(vpix, vl);

                // Reinterpret as signed for the multiply (pixel values 0-255 are safe)
                vint16m2_t vpix16s = __riscv_vreinterpret_v_u16m2_i16m2(vpix16);
                
                // Widening multiply-accumulate: vacc += vpix16s * coeff
                vacc = __riscv_vwmacc_vx_i32m4(vacc, coeff, vpix16s, vl);
            }
        }

        // ── Normalise: divide accumulator by 273, clamp, narrow to uint8 ──
        vl32 = __riscv_vsetvl_e32m4((size_t)(n - x));
        vint32m4_t vresult = __riscv_vdiv_vx_i32m4(vacc, KERNEL_SUM, vl32);

        vresult = __riscv_vmax_vx_i32m4(vresult, 0,   vl32);
        vresult = __riscv_vmin_vx_i32m4(vresult, 255, vl32);

        // Narrowing: i32→i16→u8
        vint16m2_t vnarrow16 = __riscv_vncvt_x_x_w_i16m2(vresult, vl32);
        vuint16m2_t vnarrow16u = __riscv_vreinterpret_v_i16m2_u16m2(vnarrow16);
        vuint8m1_t  vout8      = __riscv_vncvt_x_x_w_u8m1(vnarrow16u, vl);

        // Store results
        __riscv_vse8_v_u8m1(dst + y * W + x_start + x, vout8, vl);

        x += (int)vl;   // advance by however many elements we processed
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void gaussian_blur_rvv(const uint8_t* __restrict src, uint8_t* __restrict dst, int width, int height) {

    for (int y = 0; y < height; y++) {
        // ── Interior rows: use RVV for the interior columns ──────────────────
        if (y >= RADIUS && y < height - RADIUS) {
            // Border columns (left and right 2 pixels): scalar fallback
            for (int x = 0; x < RADIUS; x++)
                dst[y * width + x] = scalar_pixel(src, width, height, y, x);
            for (int x = width - RADIUS; x < width; x++)
                dst[y * width + x] = scalar_pixel(src, width, height, y, x);

            // Interior columns: RVV strip-mined
            rvv_process_interior_row(src, dst, width, y);
        } else {
            // ── Border rows: all columns use scalar fallback ─────────────────
            for (int x = 0; x < width; x++)
                dst[y * width + x] = scalar_pixel(src, width, height, y, x);
        }
    }
}