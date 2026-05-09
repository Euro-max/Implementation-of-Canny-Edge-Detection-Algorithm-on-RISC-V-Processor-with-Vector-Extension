#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: ./canny <in.raw> <out.raw> <width> <height>\n";
        return 1;
    }

    int W = std::stoi(argv[3]);
    int H = std::stoi(argv[4]);

    std::cerr << "W=" << W << " H=" << H << "\n";
    std::cerr << "Input: " << argv[1] << "\n";
    std::cerr << "Output: " << argv[2] << "\n";

    uint8_t* img_in  = allocate_buffer(W, H);
    uint8_t* img_blur = allocate_buffer(W, H);
    int16_t* gx = (int16_t*)aligned_alloc(64, W * H * 2);
    int16_t* gy = (int16_t*)aligned_alloc(64, W * H * 2);
    uint8_t* img_mag = allocate_buffer(W, H);
    uint8_t* img_dir = allocate_buffer(W, H);

    std::cerr << "Buffers allocated\n";

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir) {
        std::cerr << "ERROR: Buffer allocation failed!\n";
        return 1;
    }

    if (load_raw(argv[1], img_in, W, H)) {
        std::cerr << "File loaded OK\n";
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
        std::cerr << "Gaussian done\n";
        compute_sobel(img_blur, gx, gy, W, H);
        std::cerr << "Sobel done\n";
        compute_magnitude_l1(gx, gy, img_mag, W, H);
        std::cerr << "Magnitude done\n";
        compute_direction(gx, gy, img_dir, W, H);
        std::cerr << "Direction done\n";
        save_raw(argv[2], img_mag, W, H);
        std::cerr << "Saved output!\n";
        std::cout << "Pipeline complete.\n";
    } else {
        std::cerr << "ERROR: Failed to load file: " << argv[1] << "\n";
        return 1;
    }

    return 0;
}
