/**
 * @file gaussian.cpp
 * @brief Implementation of a 5x5 Gaussian blur filter.
 * * This file contains the scalar implementation for applying a 5x5 discrete 
 * Gaussian blur convolution to an 8-bit grayscale image. It utilizes integer 
 * approximations for the kernel coefficients to optimize performance while 
 * preventing integer overflow during pixel accumulation.
 */

#include "gaussian.h"
#include <stdint.h>

/**
 * @brief The fixed 5x5 Gaussian kernel coefficients.
 * * These represent an integer approximation of a Gaussian distribution 
 * with a standard deviation (sigma) of approximately 1.0.
 */
// The 5x5 Gaussian kernel coefficients
// These are integer approximations of a Gaussian with sigma ≈ 1.0
// They sum to 273
static const int KERNEL[5][5] = {
    { 1,  4,  7,  4,  1},
    { 4, 16, 26, 16,  4},
    { 7, 26, 41, 26,  7},
    { 4, 16, 26, 16,  4},
    { 1,  4,  7,  4,  1}
};

/**
 * @brief Normalization factor representing the sum of all kernel weights.
 */
static const int KERNEL_SUM = 273;   // sum of all kernel values

/**
 * @brief The spatial radius of the convolution kernel.
 */
static const int KERNEL_RADIUS = 2;  // kernel is 5x5, radius = (5-1)/2 = 2

/**
 * @brief Applies a 5x5 Gaussian blur to an 8-bit grayscale image.
 * * Convolves the input image with the fixed 5x5 Gaussian kernel. Boundary 
 * pixels are handled implicitly via zero-padding (out-of-bounds coordinates 
 * are ignored during the accumulation phase). The final sum is normalized 
 * by dividing by the sum of the kernel weights (273) and clamped to the 
 * valid 8-bit range [0, 255].
 * * @param src    Pointer to the 8-bit grayscale input image buffer.
 * @param dst    Pointer to the 8-bit output image buffer.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
void gaussian_blur(const uint8_t* src, uint8_t* dst,
                   int width, int height) {

    // Loop over every output pixel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // Accumulator: sum of (pixel × kernel coefficient)
            // Must be int32_t — can reach 255 × 41 × 25 ≈ 261,000
            // which overflows int16_t (max 32,767)!
            int32_t sum = 0;

            // Loop over kernel rows (ky = -2, -1, 0, 1, 2)
            for (int ky = -KERNEL_RADIUS; ky <= KERNEL_RADIUS; ky++) {
                for (int kx = -KERNEL_RADIUS; kx <= KERNEL_RADIUS; kx++) {

                    // Image coordinates of the pixel under this kernel cell
                    int img_y = y + ky;
                    int img_x = x + kx;

                    // Zero-padding: if outside image, treat as 0
                    // (multiplying by 0 adds nothing to sum, so just skip)
                    if (img_y < 0 || img_y >= height ||
                        img_x < 0 || img_x >= width) {
                        continue;   // skip — zero padding means add 0
                    }

                    // Get the pixel value
                    uint8_t pixel = src[img_y * width + img_x];

                    // Get the kernel weight for this position
                    // ky+2 and kx+2 converts (-2..2) range to (0..4) index
                    int weight = KERNEL[ky + KERNEL_RADIUS][kx + KERNEL_RADIUS];

                    // Multiply and accumulate
                    sum += pixel * weight;
                }
            }

            // Divide by kernel sum to normalize back to 0-255 range
            int32_t result = sum / KERNEL_SUM;

            // Clamp to [0, 255] just in case of rounding edge cases
            if (result < 0)   result = 0;
            if (result > 255) result = 255;

            // Store result in output image
            dst[y * width + x] = (uint8_t)result;
        }
    }
}