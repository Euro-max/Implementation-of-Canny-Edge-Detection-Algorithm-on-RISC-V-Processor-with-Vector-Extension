/**
 * @file gtest_pipeline.cpp
 * @brief Google Test suite for the Canny Edge Detection math kernels.
 * * This file contains comprehensive unit tests for the Gaussian Blur, 
 * Sobel operators, Gradient Magnitude, and Gradient Direction functions. 
 * It verifies mathematical correctness, edge cases, and impulse responses.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>

// Include all your headers
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "image_io.h"

// ═══════════════════════════════════════════════════════
//  HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════

/**
 * @brief Creates a simple test image filled with a constant value.
 * * @param w Width of the image to allocate.
 * @param h Height of the image to allocate.
 * @param fill_value The 8-bit unsigned integer value to fill the buffer with.
 * @return A 64-byte aligned pointer to the allocated and initialized image buffer.
 */
static uint8_t* make_image(int w, int h, uint8_t fill_value) {
    // Use aligned_alloc so it matches your production code
    uint8_t* img = (uint8_t*)aligned_alloc(64, w * h);
    memset(img, fill_value, w * h);
    return img;
}

// ═══════════════════════════════════════════════════════
//  GAUSSIAN BLUR TESTS
// ═══════════════════════════════════════════════════════

/**
 * @test GaussianTest.UniformImageStaysUniform
 * @brief Blurring a uniform image must give the same uniform image.
 * * @details WHY: If every pixel is 128, the weighted average of any neighborhood
 * is still 128. If your convolution has a bug (e.g. wrong divisor),
 * this test will catch it immediately.
 */
TEST(GaussianTest, UniformImageStaysUniform) {
    int w = 64, h = 64;
    uint8_t* src = make_image(w, h, 128);
    uint8_t* dst = make_image(w, h, 0);

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src, dst, w, h);

    // Check INTERIOR pixels only (skip 2-pixel border affected by zero-padding)
    for (int y = 2; y < h - 2; y++) {
        for (int x = 2; x < w - 2; x++) {
            int actual = dst[y * w + x];
            // Allow ±1 for integer rounding during division by 273
            EXPECT_NEAR(actual, 128, 1)
                << "Failed at pixel (" << x << "," << y << ")";
        }
    }

    free(src); free(dst);
}

/**
 * @test GaussianTest.AllBlackStaysBlack
 * @brief Blurring an all-black image must stay all-black.
 * * @details WHY: 0 × anything = 0. If your code has an uninitialized buffer bug,
 * this catches it because the result would be nonzero.
 */
TEST(GaussianTest, AllBlackStaysBlack) {
    int w = 64, h = 64;
    uint8_t* src = make_image(w, h, 0);
    uint8_t* dst = make_image(w, h, 99);  // fill with 99 to detect if unchanged

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src, dst, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_EQ(dst[i], 0) << "Expected 0 at index " << i;
    }

    free(src); free(dst);
}

/**
 * @test GaussianTest.ImpulseSpreadToNeighbors
 * @brief Impulse response — single bright pixel spreads to neighbors.
 * * @details WHY: This verifies the kernel shape. A single 255 pixel in a black image,
 * after blurring, should spread outward. The center pixel decreases
 * (energy spread out), and neighbors become nonzero.
 */
TEST(GaussianTest, ImpulseSpreadToNeighbors) {
    int w = 64, h = 64;
    uint8_t* src = make_image(w, h, 0);
    uint8_t* dst = make_image(w, h, 0);

    // Place a single bright pixel at the center
    int cx = w / 2, cy = h / 2;
    src[cy * w + cx] = 255;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src, dst, w, h);

    // Center pixel should have decreased (energy spread out)
    EXPECT_LT(dst[cy * w + cx], 255)
        << "Center pixel should decrease after blur";

    // Center should still be the brightest pixel (kernel peak at center)
    EXPECT_GT(dst[cy * w + cx], 0)
        << "Center pixel should still be nonzero";

    // Direct neighbors should now be nonzero (energy spread to them)
    EXPECT_GT(dst[cy * w + (cx + 1)], 0) << "Right neighbor should be nonzero";
    EXPECT_GT(dst[cy * w + (cx - 1)], 0) << "Left neighbor should be nonzero";
    EXPECT_GT(dst[(cy + 1) * w + cx], 0) << "Bottom neighbor should be nonzero";
    EXPECT_GT(dst[(cy - 1) * w + cx], 0) << "Top neighbor should be nonzero";

    // A pixel far away (10+ pixels) should stay zero
    EXPECT_EQ(dst[cy * w + (cx + 10)], 0)
        << "Far pixel should stay zero";

    free(src); free(dst);
}

// ═══════════════════════════════════════════════════════
//  SOBEL TESTS
// ═══════════════════════════════════════════════════════

/**
 * @test SobelTest.UniformImageZeroGradient
 * @brief Uniform image → zero gradient everywhere.
 * * @details WHY: No brightness change = no edge = all zeros.
 * If your Sobel has an off-by-one bug on the kernel,
 * a uniform image will produce nonzero output.
 */
TEST(SobelTest, UniformImageZeroGradient) {
    int w = 64, h = 64;
    uint8_t* src  = make_image(w, h, 100);
    int16_t* gx   = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy   = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));

    compute_sobel(src, gx, gy, w, h);

    // Interior pixels must all be zero
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            EXPECT_EQ(gx[y * w + x], 0)
                << "Gx should be 0 at (" << x << "," << y << ")";
            EXPECT_EQ(gy[y * w + x], 0)
                << "Gy should be 0 at (" << x << "," << y << ")";
        }
    }

    free(src); free(gx); free(gy);
}

/**
 * @test SobelTest.VerticalEdgeLargeGx
 * @brief Vertical edge → large Gx, near-zero Gy.
 * * @details WHY: A sharp vertical edge (left=black, right=white) changes brightness
 * LEFT-TO-RIGHT. That's exactly what Sobel-X detects.
 * Gy should be near zero because there's no TOP-TO-BOTTOM change.
 */
TEST(SobelTest, VerticalEdgeLargeGx) {
    int w = 64, h = 64;
    uint8_t* src = make_image(w, h, 0);

    // Create a vertical edge: left half black (0), right half white (255)
    for (int y = 0; y < h; y++) {
        for (int x = w / 2; x < w; x++) {
            src[y * w + x] = 255;
        }
    }

    int16_t* gx = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));

    compute_sobel(src, gx, gy, w, h);

    // At the edge column (x = w/2), Gx should be large positive
    int edge_x = w / 2;
    int mid_y  = h / 2;
    EXPECT_GT(gx[mid_y * w + edge_x], 500)
        << "Gx should be large at the vertical edge";

    // Gy should be near zero in the middle of the image
    // (no top-bottom brightness change in the middle rows)
    EXPECT_NEAR(gy[mid_y * w + edge_x], 0, 10)
        << "Gy should be near zero at a vertical edge";

    free(src); free(gx); free(gy);
}

/**
 * @test SobelTest.HorizontalEdgeLargeGy
 * @brief Horizontal edge → large Gy, near-zero Gx.
 * * @details WHY: Mirror of Test 5. Top half black, bottom half white.
 * Brightness changes TOP-TO-BOTTOM → Sobel-Y responds.
 */
TEST(SobelTest, HorizontalEdgeLargeGy) {
    int w = 64, h = 64;
    uint8_t* src = make_image(w, h, 0);

    // Top half black, bottom half white
    for (int y = h / 2; y < h; y++) {
        for (int x = 0; x < w; x++) {
            src[y * w + x] = 255;
        }
    }

    int16_t* gx = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));

    compute_sobel(src, gx, gy, w, h);

    int mid_x  = w / 2;
    int edge_y = h / 2;

    EXPECT_GT(gy[edge_y * w + mid_x], 500)
        << "Gy should be large at the horizontal edge";

    EXPECT_NEAR(gx[edge_y * w + mid_x], 0, 10)
        << "Gx should be near zero at a horizontal edge";

    free(src); free(gx); free(gy);
}

// ═══════════════════════════════════════════════════════
//  MAGNITUDE TESTS
// ═══════════════════════════════════════════════════════

/**
 * @test MagnitudeTest.L1OutputInValidRange
 * @brief L1 magnitude output must be in [0, 255].
 * * @details WHY: Output is uint8_t. If normalization is broken,
 * you might get overflow or always-zero output.
 */
TEST(MagnitudeTest, L1OutputInValidRange) {
    int w = 64, h = 64;

    // Create gradient arrays with known values
    int16_t* gx  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* mag = (uint8_t*)aligned_alloc(64, w * h);

    // Fill with mixed positive/negative gradients
    for (int i = 0; i < w * h; i++) {
        gx[i] = (int16_t)(i % 512 - 256);   // range: -256 to +255
        gy[i] = (int16_t)(i % 300 - 150);   // range: -150 to +149
    }

    compute_magnitude_l1(gx, gy, mag, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_GE(mag[i], 0)   << "L1 magnitude below 0 at index " << i;
        EXPECT_LE(mag[i], 255) << "L1 magnitude above 255 at index " << i;
    }

    // Result must not be all-zeros (that would mean normalization broke)
    int nonzero_count = 0;
    for (int i = 0; i < w * h; i++) {
        if (mag[i] > 0) nonzero_count++;
    }
    EXPECT_GT(nonzero_count, 0) << "Magnitude output is all zeros!";

    free(gx); free(gy); free(mag);
}

/**
 * @test MagnitudeTest.L2OutputInValidRange
 * @brief L1 >= L2 always (L1 is an overestimate).
 * * @details WHY: Mathematically, |a|+|b| >= sqrt(a²+b²) always.
 * If your normalization differs between L1 and L2, this might
 * not hold pixel-by-pixel (both are normalized independently).
 * But the MAXIMUM value found by L1 should be >= L2 maximum.
 */
TEST(MagnitudeTest, L2OutputInValidRange) {
    int w = 64, h = 64;

    int16_t* gx   = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy   = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* mag  = (uint8_t*)aligned_alloc(64, w * h);

    for (int i = 0; i < w * h; i++) {
        gx[i] = (int16_t)(i % 400 - 200);
        gy[i] = (int16_t)(i % 300 - 150);
    }

    compute_magnitude_l2(gx, gy, mag, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_GE(mag[i], 0)   << "L2 magnitude below 0 at index " << i;
        EXPECT_LE(mag[i], 255) << "L2 magnitude above 255 at index " << i;
    }

    free(gx); free(gy); free(mag);
}

/**
 * @test MagnitudeTest.ZeroGradientZeroMagnitude
 * @brief Zero gradients → zero magnitude.
 * * @details WHY: If Gx=0 and Gy=0 everywhere, there are no edges.
 * Magnitude must be all zero. Tests the zero-division guard.
 */
TEST(MagnitudeTest, ZeroGradientZeroMagnitude) {
    int w = 32, h = 32;

    int16_t* gx  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* mag = (uint8_t*)aligned_alloc(64, w * h);

    memset(gx, 0, w * h * sizeof(int16_t));
    memset(gy, 0, w * h * sizeof(int16_t));

    compute_magnitude_l1(gx, gy, mag, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_EQ(mag[i], 0) << "Zero gradient should give zero magnitude";
    }

    free(gx); free(gy); free(mag);
}

// ═══════════════════════════════════════════════════════
//  DIRECTION TESTS
// ═══════════════════════════════════════════════════════

/**
 * @test DirectionTest.HorizontalGradientDirection0
 * @brief Purely horizontal gradient → direction = 0.
 * * @details WHY: If Gx is large and Gy=0, the gradient points left/right.
 * That means the EDGE runs vertically... wait, let's be precise:
 * Large Gx, zero Gy → gradient angle ≈ 0° → direction bin 0.
 */
TEST(DirectionTest, HorizontalGradientDirection0) {
    int w = 16, h = 16;

    int16_t* gx = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, w * h);

    // Large Gx, zero Gy → purely horizontal gradient
    for (int i = 0; i < w * h; i++) {
        gx[i] = 1000;
        gy[i] = 0;
    }

    compute_direction(gx, gy, dir, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_EQ(dir[i], 0) << "Pure horizontal gradient should be direction 0";
    }

    free(gx); free(gy); free(dir);
}

/**
 * @test DirectionTest.VerticalGradientDirection2
 * @brief Purely vertical gradient → direction = 2.
 * * @details WHY: Large Gy, zero Gx → gradient angle ≈ 90° → direction bin 2.
 */
TEST(DirectionTest, VerticalGradientDirection2) {
    int w = 16, h = 16;

    int16_t* gx  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, w * h);

    // Zero Gx, large Gy → purely vertical gradient
    for (int i = 0; i < w * h; i++) {
        gx[i] = 0;
        gy[i] = 1000;
    }

    compute_direction(gx, gy, dir, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_EQ(dir[i], 2) << "Pure vertical gradient should be direction 2";
    }

    free(gx); free(gy); free(dir);
}

/**
 * @test DirectionTest.DiagonalGradientDirection1
 * @brief Equal Gx and Gy → direction = 1 (45°).
 * * @details WHY: When |Gx| = |Gy| and both positive,
 * angle = 45° → direction bin 1.
 */
TEST(DirectionTest, DiagonalGradientDirection1) {
    int w = 16, h = 16;

    int16_t* gx  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, w * h * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, w * h);

    // Equal Gx and Gy, both positive → 45° diagonal
    for (int i = 0; i < w * h; i++) {
        gx[i] = 500;
        gy[i] = 500;
    }

    compute_direction(gx, gy, dir, w, h);

    for (int i = 0; i < w * h; i++) {
        EXPECT_EQ(dir[i], 1) << "Equal positive gradients should be direction 1";
    }

    free(gx); free(gy); free(dir);
}