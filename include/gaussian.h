/**
 * @file gaussian.h
 * @brief Declarations for the Gaussian blur filter.
 * * This header defines the template interface for a 5x5 Gaussian blur 
 * operation. By utilizing C++ templates, the filter can seamlessly operate 
 * on various data types (e.g., 8-bit integers for standard images, 
 * or floating-point types for higher precision pipelines) without 
 * duplicating the underlying convolution logic.
 */

#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <cstdint>

/**
 * @brief Applies a 5x5 Gaussian blur to a 2D image array.
 * * Convolves the input image with a fixed 5x5 approximation of a Gaussian kernel. 
 * It smooths the image to reduce high-frequency noise and detail prior to 
 * edge detection.
 * * @tparam T_in  Data type of the input image pixels (e.g., uint8_t).
 * @tparam T_out Data type of the output image pixels (e.g., uint8_t).
 * @tparam T_acc Data type used for internal accumulation to prevent overflow (e.g., int32_t).
 * * @param input  Pointer to the input image buffer.
 * @param output Pointer to the output image buffer where the blurred image will be stored.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
// Template design as recommended for professional vision libraries
template <typename T_in, typename T_out, typename T_acc>
void gaussian_blur_5x5(const T_in* input, T_out* output, int width, int height);

#include "gaussian.ipp" // Implementation of template
#endif