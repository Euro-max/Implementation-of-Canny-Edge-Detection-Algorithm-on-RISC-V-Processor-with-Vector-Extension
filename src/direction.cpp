/**
 * @file direction.cpp
 * @brief Implementation of gradient direction quantization.
 * * This file contains the algorithm to compute the angular direction of 
 * the spatial gradients. Instead of computing the exact angle using costly 
 * floating-point operations like atan2(), it highly optimizes the process 
 * by using integer cross-multiplication to categorize the gradient into 
 * one of four discrete angular bins (0, 45, 90, 135 degrees). These bins 
 * are essential for the subsequent Non-Maximum Suppression (NMS) stage.
 */

#include "direction.h"
#include <cmath>
#include <algorithm>

/**
 * @brief Computes and quantizes the gradient direction for every pixel.
 * * This function evaluates the horizontal (Gx) and vertical (Gy) gradient 
 * components to determine the dominant edge direction. It uses an integer-based 
 * approximation for the tangent boundaries (22.5° and 67.5°) to map the 
 * continuous gradient angle into four discrete directional bins:
 * - 0: Horizontal gradient (Vertical edge)
 * - 1: 45-degree diagonal gradient
 * - 2: Vertical gradient (Horizontal edge)
 * - 3: 135-degree diagonal gradient
 * * @param gx     Pointer to the 16-bit input buffer containing horizontal gradients.
 * @param gy     Pointer to the 16-bit input buffer containing vertical gradients.
 * @param output Pointer to the 8-bit output buffer for the quantized directions [0-3].
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 */
void compute_direction(const int16_t* gx, const int16_t* gy, uint8_t* output, int width, int height) {
    int size = width * height;

    for (int i = 0; i < size; ++i) {
        int16_t ix = gx[i];
        int16_t iy = gy[i];

        // Use absolute values for comparison
        int16_t ax = std::abs(ix);
        int16_t ay = std::abs(iy);

        // Optimization from Guide: Use integer cross-multiplication instead of tan()
        // tan(22.5) approx 2/5, tan(67.5) approx 12/5 
        
        uint8_t dir = 0;
        if (ay * 5 < ax * 2) {
            dir = 0; // Horizontal gradient (Vertical edge) 
        } else if (ay * 2 > ax * 5) {
            dir = 2; // Vertical gradient (Horizontal edge) 
        } else {
            // Diagonal cases: Check if signs of Gx and Gy are same or different
            if ((ix > 0 && iy > 0) || (ix < 0 && iy < 0)) {
                dir = 1; // 45 degrees 
            } else {
                dir = 3; // 135 degrees 
            }
        }
        output[i] = dir;
    }
}