#include <algorithm>

template <typename T_in, typename T_out, typename T_acc>
void gaussian_blur_5x5(const T_in* input, T_out* output, int width, int height) {
    const int16_t kernel[5][5] = {
        {1,  4,  7,  4, 1}, {4, 16, 26, 16, 4}, {7, 26, 41, 26, 7},
        {4, 16, 26, 16, 4}, {1,  4,  7,  4, 1}
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            T_acc sum = 0;
            for (int ky = -2; ky <= 2; ++ky) {
                for (int kx = -2; kx <= 2; ++kx) {
                    int iy = y + ky;
                    int ix = x + kx;
                    // Boundary handling: Zero-padding [cite: 65]
                    if (iy >= 0 && iy < height && ix >= 0 && ix < width) {
                        sum += (T_acc)input[iy * width + ix] * kernel[ky + 2][kx + 2];
                    }
                }
            }
            output[y * width + x] = (T_out)(sum / 273); // [cite: 64]
        }
    }
}
