/**
 * @file sobel.h
 * @brief Sobel edge detection operator implementation
 * @ingroup canny
 * 
 * Provides Sobel gradient computation for Canny edge detection.
 * Uses 3x3 kernels to approximate horizontal and vertical derivatives.
 */

#ifndef SOBEL_H
#define SOBEL_H

#include <cstdint>

/**
 * @brief Computes Sobel gradients (Gx and Gy) for edge detection
 * 
 * Sobel kernels:
 * - Gx (horizontal): [[-1,0,1], [-2,0,2], [-1,0,1]]
 * - Gy (vertical):   [[-1,-2,-1], [0,0,0], [1,2,1]]
 * 
 * Output stored as int16_t, which is sufficient for 8-bit input:
 * Maximum gradient magnitude ≈ 4 * 255 = 1020
 * int16_t range: -32768 to 32767 → no overflow
 * 
 * @param input   Input grayscale image (uint8_t)
 * @param gx      Output buffer for horizontal gradients (int16_t)
 * @param gy      Output buffer for vertical gradients (int16_t)
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * 
 * @pre input buffer contains valid image data (e.g., after Gaussian blur)
 * @pre gx and gy buffers allocated with size width×height × 2 bytes
 * @pre All buffers should be 64-byte aligned for SIMD/RVV optimization
 * 
 * @note Boundary pixels use zero-padding for missing neighbors
 * @note Uses Structure of Arrays (SoA) layout for better vectorization
 * 
 * @see gaussian_blur_5x5() For pre-smoothing before Sobel
 * @see compute_magnitude_l1() For gradient magnitude from Gx/Gy
 * @see compute_direction() For edge orientation from Gx/Gy
 */
void compute_sobel(const uint8_t* input, int16_t* gx, int16_t* gy, int width, int height);

#endif