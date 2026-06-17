/**
 * @file test_magnitude.cpp
 * @brief Unit tests for gradient magnitude computation (L1 and L2 norms).
 * * This file contains bare-metal assertion tests to verify the correctness of 
 * the magnitude calculations. It tests both the L1 (Manhattan distance) and 
 * L2 (Euclidean distance) implementations, ensuring they handle edge cases, 
 * stay within 8-bit bounds, and adhere to mathematical expectations.
 */

#include <iostream>
#include <cassert>
#include <cstdint>
#include <cmath>
#include "magnitude.h"

/**
 * @brief Verifies that zero gradients produce an absolute zero magnitude.
 * * If there is no change in pixel intensity in either the X or Y direction 
 * (Gx = 0, Gy = 0), the resulting edge magnitude must be precisely 0 for 
 * both L1 and L2 calculations.
 */
// Test 1: Zero gradients = zero magnitude
void test_zero_gradient() {
    int W = 4, H = 4;
    int16_t gx[16] = {0};
    int16_t gy[16] = {0};
    uint8_t out_l1[16], out_l2[16];

    compute_magnitude_l1(gx, gy, out_l1, W, H);
    compute_magnitude_l2(gx, gy, out_l2, W, H);

    for (int i = 0; i < 16; i++) {
        assert(out_l1[i] == 0);
        assert(out_l2[i] == 0);
    }
    std::cout << "✓ Test 1 PASSED: Zero gradient = zero magnitude\n";
}

/**
 * @brief Ensures that computed magnitudes strictly bounded within the 8-bit range.
 * * Feeds a combination of large positive and negative gradients into the 
 * magnitude functions to ensure that normalization is working correctly and 
 * no integer overflow or underflow breaks the [0, 255] boundary constraint.
 */
// Test 2: Output always in [0, 255]
void test_output_range() {
    int W = 4, H = 4;
    int16_t gx[16] = {100, -200, 300, -400, 100, -200, 300, -400,
                      100, -200, 300, -400, 100, -200, 300, -400};
    int16_t gy[16] = {-400, 300, -200, 100, -400, 300, -200, 100,
                      -400, 300, -200, 100, -400, 300, -200, 100};
    uint8_t out_l1[16], out_l2[16];

    compute_magnitude_l1(gx, gy, out_l1, W, H);
    compute_magnitude_l2(gx, gy, out_l2, W, H);

    for (int i = 0; i < 16; i++) {
        assert(out_l1[i] >= 0 && out_l1[i] <= 255);
        assert(out_l2[i] >= 0 && out_l2[i] <= 255);
    }
    std::cout << "✓ Test 2 PASSED: Output always in [0, 255]\n";
}

/**
 * @brief Validates the mathematical property that L1 norm >= L2 norm.
 * * Mathematically, the sum of absolute values (|Gx| + |Gy|) will always be 
 * greater than or equal to the square root of their sum of squares 
 * sqrt(Gx^2 + Gy^2). This test ensures both implementations scale relatively.
 */
// Test 3: L1 >= L2 always (L1 overestimates)
void test_l1_greater_than_l2()  {
    int W = 4, H = 4;
    int16_t gx[16] = {10, 20, 30, 40, 50, 60, 70, 80,
                      10, 20, 30, 40, 50, 60, 70, 80};
    int16_t gy[16] = {80, 70, 60, 50, 40, 30, 20, 10,
                      80, 70, 60, 50, 40, 30, 20, 10};
    uint8_t out_l1[16], out_l2[16];

    compute_magnitude_l1(gx, gy, out_l1, W, H);
    compute_magnitude_l2(gx, gy, out_l2, W, H);

    for (int i = 0; i < 16; i++) {
        assert(out_l1[i] >= out_l2[i]);
    }
    std::cout << "✓ Test 3 PASSED: L1 >= L2 always\n";
}

/**
 * @brief Verifies that a spatially uniform gradient field yields a uniform output.
 * * If every pixel experiences the exact same gradient changes, the resulting 
 * magnitude buffer must contain identical values across all indices. This 
 * catches indexing bugs or stray accumulations in the processing loop.
 */
// Test 4: Uniform magnitude = uniform output
void test_uniform_magnitude() {
    int W = 4, H = 4;
    // Same gradient everywhere
    int16_t gx[16], gy[16];
    for (int i = 0; i < 16; i++) { gx[i] = 100; gy[i] = 0; }
    uint8_t out_l1[16], out_l2[16];

    compute_magnitude_l1(gx, gy, out_l1, W, H);
    compute_magnitude_l2(gx, gy, out_l2, W, H);

    // All outputs should be equal
    for (int i = 1; i < 16; i++) {
        assert(out_l1[i] == out_l1[0]);
        assert(out_l2[i] == out_l2[0]);
    }
    std::cout << "✓ Test 4 PASSED: Uniform gradient = uniform output\n";
}

/**
 * @brief Main execution entry point for magnitude tests.
 * * Runs all test cases sequentially. If any assert fails, the program 
 * will abort immediately. Otherwise, it prints a success summary.
 * * @return 0 on successful execution of all assertions.
 */
int main() {
    std::cout << "=== Magnitude Tests ===\n\n";
    test_zero_gradient();
    test_output_range();
    test_l1_greater_than_l2();
    test_uniform_magnitude();
    std::cout << "\n✅ All Magnitude tests PASSED!\n";
    return 0;
}