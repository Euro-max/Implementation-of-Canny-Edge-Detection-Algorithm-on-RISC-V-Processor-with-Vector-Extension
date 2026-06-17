/**
 * @file test_direction.cpp
 * @brief Unit tests for the gradient direction computation.
 * * This file contains bare-metal assertion tests to verify that the 
 * compute_direction function correctly maps Gx and Gy gradient values 
 * into the four discrete angular bins (0, 1, 2, 3) required for 
 * Non-Maximum Suppression.
 */

#include <iostream>
#include <cassert>
#include <cstdint>
#include "direction.h"

/**
 * @brief Verifies behavior when both gradients are zero.
 * * Tests the edge case where there is no change in pixel intensity.
 * Based on the current implementation, if both Gx and Gy are exactly 0, 
 * the function falls back to assigning direction bin 3 (diagonal).
 */
// Test 1: Zero gradients = direction 0
void test_zero_gradient() {
    int W = 4, H = 4;
    int16_t gx[16] = {0}, gy[16] = {0};
    uint8_t output[16] = {0};

    compute_direction(gx, gy, output, W, H);

    // When both gradients are zero, direction is diagonal (3) by implementation
    for (int i = 0; i < 16; i++)
        assert(output[i] == 3);
    std::cout << "✓ Test 1 PASSED: Zero gradient handled consistently\n";
}

/**
 * @brief Verifies direction quantization for purely horizontal gradients.
 * * A purely horizontal gradient (large Gx, zero Gy) corresponds to a 
 * vertical edge. This should be mapped strictly to direction bin 0.
 */
// Test 2: Pure horizontal gradient = direction 0
void test_horizontal_gradient() {
    int W = 4, H = 4;
    int16_t gx[16], gy[16] = {0};
    uint8_t output[16];

    for (int i = 0; i < 16; i++) gx[i] = 100;

    compute_direction(gx, gy, output, W, H);

    for (int i = 0; i < 16; i++)
        assert(output[i] == 0);
    std::cout << "✓ Test 2 PASSED: Horizontal gradient = direction 0\n";
}

/**
 * @brief Verifies direction quantization for purely vertical gradients.
 * * A purely vertical gradient (zero Gx, large Gy) corresponds to a 
 * horizontal edge. This should be mapped strictly to direction bin 2.
 */
// Test 3: Pure vertical gradient = direction 2
void test_vertical_gradient() {
    int W = 4, H = 4;
    int16_t gx[16] = {0}, gy[16];
    uint8_t output[16];

    for (int i = 0; i < 16; i++) gy[i] = 100;

    compute_direction(gx, gy, output, W, H);

    for (int i = 0; i < 16; i++)
        assert(output[i] == 2);
    std::cout << "✓ Test 3 PASSED: Vertical gradient = direction 2\n";
}

/**
 * @brief Ensures that all output directions are within the valid boundary.
 * * Feeds a mixture of positive and negative Gx and Gy combinations to 
 * guarantee that the computed direction never exceeds the 4 defined 
 * angular bins [0, 1, 2, 3].
 */
// Test 4: Output always valid (0,1,2,3)
void test_output_range() {
    int W = 4, H = 4;
    int16_t gx[16] = {100,-100,50,-50,100,-100,50,-50,
                      100,-100,50,-50,100,-100,50,-50};
    int16_t gy[16] = {50,50,-50,-50,100,100,-100,-100,
                      50,50,-50,-50,100,100,-100,-100};
    uint8_t output[16];

    compute_direction(gx, gy, output, W, H);

    for (int i = 0; i < 16; i++)
        assert(output[i] >= 0 && output[i] <= 3);
    std::cout << "✓ Test 4 PASSED: Output always in valid range [0-3]\n";
}

/**
 * @brief Main execution entry point for direction tests.
 * * Runs all test cases sequentially. If any assert fails, the program 
 * will abort immediately. Otherwise, it prints a success summary.
 * * @return 0 on successful execution of all assertions.
 */
int main() {
    std::cout << "=== Direction Tests ===\n\n";
    test_zero_gradient();
    test_horizontal_gradient();
    test_vertical_gradient();
    test_output_range();
    std::cout << "\n✅ All Direction tests PASSED!\n";
    return 0;
}
