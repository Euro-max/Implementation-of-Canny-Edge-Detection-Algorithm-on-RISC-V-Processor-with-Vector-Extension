#pragma once
#include "image.h"
#include <array>

// 5x5 Gaussian kernel (sum = 273)
static const int GAUSS_KERNEL[5][5] = {
    { 1,  4,  7,  4,  1},
    { 4, 16, 26, 16,  4},
    { 7, 26, 41, 26,  7},
    { 4, 16, 26, 16,  4},
    { 1,  4,  7,  4,  1}
};

Image gaussian_blur(const Image& input) {
    Image output(input.width, input.height);

    for (int row = 0; row < input.height; row++) {
        for (int col = 0; col < input.width; col++) {
            int sum = 0;

            for (int kr = -2; kr <= 2; kr++) {
                for (int kc = -2; kc <= 2; kc++) {
                    int r = row + kr;
                    int c = col + kc;

                    // Zero padding - treat out of bounds as 0
                    if (r < 0 || r >= input.height || 
                        c < 0 || c >= input.width) {
                        continue;
                    }

                    sum += input.at(r, c) * GAUSS_KERNEL[kr+2][kc+2];
                }
            }

            // Divide by 273 and clamp to [0,255]
            output.at(row, col) = static_cast<uint8_t>(sum / 273);
        }
    }
    return output;
}

