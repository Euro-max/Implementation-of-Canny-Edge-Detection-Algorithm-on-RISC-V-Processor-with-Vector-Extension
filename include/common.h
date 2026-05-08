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

// Quantized directions for Canny Edge Detection
enum Direction : uint8_t {
    DIR_0   = 0,   // Horizontal gradient (0°)
    DIR_45  = 1,   // Diagonal gradient (45°)
    DIR_90  = 2,   // Vertical gradient (90°)
    DIR_135 = 3    // Diagonal gradient (135°)
};

#endif

