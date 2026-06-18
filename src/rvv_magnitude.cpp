/**
 * @file rvv_magnitude.cpp
 * @brief RVV-accelerated L1 gradient magnitude with two-pass normalisation.
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

static int32_t rvv_pass1_magnitude(const int16_t* __restrict gx, const int16_t* __restrict gy,
                                   int32_t* __restrict temp_mag, int size) {

    int32_t global_max = 0;
    
    // Initial value vector for the reduction (identity for max = 0)
    size_t vl1 = __riscv_vsetvl_e32m1(1);
    vint32m1_t vzero_scalar = __riscv_vmv_v_x_i32m1(0, vl1);

    for (int i = 0; i < size; ) {
        size_t vl = __riscv_vsetvl_e16m2((size_t)(size - i));

        // ── Load Gx and compute |Gx| ────────────────────────────────────────
        vint16m2_t vgx = __riscv_vle16_v_i16m2(gx + i, vl);
        vint16m2_t vgx_neg = __riscv_vneg_v_i16m2(vgx, vl);
        vint16m2_t vgx_abs = __riscv_vmax_vv_i16m2(vgx, vgx_neg, vl);

        // ── Load Gy and compute |Gy| ────────────────────────────────────────
        vint16m2_t vgy     = __riscv_vle16_v_i16m2(gy + i, vl);
        vint16m2_t vgy_neg = __riscv_vneg_v_i16m2(vgy, vl);
        vint16m2_t vgy_abs = __riscv_vmax_vv_i16m2(vgy, vgy_neg, vl);

        // ── Widen |Gx| and |Gy| to int32, then add ──────────────────────────
        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(size - i));
        vint32m4_t vmag = __riscv_vwadd_vv_i32m4(vgx_abs, vgy_abs, vl);

        // ── Store temp_mag ──────────────────────────────────────────────────
        __riscv_vse32_v_i32m4(temp_mag + i, vmag, vl32);

        // ── Reduce this strip to find its maximum ───────────────────────────
        vint32m1_t vstrip_max = __riscv_vredmax_vs_i32m4_i32m1(vmag, vzero_scalar, vl32);
        int32_t strip_max = __riscv_vmv_x_s_i32m1_i32(vstrip_max);

        if (strip_max > global_max) global_max = strip_max;

        i += (int)vl;
    }

    return global_max;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 2: normalise to [0, 255] using Fixed-Point Math
// ─────────────────────────────────────────────────────────────────────────────

static void rvv_pass2_normalise(const int32_t* __restrict temp_mag, uint8_t* __restrict output,
                                int size, int32_t max_mag) {

    if (max_mag == 0) {
        for (int i = 0; i < size; i++) output[i] = 0;
        return;
    }

    // Precompute scale factor for fixed point math to completely avoid vector division.
    // Equivalent to (val * 255) / max_mag  ==>  (val * scale) >> 16
    int32_t scale = (255 << 16) / max_mag;

    for (int i = 0; i < size; ) {
        size_t vl32 = __riscv_vsetvl_e32m4((size_t)(size - i));
        vint32m4_t vmag = __riscv_vle32_v_i32m4(temp_mag + i, vl32);

        // Fast Fixed-Point Normalization: Multiply then arithmetic shift right
        vint32m4_t vscaled = __riscv_vmul_vx_i32m4(vmag, scale, vl32);
        vint32m4_t vnorm   = __riscv_vsra_vx_i32m4(vscaled, 16, vl32);

        // Clamp
        vnorm = __riscv_vmax_vx_i32m4(vnorm,   0, vl32);
        vnorm = __riscv_vmin_vx_i32m4(vnorm, 255, vl32);

        // ── Narrow i32m4 → i16m2 → u8m1 ────────────────────────────────────
        size_t vl16 = __riscv_vsetvl_e16m2((size_t)(size - i));
        vint16m2_t vn16  = __riscv_vncvt_x_x_w_i16m2(vnorm, vl32);

        size_t vl8  = __riscv_vsetvl_e8m1((size_t)(size - i));
        vuint16m2_t vn16u = __riscv_vreinterpret_v_i16m2_u16m2(vn16);
        vuint8m1_t  vout8 = __riscv_vncvt_x_x_w_u8m1(vn16u, vl16);

        __riscv_vse8_v_u8m1(output + i, vout8, vl8);

        i += (int)vl8;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void compute_magnitude_l1_rvv(const int16_t* __restrict gx, const int16_t* __restrict gy,
                               uint8_t* __restrict output, int width, int height) {
    int size = width * height;
    
    // Temporary buffer for unscaled magnitudes
    int32_t* temp_mag = (int32_t*)__builtin_alloca_with_align(
        (size_t)size * sizeof(int32_t), 64);

    // Pass 1
    int32_t max_mag = rvv_pass1_magnitude(gx, gy, temp_mag, size);
    
    // Pass 2
    rvv_pass2_normalise(temp_mag, output, size, max_mag);
}
