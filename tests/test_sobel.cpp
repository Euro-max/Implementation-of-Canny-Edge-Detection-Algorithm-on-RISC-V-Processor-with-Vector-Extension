/**
 * @file test_sobel.cpp
 * @brief Unit tests for the Sobel operator gradient computation.
 * * This file contains bare-metal assertion tests to verify that the Sobel 
 * edge detection kernels correctly identify spatial frequencies. It tests 
 * both the horizontal (Gx) and vertical (Gy) gradient responses against 
 * known visual patterns such as flat regions and sharp directional edges.
 */

#include <iostream>
#include <cassert>
#include <cstdint>
#include "sobel.h"

/**
 * @brief Verifies that a uniform image produces zero gradients.
 * * If an image has constant intensity across all pixels (100), there are 
 * no edges. Therefore, the convolution of both the Gx and Gy Sobel kernels 
 * must result in exactly 0 for all interior pixels. This test purposefully 
 * skips the 1-pixel boundary to avoid padding artifacts.
 */
// Test 1: Uniform image = zero gradients
void test_uniform_image() {
    int W = 8, H = 8;
    uint8_t input[64];
    int16_t gx[64] = {0}, gy[64] = {0};
    for (int i = 0; i < 64; i++) input[i] = 100;

    compute_sobel(input, gx, gy, W, H);

    // Only check interior pixels (away from borders)
    for (int y = 1; y < H-1; y++) {
        for (int x = 1; x < W-1; x++) {
            assert(gx[y * W + x] == 0);
            assert(gy[y * W + x] == 0);
        }
    }
    std::cout << "✓ Test 1 PASSED: Uniform image = zero gradients\n";
}

/**
 * @brief Verifies that a vertical edge is accurately detected by the Gx kernel.
 * * Generates an image where the left half is black (0) and the right half 
 * is white (255). Because the intensity changes from left-to-right across 
 * the x-axis, the horizontal gradient (Gx) must produce a non-zero response 
 * at the edge boundary.
 */
// Test 2: Vertical edge detected in Gx
void test_vertical_edge() {
    int W = 8, H = 8;
    uint8_t input[64] = {0};
    int16_t gx[64] = {0}, gy[64] = {0};

    // Left half black, right half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y * W + x] = (x < W/2) ? 0 : 255;

    compute_sobel(input, gx, gy, W, H);

    // Gx should be non-zero at the edge (middle column)
    assert(gx[3 * W + 3] != 0 || gx[4 * W + 4] != 0);
    std::cout << "✓ Test 2 PASSED: Vertical edge detected in Gx\n";
}

/**
 * @brief Verifies that a horizontal edge is accurately detected by the Gy kernel.
 * * Generates an image where the top half is black (0) and the bottom half 
 * is white (255). Because the intensity changes from top-to-bottom across 
 * the y-axis, the vertical gradient (Gy) must produce a non-zero response 
 * at the edge boundary.
 */
// Test 3: Horizontal edge detected in Gy
void test_horizontal_edge() {
    int W = 8, H = 8;
    uint8_t input[64] = {0};
    int16_t gx[64] = {0}, gy[64] = {0};

    // Top half black, bottom half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y * W + x] = (y < H/2) ? 0 : 255;

    compute_sobel(input, gx, gy, W, H);

    // Gy should be non-zero at the edge
    assert(gy[4 * W + 4] != 0);
    std::cout << "✓ Test 3 PASSED: Horizontal edge detected in Gy\n";
}

/**
 * @brief Verifies that an entirely black image produces zero gradients.
 * * A zero-intensity image has no gradients. This acts as a sanity check 
 * to ensure that uninitialized buffer values or improper kernel offsets 
 * do not falsely inject non-zero data into the output gradient buffers.
 */
// Test 4: Black image = zero gradients
void test_black_image() {
    int W = 8, H = 8;
    uint8_t input[64] = {0};
    int16_t gx[64] = {0}, gy[64] = {0};

    compute_sobel(input, gx, gy, W, H);

    for (int i = 0; i < 64; i++) {
        assert(gx[i] == 0);
        assert(gy[i] == 0);
    }
    std::cout << "✓ Test 4 PASSED: Black image = zero gradients\n";
}

/**
 * @brief Main execution entry point for Sobel tests.
 * * Runs all test cases sequentially. If any assert fails, the program 
 * will abort immediately. Otherwise, it prints a success summary.
 * * @return 0 on successful execution of all assertions.
 */
int main() {
    std::cout << "=== Sobel Tests ===\n\n";
    test_uniform_image();
    test_vertical_edge();
    test_horizontal_edge();
    test_black_image();
    std::cout << "\n✅ All Sobel tests PASSED!\n";
    return 0;
}