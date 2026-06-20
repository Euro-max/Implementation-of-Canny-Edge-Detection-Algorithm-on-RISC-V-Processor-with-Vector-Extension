/**
 * @file sobel.h
 * @brief Declarations for the Sobel operator gradient computation.
 * * This header defines the interface for calculating the spatial gradients 
 * of an image. The Sobel operator uses two 3x3 convolution kernels to 
 * estimate the derivative in the horizontal (Gx) and vertical (Gy) directions, 
 * which is a fundamental step in edge detection.
 */

#ifndef SOBEL_H
#define SOBEL_H

#include <cstdint>

/**
 * @brief Computes the horizontal and vertical gradients of an image using Sobel operators.
 * * Applies the 3x3 Sobel kernels across the input image. Boundary pixels are handled 
 * implicitly by skipping out-of-bounds coordinates (equivalent to zero-padding). 
 * The resulting gradients are written out to separate contiguous arrays to optimize 
 * memory access patterns.
 * * Uses int16_t for gradients as it's sufficient for 8-bit input to prevent overflow 
 * during the convolution accumulations.
 * * @param input  Pointer to the 8-bit grayscale input image buffer.
 * @param gx     Pointer to the 16-bit output buffer for the horizontal (X) gradient.
 * @param gy     Pointer to the 16-bit output buffer for the vertical (Y) gradient.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
void compute_sobel(const uint8_t* input, int16_t* gx, int16_t* gy, int width, int height);

#endif