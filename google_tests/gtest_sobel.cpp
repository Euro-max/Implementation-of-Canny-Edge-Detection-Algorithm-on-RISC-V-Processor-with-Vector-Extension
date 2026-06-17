/**
 * @file gtest_sobel.cpp
 * @brief Google Test suite for Sobel edge detection operator
 * @ingroup tests
 * 
 * Tests the compute_sobel() function which computes horizontal and
 * vertical gradients using 3x3 Sobel kernels.
 * 
 * Sobel kernels:
 * - Gx: [[-1,0,1], [-2,0,2], [-1,0,1]]  (horizontal edges)
 * - Gy: [[-1,-2,-1], [0,0,0], [1,2,1]]  (vertical edges)
 * 
 * @see compute_sobel()
 */

#include <gtest/gtest.h>
#include "sobel.h"
#include <vector>
#include <cstdint>
#include <cstring>

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @struct SobelBuffers
 * @brief Test helper structure for Sobel gradient tests
 * 
 * Manages source image and gradient buffers with convenient initialization.
 */
struct SobelBuffers {
    int W, H;                         ///< Image dimensions
    std::vector<uint8_t>  src;        ///< Source image buffer
    std::vector<int16_t>  gx, gy;     ///< Output gradient buffers

    /**
     * @brief Construct buffers with optional fill value
     * @param w Image width
     * @param h Image height
     * @param fill Fill value for all source pixels (default 0)
     */
    SobelBuffers(int w, int h, uint8_t fill = 0)
        : W(w), H(h), src(w * h, fill), gx(w * h, 0), gy(w * h, 0) {}

    /**
     * @brief Run Sobel computation
     */
    void run() { compute_sobel(src.data(), gx.data(), gy.data(), W, H); }
};

// ──────────────────────────────────────────────────────────────────────────────
// 1. Constant image → all gradients zero
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ConstantImageZeroGradient
 * @brief Verify constant image produces zero gradients
 * 
 * Sobel operator computes derivatives, so constant image should
 * yield zero gradients everywhere.
 */
TEST(Sobel, ConstantImageZeroGradient) {
    SobelBuffers b(16, 16, 128);
    b.run();

    for (int y = 1; y < b.H - 1; ++y)
        for (int x = 1; x < b.W - 1; ++x) {
            int idx = y * b.W + x;
            EXPECT_EQ(b.gx[idx], 0) << "Gx nonzero at (" << x << "," << y << ")";
            EXPECT_EQ(b.gy[idx], 0) << "Gy nonzero at (" << x << "," << y << ")";
        }
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. Vertical edge (left half = 0, right half = 255) → strong Gx, weak Gy
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test VerticalEdgeStrongGx
 * @brief Verify vertical edge produces strong horizontal gradient response
 * 
 * At a vertical edge (intensity changes horizontally), Gx should be large.
 */
TEST(Sobel, VerticalEdgeStrongGx) {
    const int W = 16, H = 16;
    SobelBuffers b(W, H);

    for (int y = 0; y < H; ++y)
        for (int x = W / 2; x < W; ++x)
            b.src[y * W + x] = 255;

    b.run();

    // At the edge column (x == W/2), interior rows should have large |Gx|
    for (int y = 1; y < H - 1; ++y)
        EXPECT_GT(std::abs(b.gx[y * W + W / 2]), 0)
            << "Expected nonzero Gx at edge column, y=" << y;
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. Horizontal edge (top half = 0, bottom half = 255) → strong Gy, weak Gx
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test HorizontalEdgeStrongGy
 * @brief Verify horizontal edge produces strong vertical gradient response
 * 
 * At a horizontal edge (intensity changes vertically), Gy should be large.
 */
TEST(Sobel, HorizontalEdgeStrongGy) {
    const int W = 16, H = 16;
    SobelBuffers b(W, H);

    for (int y = H / 2; y < H; ++y)
        for (int x = 0; x < W; ++x)
            b.src[y * W + x] = 255;

    b.run();

    for (int x = 1; x < W - 1; ++x)
        EXPECT_GT(std::abs(b.gy[(H / 2) * W + x]), 0)
            << "Expected nonzero Gy at edge row, x=" << x;
}

// ──────────────────────────────────────────────────────────────────────────────
// 4. Gx and Gy are independent: pure vertical ramp → Gx constant, Gy ≈ 0
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test HorizontalRampOnlyGx
 * @brief Verify horizontal ramp produces only Gx, not Gy
 * 
 * When intensity varies only horizontally (each row is identical),
 * Gy should be zero everywhere.
 */
TEST(Sobel, HorizontalRampOnlyGx) {
    const int W = 16, H = 16;
    SobelBuffers b(W, H);

    // Linearly increasing intensity left→right, same for every row
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            b.src[y * W + x] = static_cast<uint8_t>(x * 16);  // 0..240

    b.run();

    // Interior: Gy should be 0 because every column is uniform vertically
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x)
            EXPECT_EQ(b.gy[y * W + x], 0)
                << "Expected Gy=0 for horizontal ramp at (" << x << "," << y << ")";
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. Vertical ramp → Gy constant in interior, Gx ≈ 0
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test VerticalRampOnlyGy
 * @brief Verify vertical ramp produces only Gy, not Gx
 * 
 * When intensity varies only vertically (each column is identical),
 * Gx should be zero everywhere.
 */
TEST(Sobel, VerticalRampOnlyGy) {
    const int W = 16, H = 16;
    SobelBuffers b(W, H);

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            b.src[y * W + x] = static_cast<uint8_t>(y * 16);

    b.run();

    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x)
            EXPECT_EQ(b.gx[y * W + x], 0)
                << "Expected Gx=0 for vertical ramp at (" << x << "," << y << ")";
}

// ──────────────────────────────────────────────────────────────────────────────
// 6. All-zero image → all zero gradients
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test AllZeroImage
 * @brief Verify all-zero image produces zero gradients
 */
TEST(Sobel, AllZeroImage) {
    SobelBuffers b(16, 16, 0);
    b.run();

    for (int i = 0; i < 16 * 16; ++i) {
        EXPECT_EQ(b.gx[i], 0);
        EXPECT_EQ(b.gy[i], 0);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. No crash on minimum valid size (3×3)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test MinimumSizeNoCrash
 * @brief Verify Sobel works with minimum 3×3 image
 * 
 * The Sobel kernel is 3x3, so 3×3 is the smallest valid image size.
 */
TEST(Sobel, MinimumSizeNoCrash) {
    SobelBuffers b(3, 3, 50);
    EXPECT_NO_FATAL_FAILURE(b.run());
}

// ──────────────────────────────────────────────────────────────────────────────
// 8. Gx sign: intensity increases left→right → Gx > 0 at transition
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test GxSignCorrect
 * @brief Verify Gx sign indicates direction of horizontal intensity change
 * 
 * When brighter pixels are to the right, Gx should be positive.
 */
TEST(Sobel, GxSignCorrect) {
    const int W = 8, H = 8;
    SobelBuffers b(W, H);

    // Right side bright
    for (int y = 0; y < H; ++y)
        for (int x = 4; x < W; ++x)
            b.src[y * W + x] = 200;

    b.run();

    // At the boundary column 4, Gx should be positive (bright is to the right)
    for (int y = 1; y < H - 1; ++y)
        EXPECT_GT(b.gx[y * W + 4], 0)
            << "Expected Gx > 0 (bright right) at y=" << y;
}

// ──────────────────────────────────────────────────────────────────────────────
// 9. Gy sign: intensity increases top→bottom → Gy > 0 at transition
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test GySignCorrect
 * @brief Verify Gy sign indicates direction of vertical intensity change
 * 
 * When brighter pixels are below, Gy should be positive.
 */
TEST(Sobel, GySignCorrect) {
    const int W = 8, H = 8;
    SobelBuffers b(W, H);

    for (int y = 4; y < H; ++y)
        for (int x = 0; x < W; ++x)
            b.src[y * W + x] = 200;

    b.run();

    for (int x = 1; x < W - 1; ++x)
        EXPECT_GT(b.gy[4 * W + x], 0)
            << "Expected Gy > 0 (bright below) at x=" << x;
}

// ──────────────────────────────────────────────────────────────────────────────
// 10. Antisymmetry: flipping image horizontally negates Gx
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test FlippedImageNegatesGx
 * @brief Verify Gx antisymmetry property under horizontal flip
 * 
 * Sobel operator is antisymmetric: flipping the image horizontally
 * should negate the Gx output.
 */
TEST(Sobel, FlippedImageNegatesGx) {
    const int W = 16, H = 16;
    SobelBuffers a(W, H), b(W, H);

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            uint8_t val = static_cast<uint8_t>((x * 17 + y * 3) % 256);
            a.src[y * W + x] = val;
            b.src[y * W + (W - 1 - x)] = val;   // horizontally mirrored
        }

    a.run(); b.run();

    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) {
            int ax = x, bx = W - 1 - x;
            EXPECT_EQ(a.gx[y * W + ax], -b.gx[y * W + bx])
                << "Antisymmetry broken at (" << x << "," << y << ")";
        }
}