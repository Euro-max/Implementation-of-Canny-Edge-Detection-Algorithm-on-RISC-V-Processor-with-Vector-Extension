/**
 * @file magnitude.h
 * @brief Declarations for gradient magnitude computation.
 * * This header defines the interfaces for calculating the total edge strength 
 * (magnitude) from the horizontal (Gx) and vertical (Gy) gradients. It provides 
 * both the L1 (Manhattan) and L2 (Euclidean) norm implementations, allowing 
 * for a trade-off between computational speed and mathematical accuracy.
 */

#ifndef MAGNITUDE_H
#define MAGNITUDE_H
#include <cstdint>

/**
 * @brief Computes the gradient magnitude using the L1 norm (Manhattan distance).
 * * Approximates the edge strength by summing the absolute values of the gradients. 
 * This method is highly computationally efficient for embedded and vector targets 
 * because it entirely avoids multiplications and floating-point square roots.
 * * @param gx     Pointer to the 16-bit input buffer containing horizontal gradients.
 * @param gy     Pointer to the 16-bit input buffer containing vertical gradients.
 * @param output Pointer to the 8-bit output buffer for the computed magnitudes [0, 255].
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
// L1 norm: |Gx| + |Gy| - faster, no floating point
void compute_magnitude_l1(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

/**
 * @brief Computes the gradient magnitude using the L2 norm (Euclidean distance).
 * * Calculates the exact geometric edge strength using the Pythagorean theorem. 
 * While more mathematically accurate and isotropic than the L1 norm, it requires 
 * more computational power due to the square and square root operations.
 * * @param gx     Pointer to the 16-bit input buffer containing horizontal gradients.
 * @param gy     Pointer to the 16-bit input buffer containing vertical gradients.
 * @param output Pointer to the 8-bit output buffer for the computed magnitudes [0, 255].
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
// L2 norm: sqrt(Gx^2 + Gy^2) - more accurate
void compute_magnitude_l2(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

#endif
