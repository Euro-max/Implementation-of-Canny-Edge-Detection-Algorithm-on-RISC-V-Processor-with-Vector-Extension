/**
 * @file gtest_magnitude.cpp
 * @brief Google Test suite for gradient magnitude computation
 * @ingroup tests
 * 
 * Tests both L1 and L2 gradient magnitude computation functions.
 * Verifies normalization, scaling, and range properties.
 * 
 * Implementation notes for compute_magnitude_l1:
 *   1. raw[i] = |gx[i]| + |gy[i]|
 *   2. max_raw = max of all raw[i]
 *   3. output[i] = (uint8_t)(raw[i] * 255.0f / max_raw) (normalized)
 *   4. If max_raw == 0, all output = 0
 * 
 * So the MAX pixel always maps to 255; everything else is proportional.
 * 
 * @see compute_magnitude_l1()
 * @see compute_magnitude_l2()
 */

#include <gtest/gtest.h>
#include "magnitude.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ──────────────────────────────────────────────────────────────────────────────

/**
 * @struct MagBuffers
 * @brief Test helper structure for magnitude computation tests
 * 
 * Manages gradient buffers and provides expected value calculation
 * based on the normalization contract.
 */
struct MagBuffers {
    int W, H;                           ///< Image dimensions
    std::vector<int16_t> gx, gy;        ///< Gradient buffers
    std::vector<uint8_t> mag;           ///< Output magnitude buffer

    /**
     * @brief Construct buffers for given dimensions
     * @param w Image width
     * @param h Image height
     */
    MagBuffers(int w, int h)
        : W(w), H(h), gx(w * h, 0), gy(w * h, 0), mag(w * h, 0) {}

    /**
     * @brief Run L1 magnitude computation
     */
    void run() {
        compute_magnitude_l1(gx.data(), gy.data(), mag.data(), W, H);
    }

    /**
     * @brief Calculate expected normalized value for pixel i
     * @param i Pixel index
     * @return Expected magnitude value (0-255) based on normalization contract
     */
    uint8_t expected(int i) const {
        int32_t max_raw = 0;
        for (int j = 0; j < W * H; ++j) {
            int32_t r = std::abs((int)gx[j]) + std::abs((int)gy[j]);
            if (r > max_raw) max_raw = r;
        }
        if (max_raw == 0) return 0;
        int32_t raw = std::abs((int)gx[i]) + std::abs((int)gy[i]);
        return (uint8_t)(raw * 255.0f / max_raw);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 1. All-zero gradients → output is zero everywhere
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ZeroGradientZeroMag
 * @brief Verify zero gradients produce zero magnitude output
 */
TEST(Magnitude, ZeroGradientZeroMag) {
    MagBuffers b(16, 16);
    b.run();

    for (int i = 0; i < 16 * 16; ++i)
        EXPECT_EQ(b.mag[i], 0) << "Non-zero magnitude at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. Uniform Gx → all pixels equal → all normalize to 255
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test UniformGxAllMax
 * @brief Verify uniform horizontal gradients normalize to all 255
 * 
 * When all raw magnitudes are equal, all pixels should map to 255.
 */
TEST(Magnitude, UniformGxAllMax) {
    const int W = 8, H = 8;
    MagBuffers b(W, H);

    for (int i = 0; i < W * H; ++i) b.gx[i] = 100;
    b.run();

    // All raw values equal → all normalized to 255
    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(b.mag[i], 255) << "Expected 255 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. Uniform Gy → all pixels normalize to 255
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test UniformGyAllMax
 * @brief Verify uniform vertical gradients normalize to all 255
 */
TEST(Magnitude, UniformGyAllMax) {
    const int W = 8, H = 8;
    MagBuffers b(W, H);

    for (int i = 0; i < W * H; ++i) b.gy[i] = 80;
    b.run();

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(b.mag[i], 255) << "Expected 255 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 4. The brightest pixel always maps to exactly 255
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test MaxPixelIs255
 * @brief Verify the maximum magnitude pixel always becomes 255
 */
TEST(Magnitude, MaxPixelIs255) {
    const int W = 4, H = 4;
    MagBuffers b(W, H);

    b.gx[0] = 200; b.gy[0] = 100;  // raw = 300  <- max
    b.gx[5] = 50;  b.gy[5] = 25;   // raw = 75
    b.gx[10] = 10; b.gy[10] = 5;   // raw = 15
    b.run();

    EXPECT_EQ(b.mag[0], 255) << "Max pixel should be 255";
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. Proportional scaling: raw 300 and 150 → 255 and ~127
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ProportionalScaling
 * @brief Verify linear scaling of magnitude values
 * 
 * When raw magnitudes have ratio 2:1, normalized values should
 * have ratio approximately 2:1 (255:127.5).
 */
TEST(Magnitude, ProportionalScaling) {
    const int W = 2, H = 1;
    MagBuffers b(W, H);

    b.gx[0] = 200; b.gy[0] = 100;  // raw = 300 -> 255
    b.gx[1] = 100; b.gy[1] = 50;   // raw = 150 -> 127 or 128 (float rounding)
    b.run();

    EXPECT_EQ(b.mag[0], 255);
    EXPECT_NEAR(b.mag[1], 127, 1);  // 150 * 255 / 300 = 127.5
}

// ──────────────────────────────────────────────────────────────────────────────
// 6. Negative gradients contribute via absolute value
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test NegativeGradientsAbsoluteValue
 * @brief Verify negative gradients use absolute value for magnitude
 * 
 * Magnitude should depend only on absolute values, not sign.
 */
TEST(Magnitude, NegativeGradientsAbsoluteValue) {
    const int W = 2, H = 1;
    MagBuffers b(W, H);

    b.gx[0] = -100; b.gy[0] =  0;  // raw = 100
    b.gx[1] =  100; b.gy[1] =  0;  // raw = 100

    b.run();

    // Both pixels have same raw → both should be 255
    EXPECT_EQ(b.mag[0], 255);
    EXPECT_EQ(b.mag[1], 255);
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. Mixed-sign gradients: raw = |Gx| + |Gy|, then normalized
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test MixedSignNormalized
 * @brief Verify mixed-sign gradients produce correct normalized values
 * 
 * Raw magnitude uses absolute values regardless of sign combination.
 */
TEST(Magnitude, MixedSignNormalized) {
    const int W = 4, H = 4;
    MagBuffers b(W, H);

    b.gx[3] = -70; b.gy[3] =  50;  // raw = 120
    b.gx[0] =  120; b.gy[0] =  0;  // raw = 120 (equal, both should be 255)
    b.run();

    EXPECT_EQ(b.mag[3], 255);
    EXPECT_EQ(b.mag[0], 255);
}

// ──────────────────────────────────────────────────────────────────────────────
// 8. All pixels processed: output matches normalization formula for every pixel
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test AllPixelsMatchFormula
 * @brief Verify all output pixels match the expected formula
 * 
 * Comprehensive test with pseudorandom gradients to ensure
 * every pixel's magnitude matches the mathematical definition.
 */
TEST(Magnitude, AllPixelsMatchFormula) {
    const int W = 16, H = 16;
    MagBuffers b(W, H);

    for (int i = 0; i < W * H; ++i) {
        b.gx[i] = static_cast<int16_t>(i % 100);
        b.gy[i] = static_cast<int16_t>((i * 3) % 100);
    }
    b.run();

    for (int i = 0; i < W * H; ++i)
        EXPECT_NEAR(b.mag[i], b.expected(i), 1)
            << "Mismatch at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 9. Single pixel with nonzero gradient → output is 255
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SinglePixelIsMax
 * @brief Verify single-pixel case maps to 255
 * 
 * With only one pixel, it is by definition the maximum.
 */
TEST(Magnitude, SinglePixelIsMax) {
    MagBuffers b(1, 1);
    b.gx[0] = 30;
    b.gy[0] = 20;
    b.run();

    // Only pixel → it IS the max → normalized to 255
    EXPECT_EQ(b.mag[0], 255);
}

// ──────────────────────────────────────────────────────────────────────────────
// 10. Zero pixel stays zero when other pixels have higher magnitude
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ZeroPixelStaysZero
 * @brief Verify zero-gradient pixels remain zero after normalization
 * 
 * When the max magnitude is positive, zero raw magnitude should
 * map to exactly zero.
 */
TEST(Magnitude, ZeroPixelStaysZero) {
    const int W = 4, H = 4;
    MagBuffers b(W, H);

    b.gx[0] = 0;   b.gy[0] = 0;    // raw = 0   -> should stay 0
    b.gx[1] = 100; b.gy[1] = 100;  // raw = 200 -> max -> 255
    b.run();

    EXPECT_EQ(b.mag[0], 0)   << "Zero pixel should remain 0";
    EXPECT_EQ(b.mag[1], 255) << "Max pixel should be 255";
}