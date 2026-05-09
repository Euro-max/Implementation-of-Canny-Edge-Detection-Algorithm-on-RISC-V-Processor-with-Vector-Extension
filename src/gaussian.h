/**
 * @file gaussian.h
 * @brief Template-based 5x5 Gaussian blur implementation
 * @ingroup canny
 * 
 * Provides a templated Gaussian blur function for image smoothing
 * before gradient computation. The 5x5 kernel approximates a
 * Gaussian distribution with σ ≈ 1.0.
 * 
 * The template design allows for different input/output types and
 * accumulator precision, following professional vision library practices.
 */

#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <cstdint>

/**
 * @brief Applies a 5x5 Gaussian blur to an image
 * 
 * Uses integer kernel with coefficients:
 * @code
 * [1,  4,  7,  4, 1]
 * [4, 16, 26, 16, 4]
 * [7, 26, 41, 26, 7]
 * [4, 16, 26, 16, 4]
 * [1,  4,  7,  4, 1]
 * @endcode
 * 
 * Normalization factor: 273 (sum of all kernel coefficients)
 * 
 * @tparam T_in  Input pixel type (e.g., uint8_t)
 * @tparam T_out Output pixel type (e.g., uint8_t)
 * @tparam T_acc Accumulator type for intermediate sums (e.g., int32_t)
 * 
 * @param input   Input image buffer (width × height)
 * @param output  Output blurred image buffer
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * 
 * @pre input and output buffers must be allocated
 * @pre width > 0, height > 0
 * @pre Boundary handling uses zero-padding
 * 
 * @note The kernel is separable but implemented as full 5x5 for simplicity
 * @note 64-byte aligned buffers recommended for SIMD/RVV optimization
 * 
 * @see compute_sobel() For edge detection after blurring
 */
template <typename T_in, typename T_out, typename T_acc>
void gaussian_blur_5x5(const T_in* input, T_out* output, int width, int height);

#include "gaussian.ipp" // Implementation of template
#endif