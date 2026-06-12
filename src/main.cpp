#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <iostream>
#include <chrono> // Added for time measurement

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

    uint8_t* img_in   = allocate_buffer(W, H);
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

        // ──── Profile Gaussian Blur ────
        auto start_gaussian = std::chrono::high_resolution_clock::now();
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
        auto end_gaussian = std::chrono::high_resolution_clock::now();
        auto duration_gaussian = std::chrono::duration_cast<std::chrono::microseconds>(end_gaussian - start_gaussian).count();
        std::cerr << "Gaussian done\n";

        // ──── Profile Sobel Filter ────
        auto start_sobel = std::chrono::high_resolution_clock::now();
        compute_sobel(img_blur, gx, gy, W, H);
        auto end_sobel = std::chrono::high_resolution_clock::now();
        auto duration_sobel = std::chrono::duration_cast<std::chrono::microseconds>(end_sobel - start_sobel).count();
        std::cerr << "Sobel done\n";

        // ──── Profile Magnitude Calculation ────
        auto start_mag = std::chrono::high_resolution_clock::now();
        compute_magnitude_l1(gx, gy, img_mag, W, H);
        auto end_mag = std::chrono::high_resolution_clock::now();
        auto duration_mag = std::chrono::duration_cast<std::chrono::microseconds>(end_mag - start_mag).count();
        std::cerr << "Magnitude done\n";

        // ──── Profile Direction Calculation ────
        auto start_dir = std::chrono::high_resolution_clock::now();
        compute_direction(gx, gy, img_dir, W, H);
        auto end_dir = std::chrono::high_resolution_clock::now();
        auto duration_dir = std::chrono::duration_cast<std::chrono::microseconds>(end_dir - start_dir).count();
        std::cerr << "Direction done\n";

        save_raw(argv[2], img_mag, W, H);
        std::cerr << "Saved output!\n";

        // ──── Print Execution Time Report for Phase 5 ────
        std::cout << "\n================= PROFILING RESULTS =================" << std::endl;
        std::cout << "Gaussian Blur Time : " << duration_gaussian << " us" << std::endl;
        std::cout << "Sobel Filter Time  : " << duration_sobel << " us" << std::endl;
        std::cout << "Magnitude Time     : " << duration_mag << " us" << std::endl;
        std::cout << "Direction Time     : " << duration_dir << " us" << std::endl;
        std::cout << "=====================================================\n" << std::endl;
        std::cout << "Pipeline complete.\n";
    } else {
        std::cerr << "ERROR: Failed to load file: " << argv[1] << "\n";
        return 1;
    }

    return 0;
}
