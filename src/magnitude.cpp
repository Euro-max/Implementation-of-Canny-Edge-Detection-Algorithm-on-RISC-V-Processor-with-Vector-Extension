#include "magnitude.h"
#include <cmath>
#include <algorithm>
#include <vector>

void compute_magnitude_l1(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height) {
    int32_t max_mag = 0;
    int size = width * height;
    std::vector<int32_t> temp_mag(size);

    // Pass 1: Compute L1 magnitude and find max
    for (int i = 0; i < size; ++i) {
        temp_mag[i] = std::abs(gx[i]) + std::abs(gy[i]);
        if (temp_mag[i] > max_mag) max_mag = temp_mag[i];
    }

    // Pass 2: Normalize to [0, 255]
    float scale = (max_mag > 0) ? 255.0f / max_mag : 0.0f;
    for (int i = 0; i < size; ++i) {
        output[i] = (uint8_t)(temp_mag[i] * scale);
    }
}

void compute_magnitude_l2(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height) {
    int size = width * height;
    std::vector<float> temp_mag(size);
    float max_mag = 0.0f;

    // Pass 1: Compute L2 magnitude and find max
    for (int i = 0; i < size; ++i) {
        temp_mag[i] = sqrt((float)gx[i] * gx[i] + 
                          (float)gy[i] * gy[i]);
        if (temp_mag[i] > max_mag) max_mag = temp_mag[i];
    }

    // Pass 2: Normalize to [0, 255]
    float scale = (max_mag > 0) ? 255.0f / max_mag : 0.0f;
    for (int i = 0; i < size; ++i) {
        output[i] = (uint8_t)(temp_mag[i] * scale);
    }
}
