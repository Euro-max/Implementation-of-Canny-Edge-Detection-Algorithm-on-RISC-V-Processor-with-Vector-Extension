#include <cstdint>

// The 5x5 Gaussian Kernel coefficients 
const int16_t gaussian_kernel[5][5] = {
    {2,  4,  5,  4, 2},
    {4,  9, 12,  9, 4},
    {5, 12, 15, 12, 5},
    {4,  9, 12,  9, 4},
    {2,  4,  5,  4, 2}
};

void gaussian_blur_scalar(const uint8_t* input, uint8_t* output, int width, int height) {
    // Loop through every pixel in the image [cite: 154]
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int32_t accumulator = 0; // Use 32-bit to avoid overflow 

            // Apply the 5x5 kernel (ky/kx are kernel offsets) 
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int py = y + ky;
                    int px = x + kx;

                    // Zero-padding: if outside the image, treat as 0 [cite: 65]
                    if (py >= 0 && py < height && px >= 0 && px < width) {
                        uint8_t pixel = input[py * width + px];
                        accumulator += pixel * gaussian_kernel[ky + 2][kx + 2];
                    }
                }
            }
            // Divide by the kernel sum (273) and store 
            output[y * width + x] = static_cast<uint8_t>(accumulator / 273);
        }
    }
}
