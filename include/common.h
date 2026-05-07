#ifndef COMMON_H
#define COMMON_H
#include <vector>
#include <cstdint>

struct Image {
    int width;
    int height;
    std::vector<uint8_t> pixels;      // For 0-255 grayscale data
    std::vector<float> magnitudes;    // For Sobel results
    std::vector<float> angles;        // For Sobel directions
};
#endif

