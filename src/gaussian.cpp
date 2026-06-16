#include "gaussian.h"
#include <stdint.h>

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

static const int KERNEL_SUM = 273;   // sum of all kernel values
static const int KERNEL_RADIUS = 2;  // kernel is 5x5, radius = (5-1)/2 = 2

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
