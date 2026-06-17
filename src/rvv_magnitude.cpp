/**
 * @file rvv_magnitude.cpp
 * @brief RVV-accelerated L1 gradient magnitude with two-pass normalisation.
 *
 * ## Algorithm recap (matching scalar baseline in magnitude.cpp)
 *
 *  **Pass 1** — for every pixel i:
 *    `temp[i] = |Gx[i]| + |Gy[i]|`   (int32_t, maximum value ≈ 2×32 767 = 65 534)
 *    Track the running maximum across the whole image.
 *
 *  **Pass 2** — normalise to [0, 255]:
 *    `output[i] = (uint8_t)(temp[i] * 255 / max_mag)`
 *
 * ## Why this is a good RVV candidate
 *
 *  - Pass 1 is purely element-wise on the SoA Gx/Gy arrays — a perfect fit for
 *    strip-mined vector abs + add.
 *  - The running max across the image requires a **vector reduction** at the end
 *    of each strip: `vredmax` folds a vector of N elements into a single scalar.
 *  - Pass 2 multiplies every temp value by the same scalar (255/max_mag) — another
 *    broadcast-scalar × vector pattern.
 *  - No boundary conditions at all — this is a flat 1-D loop over W×H elements.
 *
 * ## New RVV concept: vector reduction
 *
 *  `__riscv_vredmax_vs_i32m4_i32m1(vs2, vs1, vl)`:
 *  - `vs2` : the vector to reduce (m4 — LMUL=4, i.e. our accumulation register).
 *  - `vs1` : a scalar vector holding the initial value (identity for max = INT32_MIN).
 *  - Returns a LMUL=m1 vector whose element [0] holds the maximum.
 *  - Extract element [0] with `__riscv_vmv_x_s_i32m1_i32`.
 *
 * ## LMUL choice
 *
 *  Gx and Gy are int16_t.  After abs and add the result is int32_t (to avoid
 *  overflow: max |Gx|+|Gy| is 65 534, fits in int16_t actually, but we keep
 *  int32_t to match the scalar baseline and avoid signed-overflow UB).
 *
 *  For widening int16→int32 we use LMUL=m2 for the int16 load, giving m4
 *  for the int32 accumulation.  This lets us process more elements per
 *  iteration (at VLEN=256, m2 holds 32 int16 elements → 512 bits).
 *
 * @author  Adham (The_GOAT branch) — Phase 6 RVV Optimization
 */

#include "rvv_magnitude.h"
#include <riscv_vector.h>
#include <stdint.h>
#include <limits.h>

// ─────────────────────────────────────────────────────────────────────────────
// Pass 1: compute |Gx|+|Gy| and find the global maximum
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Compute L1 magnitude for all pixels and return the global maximum.
 *
 * Fills `temp_mag[i] = |gx[i]| + |gy[i]|` (int32_t) for all i in [0, size).
 * Returns the maximum value found (used by Pass 2 to compute the scale factor).
 *
 * ### Strip-mining structure
 *
 * Each strip processes `vl` pixels:
 *  1. Load `vl` × int16 from Gx  →  vuint16 abs  →  widen to int32.
 *  2. Load `vl` × int16 from Gy  →  vuint16 abs  →  widen to int32.
 *  3. Add the two int32 vectors.
 *  4. Store into temp_mag (int32_t).
 *  5. Reduce the strip's vector to find its max; accumulate into `global_max`.
 *
 * @param gx        Sobel X gradient buffer (int16_t SoA, size elements).
 * @param gy        Sobel Y gradient buffer (int16_t SoA, size elements).
 * @param temp_mag  Output buffer for unscaled magnitude (int32_t, caller-allocated).
 * @param size      Total number of pixels (width × height).
 * @return          Global maximum magnitude across all pixels.
 */
static int32_t rvv_pass1_magnitude(const int16_t* gx, const int16_t* gy,
                                   int32_t* temp_mag, int size) {

    /**
     * @note Scalar holding the running global maximum.
     *   We initialise it to 0 because magnitudes are always ≥ 0.
     */
    int32_t global_max = 0;

    /**
     * @note Initial value vector for the reduction.
     *   vredmax requires a "neutral element" vector (the identity for max).
     *   We broadcast 0 into a single-element m1 vector — used as the starting
     *   accumulator for the per-strip max reduction.
     */
    size_t vl1 = __riscv_vsetvl_e32m1(1);
    vint32m1_t vzero_scalar = __riscv_vmv_v_x_i32m1(0, vl1);

    for (int i = 0; i < size; ) {

        /**
         * @note vsetvl_e16m2:
         *   We want to load int16_t elements with LMUL=m2 so the subsequent
         *   widening (int16→int32, LMUL doubles) lands at m4 — a legal group.
         *   `vl` is the number of int16 elements processed this strip.
         *   At VLEN=128, m2: vl ≤ 16.  At VLEN=256, m2: vl ≤ 32. Etc.
         */
        size_t vl = __riscv_vsetvl_e16m2((size_t)(size - i));

        // ── Load Gx[i..i+vl-1] ──────────────────────────────────────────────
        /**
         * @note vle16_v_i16m2: loads vl signed 16-bit integers from gx+i.
         *   LMUL=m2 means each logical vector register group spans 2 physical
         *   registers, doubling capacity vs m1.
         */
        vint16m2_t vgx = __riscv_vle16_v_i16m2(gx + i, vl);

        // ── Absolute value of Gx ─────────────────────────────────────────────
        /**
         * @note There is no dedicated vabs intrinsic in RVV 1.0.
         *   The standard trick: abs(x) = max(x, -x).
         *   vneg_v negates all elements; vmax_vv picks the larger per element.
         */
        vint16m2_t vgx_neg = __riscv_vneg_v_i16m2(vgx, vl);
        vint16m2_t vgx_abs = __riscv_vmax_vv_i16m2(vgx, vgx_neg, vl);

        // ── Load Gy[i..i+vl-1] and compute |Gy| ─────────────────────────────
        vint16m2_t vgy     = __riscv_vle16_v_i16m2(gy + i, vl);
        vint16m2_t vgy_neg = __riscv_vneg_v_i16m2(vgy, vl);
        vint16m2_t vgy_abs = __riscv_vmax_vv_i16m2(vgy, vgy_neg, vl);

        // ── Widen |Gx| and |Gy| to int32, then add ──────────────────────────
        /**
         * @note vwadd_vv_i32m4:
         *   Widening add: (int32_t)vgx_abs[j] + (int32_t)vgy_abs[j].
         *   Inputs: i16m2.  Output: i32m4.  LMUL doubles as expected.
         *   Maximum possible value: 32767 + 32767 = 65534 — fits in int32_t.
         *
         *   We also need to set vl for the 32-bit operations:
         */
        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(size - i));
        vint32m4_t vmag = __riscv_vwadd_vv_i32m4(vgx_abs, vgy_abs, vl);

        // ── Store temp_mag[i..i+vl-1] ───────────────────────────────────────
        /**
         * @note vse32_v_i32m4: stores vl32 int32_t values at temp_mag+i.
         *   Note: vl32 == vl (same number of elements, different element width).
         */
        __riscv_vse32_v_i32m4(temp_mag + i, vmag, vl32);

        // ── Reduce this strip to find its maximum ────────────────────────────
        /**
         * @note vredmax_vs_i32m4_i32m1:
         *   Reduces `vmag` (m4 vector, vl32 elements) to a single maximum value,
         *   starting from the neutral element in vzero_scalar (= 0).
         *   Result is placed in element [0] of the returned m1 vector.
         *
         *   This is a TREE reduction inside the hardware — O(log vl) work,
         *   not O(vl).  Much faster than a scalar loop over the strip.
         */
        vint32m1_t vstrip_max = __riscv_vredmax_vs_i32m4_i32m1(vmag, vzero_scalar, vl32);

        /**
         * @note vmv_x_s_i32m1_i32:
         *   Extracts element [0] from the scalar result vector into a C int32_t.
         *   This is the only way to get the reduction result back into scalar land.
         */
        int32_t strip_max = __riscv_vmv_x_s_i32m1_i32(vstrip_max);

        if (strip_max > global_max) global_max = strip_max;

        i += (int)vl;
    }

    return global_max;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 2: normalise to [0, 255]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Normalise int32_t magnitudes to uint8_t [0, 255].
 *
 * For each pixel i:  `output[i] = (uint8_t)( temp_mag[i] * 255 / max_mag )`
 *
 * The scalar baseline uses a float scale factor.  We use integer arithmetic
 * here: `(temp_mag[i] * 255) / max_mag`.  Both give the same [0,255] result
 * for typical images; differences of ±1 are expected at the rounding boundary.
 *
 * ### Why we can't fuse Pass 1 and Pass 2 into one pass
 *
 * The scale factor is `255 / max_mag`, and `max_mag` is not known until all
 * pixels have been visited.  A single-pass approach would require storing
 * every pixel's magnitude in a buffer anyway (exactly what we do), then going
 * back to apply the scale.  The two-pass structure is unavoidable unless you
 * accept a pre-fixed maximum (which changes the output).
 *
 * @param temp_mag  Unscaled magnitude buffer (int32_t, size elements).
 * @param output    Output image buffer (uint8_t, size elements).
 * @param size      Total pixel count.
 * @param max_mag   Global maximum from Pass 1.
 */
static void rvv_pass2_normalise(const int32_t* temp_mag, uint8_t* output,
                                int size, int32_t max_mag) {

    if (max_mag == 0) {
        // Edge case: all-uniform image, all gradients are zero → all-black output
        for (int i = 0; i < size; i++) output[i] = 0;
        return;
    }

    for (int i = 0; i < size; ) {

        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(size - i));

        /**
         * @note vle32_v_i32m4: load vl32 int32_t magnitudes from temp_mag+i.
         */
        vint32m4_t vmag = __riscv_vle32_v_i32m4(temp_mag + i, vl32);

        /**
         * @note Normalise using integer arithmetic:
         *   result = (mag * 255) / max_mag
         *
         *   vmul_vx: multiply every element by the scalar 255.
         *   vdiv_vx: divide every element by the scalar max_mag.
         *
         *   This avoids floating point entirely — important for deterministic
         *   cross-VLEN equivalence (float rounding can differ across VLEN).
         *
         *   Overflow check: max(mag) * 255 = 65534 * 255 = 16 711 170,
         *   which fits comfortably in int32_t (max ≈ 2.1 × 10⁹).
         */
        vint32m4_t vscaled = __riscv_vmul_vx_i32m4(vmag,  255,     vl32);
        vint32m4_t vnorm   = __riscv_vdiv_vx_i32m4(vscaled, max_mag, vl32);

        // Clamp to [0, 255] (should be unnecessary after correct division, but defensive)
        vnorm = __riscv_vmax_vx_i32m4(vnorm,   0, vl32);
        vnorm = __riscv_vmin_vx_i32m4(vnorm, 255, vl32);

        // ── Narrow i32m4 → i16m2 → u8m1 ────────────────────────────────────
        /**
         * @note Two-step narrowing (same as Gaussian):
         *   vncvt i32m4 → i16m2 (each element halved in bit width, LMUL halves).
         *   vncvt i16m2 → u8m1  (another halving).
         *   Need to reset vl for each width step.
         */
        size_t vl16 = __riscv_vsetvl_e16m2((size_t)(size - i));
        vint16m2_t vn16  = __riscv_vncvt_x_x_w_i16m2(vnorm, vl32);

        size_t vl8  = __riscv_vsetvl_e8m1((size_t)(size - i));
        vuint16m2_t vn16u = __riscv_vreinterpret_v_i16m2_u16m2(vn16);
        vuint8m1_t  vout8 = __riscv_vncvt_x_x_w_u8m1(vn16u, vl16);

        /**
         * @note vse8_v_u8m1: store vl8 bytes into the output buffer.
         */
        __riscv_vse8_v_u8m1(output + i, vout8, vl8);

        i += (int)vl8;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief RVV-accelerated L1 gradient magnitude with normalisation.
 *
 * Drop-in replacement for `compute_magnitude_l1()` from magnitude.cpp.
 * Allocates a temporary int32_t buffer on the stack via VLA (or the caller
 * could pre-allocate it; here we use a static local to avoid heap allocation
 * in the timed hot path — if image size is fixed this is safe).
 *
 * @warning The temporary buffer is heap-allocated here to support arbitrary
 *          image sizes.  For a fixed-size embedded deployment, a static array
 *          would save the malloc overhead.
 *
 * ### Two-pass structure
 *
 *  - **Pass 1** (`rvv_pass1_magnitude`): fill temp_mag[], return global max.
 *  - **Pass 2** (`rvv_pass2_normalise`):  normalise temp_mag[] → output[].
 *
 * ### Equivalence guarantee
 *
 *  Results match `compute_magnitude_l1` within ±1 LSB.  The scalar baseline
 *  uses `float scale = 255.0f / max_mag` and then `(uint8_t)(temp[i] * scale)`,
 *  which can differ from integer division by ±1 at rounding boundaries.
 *
 * @param gx      Sobel X gradient (int16_t SoA, width×height elements).
 * @param gy      Sobel Y gradient (int16_t SoA, width×height elements).
 * @param output  Output magnitude image (uint8_t, caller-allocated).
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
void compute_magnitude_l1_rvv(const int16_t* gx, const int16_t* gy,
                               uint8_t* output, int width, int height) {

    int size = width * height;

    // Temporary buffer: unscaled int32_t magnitudes (Pass 1 → Pass 2 handoff)
    int32_t* temp_mag = (int32_t*)__builtin_alloca_with_align(
        (size_t)size * sizeof(int32_t), 64);

    // Pass 1: fill temp_mag and find global max
    int32_t max_mag = rvv_pass1_magnitude(gx, gy, temp_mag, size);

    // Pass 2: normalise to [0, 255] and write output
    rvv_pass2_normalise(temp_mag, output, size, max_mag);
}
