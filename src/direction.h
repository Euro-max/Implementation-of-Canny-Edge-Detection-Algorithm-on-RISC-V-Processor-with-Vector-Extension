/**
 * @file direction.h
 * @brief Gradient direction quantization into 4 bins
 * @ingroup canny
 * 
 * Provides function to compute gradient direction from Sobel derivatives
 * and quantize into one of four discrete orientations (0°, 45°, 90°, 135°).
 * This is a key step in the Canny edge detection pipeline after gradient
 * magnitude computation.
 */

#ifndef DIRECTION_H
#define DIRECTION_H

#include <cstdint>

/**
 * @brief Quantizes gradient direction into 4 bins for non-maximum suppression
 * 
 * The function uses integer cross-multiplication to avoid expensive trigonometry.
 * Approximation thresholds: tan(22.5°) ≈ 2/5, tan(67.5°) ≈ 12/5.
 * 
 * Direction bins:
 * - 0: 0°   (horizontal gradient → vertical edge)
 * - 1: 45°  (diagonal edge, Gx and Gy have same sign)
 * - 2: 90°  (vertical gradient → horizontal edge)
 * - 3: 135° (diagonal edge, Gx and Gy have opposite signs)
 * 
 * @param gx      Input array of horizontal Sobel gradients (int16_t)
 * @param gy      Input array of vertical Sobel gradients (int16_t)
 * @param output  Output array of quantized directions (uint8_t, values 0-3)
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * 
 * @pre gx, gy, and output must be allocated with size width*height
 * @pre All pointers must be 64-byte aligned for SIMD/RVV optimization
 * 
 * @see compute_sobel() For gradient computation
 * @see compute_magnitude_l1() For gradient magnitude
 */
void compute_direction(const int16_t* gx, const int16_t* gy, uint8_t* output, int width, int height);

#endif