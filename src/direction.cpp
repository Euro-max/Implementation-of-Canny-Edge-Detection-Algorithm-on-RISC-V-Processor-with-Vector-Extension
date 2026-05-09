/**
 * @file direction.cpp
 * @brief Implementation of gradient direction quantization
 * @ingroup canny
 * 
 * Implements a fast integer-based gradient direction quantization
 * using cross-multiplication instead of arctangent calculations.
 */

#include "direction.h"
#include <cmath>
#include <algorithm>

/**
 * @brief Computes gradient direction using integer arithmetic optimization
 * 
 * Implementation details:
 * - Uses absolute values for magnitude comparison
 * - Integer cross-multiplication avoids float division
 * - Thresholds: |Gy| * 5 < |Gx| * 2 → near-horizontal
 * - Thresholds: |Gy| * 2 > |Gx| * 5 → near-vertical
 * - Diagonal cases use sign comparison for orientation
 * 
 * Complexity: O(width × height) with all integer operations
 * 
 * @param gx      Horizontal Sobel gradients (Sx)
 * @param gy      Vertical Sobel gradients (Sy)
 * @param output  Output direction map (0-3 per pixel)
 * @param width   Image width
 * @param height  Image height
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