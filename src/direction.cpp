#include "common.h"
#include <cmath> // For std::abs

/**
 * Quantizes gradient direction into four values using integer arithmetic.
 * Uses cross-multiplication (ay * 5 < ax * 2) as an embedded optimization.
 */
void compute_direction(const int16_t* Gx, const int16_t* Gy, uint8_t* direction, int width, int height) {
    for (int i = 0; i < width * height; ++i) {
        int32_t ax = std::abs(Gx[i]);
        int32_t ay = std::abs(Gy[i]);

        // Use cross-multiplication with 2/5 (tan 22.5) and 12/5 (tan 67.5)
        if (ay * 5 < ax * 2) {
            direction[i] = DIR_0;   // Horizontal
        } else if (ay * 2 > ax * 5) {
            direction[i] = DIR_90;  // Vertical
        } else {
            // Check signs to distinguish 45° from 135°
            if ((Gx[i] > 0 && Gy[i] > 0) || (Gx[i] < 0 && Gy[i] < 0)) {
                direction[i] = DIR_45;
            } else {
                direction[i] = DIR_135;
            }
        }
    }
}
