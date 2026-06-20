/**
 * @file rvv_gaussian.h
 * @brief Public interface for the RVV-accelerated 5×5 Gaussian blur.
 *
 * Drop-in replacement for `gaussian_blur_5x5<uint8_t, uint8_t, int32_t>`.
 * Only compiled for the RISC-V target (requires `<riscv_vector.h>`).
 *
 * @author  Adham (The_GOAT branch) — Phase 6 RVV Optimization
 */

#ifndef RVV_GAUSSIAN_H
#define RVV_GAUSSIAN_H

#include <stdint.h>

/**
 * @brief RVV-accelerated 5×5 Gaussian blur.
 *
 * Semantically identical to the scalar baseline:
 * @code
 * gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src, dst, width, height);
 * @endcode
 *
 * Results may differ by ±1 LSB from the scalar version due to integer
 * division rounding — this is expected and acceptable (tested in equivalence
 * tests with ±1 tolerance).
 *
 * @param src    Read-only input image (width × height uint8_t, row-major).
 * @param dst    Output image buffer (same size, caller-allocated).
 * @param width  Image width in pixels.
 * @param height Image height in pixels.
 */
void gaussian_blur_rvv(const uint8_t* __restrict src, uint8_t* __restrict dst, int width, int height);

#endif /* RVV_GAUSSIAN_H */
