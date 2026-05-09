/**
 * @file main.cpp
 * @brief Complete Canny edge detection pipeline
 * @ingroup canny
 * 
 * Executes the full Canny edge detection pipeline:
 * 1. Load raw 8-bit grayscale image
 * 2. Apply 5x5 Gaussian blur for noise reduction
 * 3. Compute Sobel gradients (Gx, Gy)
 * 4. Calculate gradient magnitude (L1 norm)
 * 5. Quantize gradient direction (4 bins)
 * 6. Save magnitude output as raw image
 * 
 * @section usage Usage
 * @code
 * ./canny <input.raw> <output.raw> <width> <height>
 * @endcode
 * 
 * @section pipeline Pipeline Stages
 * - Stage 1: Gaussian blur (σ≈1.0, 5x5 kernel)
 * - Stage 2: Sobel operator (3x3 kernels)
 * - Stage 3: L1 magnitude with normalization
 * - Stage 4: Direction quantization (0°, 45°, 90°, 135°)
 * 
 * @note Full Canny also includes non-maximum suppression and hysteresis
 *       thresholding, which are not implemented in this version.
 * 
 * @author Generated from Canny edge detection implementation
 * @date Current version
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <iostream>

/**
 * @brief Main entry point for Canny edge detection pipeline
 * @param argc Number of command line arguments (must be 5)
 * @param argv Command line arguments:
 *            argv[1] = input raw image file path
 *            argv[2] = output raw image file path
 *            argv[3] = image width (pixels)
 *            argv[4] = image height (pixels)
 * @return 0 on success, non-zero on error
 * 
 * @retval 0 Successfully completed pipeline
 * @retval 1 Invalid arguments or file I/O error
 * 
 * @exception Handles memory allocation failures gracefully
 * @exception File I/O errors reported via stderr
 */
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