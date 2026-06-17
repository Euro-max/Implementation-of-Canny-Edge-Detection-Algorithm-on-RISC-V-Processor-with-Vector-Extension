/**
 * @file magnitude.cpp
 * @brief Implementation of gradient magnitude computation (L1 and L2 norms).
 * * This file contains the algorithms for calculating the total edge strength 
 * from horizontal and vertical gradients. Both implementations use a robust 
 * two-pass approach: the first pass computes the raw magnitudes and finds 
 * the global maximum, while the second pass normalizes all values to the 
 * standard 8-bit image range [0, 255] to prevent data loss or overflow.
 */

#include "magnitude.h"
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * @brief Computes the gradient magnitude using the L1 norm (Manhattan distance).
 * * Approximates the edge strength using the sum of absolute differences: |Gx| + |Gy|.
 * This method avoids floating-point square roots, making it significantly faster 
 * for embedded systems, while maintaining sufficient accuracy for edge detection.
 * The results are globally normalized to span the full 0-255 uint8_t range.
 * * @param gx     Pointer to the 16-bit input buffer containing horizontal gradients.
 * @param gy     Pointer to the 16-bit input buffer containing vertical gradients.
 * @param output Pointer to the 8-bit output buffer for the normalized magnitudes.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
void compute_magnitude_l1(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height) {
    int32_t max_mag = 0;
    int size = width * height;
    std::vector<int32_t> temp_mag(size);

    // Pass 1: Compute L1 magnitude and find max
    for (int i = 0; i < size; ++i) {
        temp_mag[i] = std::abs(gx[i]) + std::abs(gy[i]);
        if (temp_mag[i] > max_mag) max_mag = temp_mag[i];
    }

    // Pass 2: Normalize to [0, 255]
    float scale = (max_mag > 0) ? 255.0f / max_mag : 0.0f;
    for (int i = 0; i < size; ++i) {
        output[i] = (uint8_t)(temp_mag[i] * scale);
    }
}

/**
 * @brief Computes the gradient magnitude using the L2 norm (Euclidean distance).
 * * Calculates the exact geometric edge strength using the Pythagorean theorem: 
 * sqrt(Gx^2 + Gy^2). While this provides a highly accurate, isotropic magnitude 
 * response, it incurs a heavier performance penalty due to the floating-point 
 * square root operation. Results are globally normalized to [0, 255].
 * * @param gx     Pointer to the 16-bit input buffer containing horizontal gradients.
 * @param gy     Pointer to the 16-bit input buffer containing vertical gradients.
 * @param output Pointer to the 8-bit output buffer for the normalized magnitudes.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
void compute_magnitude_l2(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height) {
    int size = width * height;
    std::vector<float> temp_mag(size);
    float max_mag = 0.0f;

    // Pass 1: Compute L2 magnitude and find max
    for (int i = 0; i < size; ++i) {
        temp_mag[i] = sqrt((float)gx[i] * gx[i] + 
                          (float)gy[i] * gy[i]);
        if (temp_mag[i] > max_mag) max_mag = temp_mag[i];
    }

    // Pass 2: Normalize to [0, 255]
    float scale = (max_mag > 0) ? 255.0f / max_mag : 0.0f;
    for (int i = 0; i < size; ++i) {
        output[i] = (uint8_t)(temp_mag[i] * scale);
    }
}