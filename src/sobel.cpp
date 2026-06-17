/**
 * @file sobel.cpp
 * @brief Implementation of the Sobel edge detection operator.
 * * This file contains the scalar implementation of the 3x3 Sobel filter,
 * which computes the spatial gradient of an image to identify regions 
 * of high spatial frequency (edges).
 */

#include "sobel.h"

/**
 * @brief Computes the horizontal and vertical gradients of an image using Sobel operators.
 * * Applies the 3x3 Sobel kernels (Kx and Ky) across the input image using a 
 * standard 2D convolution. Boundary pixels are handled implicitly by skipping 
 * out-of-bounds coordinates (equivalent to zero-padding). The resulting gradients 
 * are written out to separate contiguous arrays to optimize memory access patterns 
 * for subsequent processing stages.
 * * @param input  Pointer to the 8-bit grayscale input image buffer.
 * @param gx     Pointer to the 16-bit output buffer for the horizontal (X) gradient.
 * @param gy     Pointer to the 16-bit output buffer for the vertical (Y) gradient.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
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
            gx[y * width + x] = (int16_t)sumX; // Store in SoA
            gy[y * width + x] = (int16_t)sumY;
        }
    }
}