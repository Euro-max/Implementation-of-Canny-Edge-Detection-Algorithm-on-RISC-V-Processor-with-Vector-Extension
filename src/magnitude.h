/**
 * @file magnitude.h
 * @brief Gradient magnitude computation (L1 and L2 norms)
 * @ingroup canny
 * 
 * Provides both L1 (Manhattan) and L2 (Euclidean) gradient magnitude
 * computation functions. L1 is faster (no sqrt), L2 is more accurate.
 */

#ifndef MAGNITUDE_H
#define MAGNITUDE_H
#include <cstdint>

/**
 * @brief Computes L1 gradient magnitude with normalization
 * 
 * Magnitude = |Gx| + |Gy|
 * 
 * Advantages:
 * - No floating point operations
 * - Faster than L2 (especially on embedded platforms)
 * - Integer-only arithmetic
 * 
 * The function performs two passes:
 * 1. Compute magnitudes and track maximum
 * 2. Normalize values to [0, 255] range
 * 
 * @param gx      Horizontal Sobel gradients (int16_t)
 * @param gy      Vertical Sobel gradients (int16_t)
 * @param output  Output magnitude image (uint8_t, normalized 0-255)
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * 
 * @pre gx and gy computed via compute_sobel()
 * @pre output buffer allocated with size width×height
 * 
 * @see compute_sobel() For gradient computation
 * @see compute_magnitude_l2() For Euclidean norm
 */
void compute_magnitude_l1(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

/**
 * @brief Computes L2 (Euclidean) gradient magnitude with normalization
 * 
 * Magnitude = sqrt(Gx² + Gy²)
 * 
 * Advantages:
 * - More accurate magnitude (geometrically correct)
 * - Better edge detection in theory
 * 
 * Disadvantages:
 * - Uses floating point sqrt operations
 * - Slower than L1
 * 
 * @param gx      Horizontal Sobel gradients (int16_t)
 * @param gy      Vertical Sobel gradients (int16_t)
 * @param output  Output magnitude image (uint8_t, normalized 0-255)
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * 
 * @pre gx and gy computed via compute_sobel()
 * @pre output buffer allocated with size width×height
 * 
 * @see compute_sobel() For gradient computation
 * @see compute_magnitude_l1() For faster but less accurate norm
 */
void compute_magnitude_l2(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

#endif