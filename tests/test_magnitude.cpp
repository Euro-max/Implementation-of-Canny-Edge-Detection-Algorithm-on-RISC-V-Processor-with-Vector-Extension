#include <iostream>
#include <cassert>
#include <cstdint>
#include <cmath>
#include "magnitude.h"

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

int main() {
    std::cout << "=== Magnitude Tests ===\n\n";
    test_zero_gradient();
    test_output_range();
    test_l1_greater_than_l2();
    test_uniform_magnitude();
    std::cout << "\n✅ All Magnitude tests PASSED!\n";
    return 0;
}
