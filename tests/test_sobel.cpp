#include <iostream>
#include <cassert>
#include <cstdint>
#include "sobel.h"

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

int main() {
    std::cout << "=== Sobel Tests ===\n\n";
    test_uniform_image();
    test_vertical_edge();
    test_horizontal_edge();
    test_black_image();
    std::cout << "\n✅ All Sobel tests PASSED!\n";
    return 0;
}

