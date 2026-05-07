#include "sobel.h"
#include <cmath>

void applySobel(const Image& input, Image& output) {
    int w = input.width;
    int h = input.height;
    
    // Initialize output
    output.width = w;
    output.height = h;
    output.magnitudes.assign(w * h, 0.0f);//w*h is number of pixels in the input image, based on that number, memory is reserved and each memory slot is initalised to 0
    output.angles.assign(w * h, 0.0f);

    // Loop through pixels, not calculating at edges. That is why y starts at 1 and ends at h-2 to avoid segmentation fault since there is no pixel at -1
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            // Gx Kernel (Vertical edges)
            float gx = -1.0f * input.pixels[(y-1)*w + (x-1)] + 1.0f * input.pixels[(y-1)*w + (x+1)]
                     - 2.0f * input.pixels[(y)*w   + (x-1)] + 2.0f * input.pixels[(y)*w   + (x+1)]
                     - 1.0f * input.pixels[(y+1)*w + (x-1)] + 1.0f * input.pixels[(y+1)*w + (x+1)];

            // Gy Kernel (Horizontal edges)
            float gy =   1.0f * input.pixels[(y-1)*w + (x-1)] + 2.0f * input.pixels[(y-1)*w + (x)] + 1.0f * input.pixels[(y-1)*w + (x+1)]
                       - 1.0f * input.pixels[(y+1)*w + (x-1)] - 2.0f * input.pixels[(y+1)*w + (x)] - 1.0f * input.pixels[(y+1)*w + (x+1)];

            // Calculate Magnitude and Angle
            output.magnitudes[y * w + x] = std::sqrt(gx * gx + gy * gy);
            output.angles[y * w + x] = std::atan2(gy, gx);
        }
    }
}
