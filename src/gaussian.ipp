/**
 * @file gaussian.ipp
 * @brief Template implementation for a 5x5 Gaussian blur filter.
 * * This file contains the template definition for applying a 5x5 discrete 
 * Gaussian blur convolution. The kernel approximates a Gaussian distribution 
 * to reduce image noise and high-frequency detail, which is a critical 
 * preprocessing step in edge detection pipelines.
 */

#include <algorithm>

/**
 * @brief Applies a 5x5 Gaussian blur to a 2D image array.
 * * Convolves the input image with a fixed 5x5 integer approximation of a 
 * Gaussian kernel. Boundary pixels are handled implicitly via zero-padding 
 * (out-of-bounds coordinates are ignored during accumulation). The final sum 
 * is normalized by dividing by 273 (the sum of all kernel weights) to 
 * preserve the overall brightness of the image.
 * * @tparam T_in  Data type of the input image pixels (e.g., uint8_t).
 * @tparam T_out Data type of the output image pixels (e.g., uint8_t).
 * @tparam T_acc Data type used for accumulation to prevent overflow (e.g., int32_t).
 * * @param input  Pointer to the input image buffer.
 * @param output Pointer to the output image buffer where the blurred image will be stored.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
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
                    // Boundary handling: Zero-padding
                    if (iy >= 0 && iy < height && ix >= 0 && ix < width) {
                        sum += (T_acc)input[iy * width + ix] * kernel[ky + 2][kx + 2];
                    }
                }
            }
            output[y * width + x] = (T_out)(sum / 273);
        }
    }
}
