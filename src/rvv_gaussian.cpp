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
 *  - **Interior region**: pixels where the full 5×5 kernel fits inside the image
 *    (rows 2..H-3, cols 2..W-3). No boundary check is needed here, so the inner
 *    column loop is fully strip-mined with RVV intrinsics.
 *
 *  - **Border region**: the 2-pixel-wide border around the image. Handled with a
 *    scalar fallback that does zero-padding exactly like the baseline.
 *
 * ## Data-widening chain
 *
 *  uint8_t pixels  →  widen to int16_t  →  multiply by int16_t kernel coefficient
 *  →  accumulate into int32_t  →  divide by 273  →  clamp  →  uint8_t output
 *
 *  Widening doubles LMUL at each step, so with LMUL=m1 for the int16 load the
 *  int32 accumulator is m2. This caps the practical LMUL at m2 (widened result
 *  would be m4, which is still legal but leaves fewer registers for temporaries).
 *
 * ## Strip-mining
 *
 *  The canonical RVV strip-mining pattern:
 *  @code
 *      for (int i = 0; i < n; ) {
 *          size_t vl = __riscv_vsetvl_e8m1(n - i);   // ask HW: how many fit?
 *          // ... process vl elements ...
 *          i += vl;
 *      }
 *  @endcode
 *  `vl` is determined at runtime by the hardware based on VLEN and LMUL.
 *  At VLEN=128 with LMUL=m1: vl = 16 bytes.
 *  At VLEN=256 with LMUL=m1: vl = 32 bytes.
 *  At VLEN=512 with LMUL=m1: vl = 64 bytes.
 *  The same binary, the same source — different throughput on different hardware.
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
 *
 * Used for the 2-pixel-wide border where the kernel extends outside the image.
 * Mirrors the logic in gaussian.ipp exactly so outputs match.
 *
 * @param src    Input image buffer (row-major, uint8_t).
 * @param W      Image width in pixels.
 * @param H      Image height in pixels.
 * @param y      Output row index.
 * @param x      Output column index.
 * @return       Blurred pixel value clamped to [0, 255].
 */
static inline uint8_t scalar_pixel(const uint8_t* src, int W, int H, int y, int x) {
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
 *
 * "Interior" means y is in [RADIUS, H-RADIUS-1] so every kernel row
 * (y-2 .. y+2) is a valid image row — no boundary check needed.
 * Within each such row, x runs over [RADIUS, W-RADIUS-1] so that the
 * kernel columns also never go out-of-bounds.
 *
 * ### Why the outer loop (ky, kx) is scalar
 *
 * Each (ky, kx) pair contributes `pixel[row][x + kx] * coeff` to every
 * output pixel in the current row.  The pixels form a contiguous vector
 * in memory (just offset by kx from the output index), so we load them
 * with a single `vle8` and multiply by the scalar coefficient.  The 25
 * multiply-accumulate operations per output pixel are spread across 25
 * passes of the inner strip-mining loop — a classic "broadcast scalar,
 * vector-load pixels, vector-MAC" pattern.
 *
 * ### LMUL choices and the widening chain
 *
 *  - `vle8_v_u8m1`   : loads  vl × uint8  into a LMUL=m1 register group.
 *  - `vwcvtu_x`      : widens uint8→uint16, result is LMUL=m2 (widening doubles).
 *  - `vwmacc_vx`     : widens uint16→int32 and accumulates, result is LMUL=m4.
 *
 *  So the accumulator `vacc` is declared as `vint32m4_t`.  With LMUL=m4 we
 *  have 8 logical 32-bit register groups available — enough for the 25
 *  temporaries we need (load → u16 → i32 happens in sequence, not all at once).
 *
 * @param src     Input image.
 * @param dst     Output image.
 * @param W       Image width.
 * @param y       Row index (must satisfy RADIUS ≤ y < H-RADIUS).
 */
static void rvv_process_interior_row(const uint8_t* src, uint8_t* dst, int W, int y) {

    // Number of interior columns to process
    int n = W - 2 * RADIUS;          // = W - 4
    int x_start = RADIUS;            // first interior column (index 2)

    // Strip-mine across interior columns
    for (int x = 0; x < n; ) {

        /**
         * @note vsetvl call:
         *   __riscv_vsetvl_e8m1(n - x)
         *   - e8   : element width = 8 bits (we load uint8_t pixels)
         *   - m1   : LMUL = 1 (one register group per vector variable at this width)
         *   - Returns vl: the number of elements the hardware can process this iteration.
         *     At VLEN=128: vl ≤ 16.  At VLEN=256: vl ≤ 32.  At VLEN=512: vl ≤ 64.
         *     On the last iteration vl = remaining elements (tail case).
         *     This is the strip-mining tail — the same code handles it automatically.
         */
        size_t vl = __riscv_vsetvl_e8m1((size_t)(n - x));

        /**
         * @note Accumulator initialisation:
         *   vint32m4_t vacc — LMUL=m4 because the widening chain will land here:
         *     uint8 (m1) → uint16 (m2, after vwcvtu) → int32 (m4, after vwmacc).
         *   We zero it with vmv_v_x (broadcast scalar 0 to all vl elements).
         *   The vl here must match the e32m4 element count, so we call vsetvl again.
         */
        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(n - x));
        vint32m4_t vacc = __riscv_vmv_v_x_i32m4(0, vl32);

        // ── Kernel accumulation: 5 rows × 5 cols = 25 multiply-accumulate passes ──
        for (int ky = -RADIUS; ky <= RADIUS; ky++) {
            const uint8_t* src_row = src + (y + ky) * W + x_start + x;

            for (int kx = -RADIUS; kx <= RADIUS; kx++) {
                // Scalar kernel coefficient for this (ky, kx) position
                int16_t coeff = KERNEL_FLAT[(ky + RADIUS) * 5 + (kx + RADIUS)];

                /**
                 * @note vle8_v_u8m1:
                 *   Loads `vl` consecutive uint8_t pixels starting at (src_row + kx).
                 *   Because src_row already points to (y+ky, x_start+x), adding kx
                 *   gives us the correct kernel column offset.
                 *   Result type: vuint8m1_t (LMUL=m1, element=u8).
                 */
                vuint8m1_t vpix = __riscv_vle8_v_u8m1(src_row + kx, vl);

                /**
                 * @note vwcvtu_x_x_v_u16m2:
                 *   Zero-extends uint8→uint16.  LMUL doubles: m1→m2.
                 *   We need uint16 before multiplying because coeff is int16_t and
                 *   the product fits in 32 bits (max: 255 × 41 = 10 455).
                 */
                vuint16m2_t vpix16 = __riscv_vwcvtu_x_x_v_u16m2(vpix, vl);

                /**
                 * @note vwmacc_vx_i32m4:
                 *   Widening multiply-accumulate: vacc += (int32_t)vpix16 * coeff.
                 *   "Widening" means inputs are i16/u16 but the result is i32.
                 *   LMUL of inputs is m2; result (and vacc) is m4 — doubles again.
                 *   We reinterpret vpix16 as signed for the multiply (pixel values
                 *   0-255 are safe — no sign-extension issue).
                 */
                vint16m2_t vpix16s = __riscv_vreinterpret_v_u16m2_i16m2(vpix16);
                vacc = __riscv_vwmacc_vx_i32m4(vacc, coeff, vpix16s, vl);
            }
        }

        // ── Normalise: divide accumulator by 273, clamp, narrow to uint8 ──

        /**
         * @note vdiv_vx_i32m4:
         *   Element-wise integer divide: vacc[i] / 273.
         *   Integer division is slow on most hardware — the hints guide suggests
         *   replacing with (vacc * 240) >> 16 as an approximation of /273.
         *   We use true division here for correctness; see comments below for the
         *   fixed-point alternative.
         */
        vl32 = __riscv_vsetvl_e32m4((size_t)(n - x));
        vint32m4_t vresult = __riscv_vdiv_vx_i32m4(vacc, KERNEL_SUM, vl32);

        /**
         * @note Clamp to [0, 255] using vmax/vmin with scalar broadcast:
         *   vmax(result, 0) → vmin(result, 255).
         *   This avoids a conditional branch per element.
         */
        vresult = __riscv_vmax_vx_i32m4(vresult, 0,   vl32);
        vresult = __riscv_vmin_vx_i32m4(vresult, 255, vl32);

        /**
         * @note Narrowing: i32→i16→u8.
         *   vncvt_x: narrows by half each time (i32m4→i16m2, then i16m2→u8m1).
         *   After clamping, all values are in [0,255] so no overflow.
         */
        vint16m2_t vnarrow16 = __riscv_vncvt_x_x_w_i16m2(vresult, vl32);

        // Reinterpret signed i16 as unsigned before second narrow
        vuint16m2_t vnarrow16u = __riscv_vreinterpret_v_i16m2_u16m2(vnarrow16);
        vuint8m1_t  vout8      = __riscv_vncvt_x_x_w_u8m1(vnarrow16u, vl);

        /**
         * @note vse8_v_u8m1:
         *   Stores `vl` uint8_t results into the output row at (y, x_start+x).
         */
        __riscv_vse8_v_u8m1(dst + y * W + x_start + x, vout8, vl);

        x += (int)vl;   // advance by however many elements we processed
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief RVV-accelerated 5×5 Gaussian blur.
 *
 * Produces output byte-for-byte identical to `gaussian_blur_5x5<uint8_t,uint8_t,int32_t>`
 * from the scalar baseline (within ±1 due to integer division rounding).
 *
 * ### Correctness guarantee
 * - Border pixels (row < 2, row ≥ H-2, col < 2, col ≥ W-2): handled by
 *   `scalar_pixel()` which is a direct copy of the baseline logic.
 * - Interior pixels: handled by `rvv_process_interior_row()` which accumulates
 *   with the same coefficients and divides by 273.
 *
 * ### VLEN-agnosticism
 * No VLEN value is hardcoded anywhere.  `vsetvl` asks the hardware at runtime.
 * Running this binary at VLEN=128, 256, or 512 must produce identical output.
 *
 * @param src    Input image (width × height bytes, row-major, uint8_t).
 * @param dst    Output image (same dimensions, caller-allocated).
 * @param width  Image width in pixels.
 * @param height Image height in pixels.
 */
void gaussian_blur_rvv(const uint8_t* src, uint8_t* dst, int width, int height) {

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
