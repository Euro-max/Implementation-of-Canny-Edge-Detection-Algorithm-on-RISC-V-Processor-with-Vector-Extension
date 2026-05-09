/**
 * @file gaussian.ipp
 * @brief Template implementation of 5x5 Gaussian blur
 * @ingroup canny
 * 
 * Inline implementation file for the Gaussian blur template.
 * Includes boundary handling with zero-padding.
 */

#include <algorithm>

/**
 * @brief Template specialization of 5x5 Gaussian convolution
 * 
 * Implementation performs:
 * 1. Nested loops over output pixels
 * 2. 5x5 convolution with kernel coefficients
 * 3. Zero-padding for boundary pixels
 * 4. Integer division by 273 for normalization
 * 
 * @tparam T_in  Input pixel type (convertible to T_acc)
 * @tparam T_out Output pixel type (from T_acc via cast)
 * @tparam T_acc Accumulator type (large enough to avoid overflow)
 * 
 * @param input   Source image data
 * @param output  Destination image data
 * @param width   Image width
 * @param height  Image height
 */
template <typename T_in, typename T_out, typename T_acc>
void gaussian_blur_5x5(const T_in* input, T_out* output, int width, int height) {
    const int16_t kernel[5][5] = {
        {1,  4,  7,  4, 1}, {4, 16, 26, 16, 4}, {7, 26, 41, 26, 7},
        {4, 16, 26, 16, 4}, {1,  4,  7,  4, 1}
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            T_acc sum = 0;
            for (int ky = -2; ky <= 2; ++ky) {
                for (int kx = -2; kx <= 2; ++kx) {
                    int iy = y + ky;
                    int ix = x + kx;
                    // Boundary handling: Zero-padding [cite: 65]
                    if (iy >= 0 && iy < height && ix >= 0 && ix < width) {
                        sum += (T_acc)input[iy * width + ix] * kernel[ky + 2][kx + 2];
                    }
                }
            }
            output[y * width + x] = (T_out)(sum / 273); 
        }
    }
}