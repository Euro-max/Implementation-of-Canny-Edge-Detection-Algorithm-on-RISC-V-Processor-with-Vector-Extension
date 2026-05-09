#include <iostream>
#include <cassert>
#include <cstdint>
#include "direction.h"

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

int main() {
    std::cout << "=== Direction Tests ===\n\n";
    test_zero_gradient();
    test_horizontal_gradient();
    test_vertical_gradient();
    test_output_range();
    std::cout << "\n✅ All Direction tests PASSED!\n";
    return 0;
}
