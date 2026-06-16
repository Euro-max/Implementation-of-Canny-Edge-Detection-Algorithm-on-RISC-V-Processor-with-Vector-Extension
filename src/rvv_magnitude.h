/**
 * @file rvv_magnitude.h
 * @brief Public interface for the RVV-accelerated L1 gradient magnitude.
 *
 * Drop-in replacement for `compute_magnitude_l1()` from magnitude.h/cpp.
 * Only compiled for the RISC-V target (requires `<riscv_vector.h>`).
 *
 * @author  Adham (The_GOAT branch) — Phase 6 RVV Optimization
 */

#ifndef RVV_MAGNITUDE_H
#define RVV_MAGNITUDE_H

#include <stdint.h>

/**
 * @brief RVV-accelerated L1 gradient magnitude with global normalisation.
 *
 * Two-pass algorithm:
 *  - Pass 1: `temp[i] = |Gx[i]| + |Gy[i]|`, track max.
 *  - Pass 2: `output[i] = (uint8_t)(temp[i] * 255 / max)`.
 *
 * Results match `compute_magnitude_l1` within ±1 LSB (integer vs float
 * rounding difference in the normalisation step).
 *
 * @param gx      Sobel X gradient buffer (int16_t, width×height elements).
 * @param gy      Sobel Y gradient buffer (int16_t, width×height elements).
 * @param output  Output image buffer (uint8_t, caller-allocated).
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
void compute_magnitude_l1_rvv(const int16_t* gx, const int16_t* gy,
                               uint8_t* output, int width, int height);

#endif /* RVV_MAGNITUDE_H */
