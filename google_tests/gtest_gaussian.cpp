/**
 * @file gtest_gaussian.cpp
 * @brief Google Test suite for 5x5 Gaussian blur
 * @ingroup tests
 * 
 * Tests the gaussian_blur_5x5() template function which applies
 * a 5x5 Gaussian kernel for image smoothing. Verifies properties
 * like constant preservation, energy conservation, and symmetry.
 * 
 * @see gaussian_blur_5x5()
 */

#include <gtest/gtest.h>
#include "gaussian.h"
#include <vector>
#include <cstdint>
#include <numeric>
#include <cstring>

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @brief Create a buffer filled with a constant value
 * @param W Image width
 * @param H Image height
 * @param fill Fill value (0-255)
 * @return Vector of size W×H with all elements set to fill
 */
static std::vector<uint8_t> make_buf(int W, int H, uint8_t fill = 0) {
    return std::vector<uint8_t>(W * H, fill);
}

// ──────────────────────────────────────────────────────────────────────────────
// 1. Constant image → output equals the same constant (kernel sums to 1)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ConstantImagePreserved
 * @brief Verify constant image remains constant after blur
 * 
 * Since the kernel sums to 1, a constant input should produce
 * the same constant output (within interior region).
 */
TEST(GaussianBlur, ConstantImagePreserved) {
    const int W = 16, H = 16;
    auto src = make_buf(W, H, 128);
    auto dst = make_buf(W, H, 0);

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    // Interior pixels must equal the constant; border handling may differ.
    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x)
            EXPECT_EQ(dst[y * W + x], 128)
                << "Mismatch at (" << x << "," << y << ")";
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. All-zero image stays zero
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ZeroImageStaysZero
 * @brief Verify zero input produces zero output
 * 
 * Convolution with zero-padding should preserve zeros everywhere.
 */
TEST(GaussianBlur, ZeroImageStaysZero) {
    const int W = 16, H = 16;
    auto src = make_buf(W, H, 0);
    auto dst = make_buf(W, H, 255);   // pre-fill with non-zero

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(dst[i], 0) << "Non-zero output at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. All-255 image stays 255 in the interior
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test AllMaxInteriorPreserved
 * @brief Verify maximum-intensity image remains max in interior
 * 
 * Interior pixels should remain 255 as kernel sum = 1 and no overflow.
 */
TEST(GaussianBlur, AllMaxInteriorPreserved) {
    const int W = 20, H = 20;
    auto src = make_buf(W, H, 255);
    auto dst = make_buf(W, H, 0);

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x)
            EXPECT_EQ(dst[y * W + x], 255)
                << "Mismatch at (" << x << "," << y << ")";
}

// ──────────────────────────────────────────────────────────────────────────────
// 4. Single hot pixel gets spread (blur reduces peak)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test HotPixelGetsSpreaded
 * @brief Verify a single bright pixel spreads energy to neighbors
 * 
 * After blur, the peak should be reduced and energy distributed
 * to adjacent pixels.
 */
TEST(GaussianBlur, HotPixelGetsSpreaded) {
    const int W = 16, H = 16;
    auto src = make_buf(W, H, 0);
    auto dst = make_buf(W, H, 0);

    // Place a single bright pixel at the centre
    src[8 * W + 8] = 255;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    // Peak must be strictly less than the original
    EXPECT_LT(dst[8 * W + 8], 255);

    // Energy must have spread to neighbours
    EXPECT_GT(dst[8 * W + 9], 0);
    EXPECT_GT(dst[9 * W + 8], 0);
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. Output is symmetric when input is symmetric
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SymmetricInputGivesSymmetricOutput
 * @brief Verify Gaussian blur preserves input symmetry
 * 
 * The blur kernel is symmetric, so symmetric input should
 * produce symmetric output.
 */
TEST(GaussianBlur, SymmetricInputGivesSymmetricOutput) {
    const int W = 16, H = 16;
    auto src = make_buf(W, H, 0);
    auto dst = make_buf(W, H, 0);

    // Symmetric ring at distance 4 from each edge
    for (int i = 4; i < 12; ++i) {
        src[4 * W + i] = 200;
        src[11 * W + i] = 200;
        src[i * W + 4] = 200;
        src[i * W + 11] = 200;
    }

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    // Horizontal symmetry: dst[y][x] == dst[y][W-1-x]
    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x)
            EXPECT_EQ(dst[y * W + x], dst[y * W + (W - 1 - x)])
                << "Horizontal symmetry broken at (" << x << "," << y << ")";
}

// ──────────────────────────────────────────────────────────────────────────────
// 6. Minimum image size (5×5) doesn't crash
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test MinimumSizeNoCrash
 * @brief Verify function handles minimum 5×5 image without crashing
 * 
 * The kernel is 5x5, so 5x5 is the smallest valid image size.
 */
TEST(GaussianBlur, MinimumSizeNoCrash) {
    const int W = 5, H = 5;
    auto src = make_buf(W, H, 100);
    auto dst = make_buf(W, H, 0);

    auto call1 = [&]{ gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H); };
    EXPECT_NO_FATAL_FAILURE(call1());
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. Output pixel is strictly less than 255 when only a few pixels are lit
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test OutputDoesNotOverflow
 * @brief Verify no overflow occurs for sparse bright pixels
 * 
 * Even with a single 255 pixel, the blur should not exceed 255
 * due to kernel normalization.
 */
TEST(GaussianBlur, OutputDoesNotOverflow) {
    const int W = 32, H = 32;
    auto src = make_buf(W, H, 0);
    auto dst = make_buf(W, H, 0);

    // Set only one pixel to max
    src[16 * W + 16] = 255;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    for (int i = 0; i < W * H; ++i)
        EXPECT_LE(dst[i], 255) << "Overflow at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 8. Non-square image (wide)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test NonSquareWideImage
 * @brief Verify function works with wide (horizontal rectangle) images
 * 
 * Tests aspect ratio where width > height.
 */
TEST(GaussianBlur, NonSquareWideImage) {
    const int W = 32, H = 8;
    auto src = make_buf(W, H, 64);
    auto dst = make_buf(W, H, 0);

    auto call2 = [&]{ gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H); };
    EXPECT_NO_FATAL_FAILURE(call2());

    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x)
            EXPECT_EQ(dst[y * W + x], 64);
}

// ──────────────────────────────────────────────────────────────────────────────
// 9. Non-square image (tall)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test NonSquareTallImage
 * @brief Verify function works with tall (vertical rectangle) images
 * 
 * Tests aspect ratio where height > width.
 */
TEST(GaussianBlur, NonSquareTallImage) {
    const int W = 8, H = 32;
    auto src = make_buf(W, H, 200);
    auto dst = make_buf(W, H, 0);

    auto call3 = [&]{ gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H); };
    EXPECT_NO_FATAL_FAILURE(call3());

    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x)
            EXPECT_EQ(dst[y * W + x], 200);
}

// ──────────────────────────────────────────────────────────────────────────────
// 10. Blurring does not raise average brightness of a dark image
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test BlurDoesNotRaiseMean
 * @brief Verify energy conservation (mean brightness approximately preserved)
 * 
 * Since the kernel sums to 1, the total energy (sum of pixel values)
 * should be approximately conserved (within rounding error).
 */
TEST(GaussianBlur, BlurDoesNotRaiseMean) {
    const int W = 32, H = 32;
    auto src = make_buf(W, H, 0);
    auto dst = make_buf(W, H, 0);

    // Sparse bright pixels
    src[5 * W + 5]   = 200;
    src[20 * W + 20] = 200;

    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(src.data(), dst.data(), W, H);

    long sum_src = 0, sum_dst = 0;
    for (int i = 0; i < W * H; ++i) { sum_src += src[i]; sum_dst += dst[i]; }

    // Mean should be approximately conserved (within rounding)
    EXPECT_NEAR(sum_dst, sum_src, sum_src * 0.05 + 10);
}