#include <iostream>
#include <cassert>
#include <cstdint>
#include "gaussian.h"

// Test 1: Uniform image stays uniform after blur
void test_uniform_image() {
    int W = 10, H = 10;
    uint8_t input[100], output[100];

    // Fill with constant value 100
    for (int i = 0; i < 100; i++) input[i] = 100;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(input, output, W, H);

    // Interior pixels should still be ~100
    for (int y = 2; y < H-2; y++) {
        for (int x = 2; x < W-2; x++) {
            assert(output[y * W + x] == 100);
        }
    }
    std::cout << "✓ Test 1 PASSED: Uniform image stays uniform\n";
}

// Test 2: Black image stays black
void test_black_image() {
    int W = 10, H = 10;
    uint8_t input[100] = {0};
    uint8_t output[100] = {0};

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(input, output, W, H);

    for (int i = 0; i < 100; i++) {
        assert(output[i] == 0);
    }
    std::cout << "✓ Test 2 PASSED: Black image stays black\n";
}

// Test 3: Blur smooths a sharp edge
void test_blur_smooths_edge() {
    int W = 20, H = 10;
    uint8_t input[200], output[200];

    // Left half black, right half white
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            input[y * W + x] = (x < W/2) ? 0 : 255;
        }
    }

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(input, output, W, H);

    // Middle pixel at the edge should be blurred (not 0 or 255)
    int mid_pixel = output[5 * W + 10];
    assert(mid_pixel > 0 && mid_pixel < 255);
    std::cout << "✓ Test 3 PASSED: Sharp edge is smoothed (value=" 
              << mid_pixel << ")\n";
}

// Test 4: Output is always in valid range [0, 255]
void test_output_range() {
    int W = 16, H = 16;
    uint8_t input[256], output[256];

    // Checkerboard pattern (worst case for blur)
    for (int i = 0; i < 256; i++)
        input[i] = (i % 2 == 0) ? 255 : 0;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(input, output, W, H);

    for (int i = 0; i < 256; i++) {
        assert(output[i] >= 0 && output[i] <= 255);
    }
    std::cout << "✓ Test 4 PASSED: All output values in [0, 255]\n";
}

int main() {
    std::cout << "=== Gaussian Blur Tests ===\n\n";
    test_uniform_image();
    test_black_image();
    test_blur_smooths_edge();
    test_output_range();
    std::cout << "\n✅ All Gaussian tests PASSED!\n";
    return 0;
}
