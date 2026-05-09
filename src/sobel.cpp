/**
 * @file sobel.cpp
 * @brief Implementation of Sobel gradient computation
 * @ingroup canny
 * 
 * Implements 3x3 Sobel convolution with zero-padding boundary handling.
 */

#include "sobel.h"

/**
 * @brief Convolves image with Sobel kernels to compute gradients
 * 
 * Implementation details:
 * - Uses 3x3 separable kernels (but implemented as full convolution)
 * - Zero-padding for boundary pixels (outside pixels treated as 0)
 * - Results stored as int16_t (safe for 8-bit input range)
 * - Memory layout: separate Gx and Gy arrays (SoA format)
 * 
 * Complexity: O(9 × width × height) operations
 * 
 * @param input   Source grayscale image
 * @param gx      Destination for horizontal gradients
 * @param gy      Destination for vertical gradients
 * @param width   Image width
 * @param height  Image height
 */
void compute_sobel(const uint8_t* input, int16_t* gx, int16_t* gy, int width, int height) {
    const int8_t Kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    const int8_t Ky[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int32_t sumX = 0, sumY = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    int iy = y + ky;
                    int ix = x + kx;
                    if (iy >= 0 && iy < height && ix >= 0 && ix < width) {
                        uint8_t pixel = input[iy * width + ix];
                        sumX += pixel * Kx[ky + 1][kx + 1];
                        sumY += pixel * Ky[ky + 1][kx + 1];
                    }
                }
            }
            gx[y * width + x] = (int16_t)sumX; // Store in SoA [cite: 78]
            gy[y * width + x] = (int16_t)sumY;
        }
    }
}