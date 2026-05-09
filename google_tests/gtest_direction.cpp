/**
 * @file gtest_direction.cpp
 * @brief Google Test suite for gradient direction quantization
 * @ingroup tests
 * 
 * Tests the compute_direction() function which quantizes gradient orientation
 * into 4 bins (0°, 45°, 90°, 135°). Uses Google Test framework for assertions.
 * 
 * Direction conventions:
 * - 0 → 0°   (horizontal gradient / vertical edge)
 * - 1 → 45°  (diagonal edge, Gx/Gy same sign)
 * - 2 → 90°  (vertical gradient / horizontal edge)
 * - 3 → 135° (diagonal edge, Gx/Gy opposite signs)
 * 
 * @see compute_direction()
 */

#include <gtest/gtest.h>
#include "direction.h"
#include <vector>
#include <cstdint>
#include <cmath>

// ──────────────────────────────────────────────────────────────────────────────
// Direction conventions (match your implementation):
//   0 → 0°   (horizontal)
//   1 → 45°  (diagonal /)
//   2 → 90°  (vertical)
//   3 → 135° (diagonal \)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @struct DirBuffers
 * @brief Test helper structure containing gradient and direction buffers
 * 
 * Manages test data for direction computation tests, providing convenient
 * initialization and execution methods.
 */
struct DirBuffers {
    int W, H;                        ///< Image width and height
    std::vector<int16_t>  gx, gy;    ///< Horizontal/vertical gradient buffers
    std::vector<uint8_t>  dir;       ///< Output direction buffer

    /**
     * @brief Construct buffers for given dimensions
     * @param w Image width in pixels
     * @param h Image height in pixels
     */
    DirBuffers(int w, int h)
        : W(w), H(h), gx(w * h, 0), gy(w * h, 0), dir(w * h, 99) {}

    /**
     * @brief Execute direction computation on current buffers
     */
    void run() { compute_direction(gx.data(), gy.data(), dir.data(), W, H); }
};

// ──────────────────────────────────────────────────────────────────────────────
// 1. Pure horizontal gradient → direction 0
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test HorizontalGradientDir0
 * @brief Verify pure horizontal gradient quantizes to direction 0
 * 
 * When Gy = 0 and Gx ≠ 0, the gradient is purely horizontal,
 * which should be quantized to bin 0 (vertical edge).
 */
TEST(Direction, HorizontalGradientDir0) {
    DirBuffers b(4, 4);
    for (int i = 0; i < 4 * 4; ++i) { b.gx[i] = 100; b.gy[i] = 0; }
    b.run();

    for (int i = 0; i < 4 * 4; ++i)
        EXPECT_EQ(b.dir[i], 0) << "Expected dir=0 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. Pure vertical gradient → direction 2
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test VerticalGradientDir2
 * @brief Verify pure vertical gradient quantizes to direction 2
 * 
 * When Gx = 0 and Gy ≠ 0, the gradient is purely vertical,
 * which should be quantized to bin 2 (horizontal edge).
 */
TEST(Direction, VerticalGradientDir2) {
    DirBuffers b(4, 4);
    for (int i = 0; i < 4 * 4; ++i) { b.gx[i] = 0; b.gy[i] = 100; }
    b.run();

    for (int i = 0; i < 4 * 4; ++i)
        EXPECT_EQ(b.dir[i], 2) << "Expected dir=2 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. 45° gradient (Gx == Gy > 0) → direction 1
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test Diagonal45Dir1
 * @brief Verify 45° diagonal gradient quantizes to direction 1
 * 
 * When Gx = Gy > 0, the gradient points at 45°,
 * which should be quantized to bin 1.
 */
TEST(Direction, Diagonal45Dir1) {
    DirBuffers b(4, 4);
    for (int i = 0; i < 4 * 4; ++i) { b.gx[i] = 100; b.gy[i] = 100; }
    b.run();

    for (int i = 0; i < 4 * 4; ++i)
        EXPECT_EQ(b.dir[i], 1) << "Expected dir=1 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 4. 135° gradient (Gx = −Gy) → direction 3
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test Diagonal135Dir3
 * @brief Verify 135° diagonal gradient quantizes to direction 3
 * 
 * When Gx = -Gy, the gradient points at 135°,
 * which should be quantized to bin 3.
 */
TEST(Direction, Diagonal135Dir3) {
    DirBuffers b(4, 4);
    for (int i = 0; i < 4 * 4; ++i) { b.gx[i] = -100; b.gy[i] = 100; }
    b.run();

    for (int i = 0; i < 4 * 4; ++i)
        EXPECT_EQ(b.dir[i], 3) << "Expected dir=3 at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. All-zero gradients → direction is defined (no crash, value in [0,3])
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test ZeroGradientsNoCrash
 * @brief Verify zero gradients don't cause crashes and produce valid output
 * 
 * Edge case: When both gradients are zero, the function should still
 * produce a valid direction (implementation-dependent, but within [0,3]).
 */
TEST(Direction, ZeroGradientsNoCrash) {
    DirBuffers b(8, 8);
    EXPECT_NO_FATAL_FAILURE(b.run());

    for (int i = 0; i < 8 * 8; ++i)
        EXPECT_LE(b.dir[i], 3) << "Direction out of range at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 6. Direction is in range [0,3] for arbitrary gradients
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test OutputAlwaysInRange
 * @brief Verify output is always in valid range [0,3] for arbitrary inputs
 * 
 * Tests with pseudorandom gradient values to ensure no out-of-bounds
 * outputs occur for any valid input combination.
 */
TEST(Direction, OutputAlwaysInRange) {
    const int W = 16, H = 16;
    DirBuffers b(W, H);

    for (int i = 0; i < W * H; ++i) {
        b.gx[i] = static_cast<int16_t>((i * 37 - 500) % 512);
        b.gy[i] = static_cast<int16_t>((i * 19 + 300) % 512);
    }
    b.run();

    for (int i = 0; i < W * H; ++i)
        EXPECT_LE(b.dir[i], 3) << "Direction out of [0,3] at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. Negative pure horizontal gradient → same direction as positive (0°)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test NegativeHorizontalSameDir
 * @brief Verify direction is sign-invariant for pure horizontal gradients
 * 
 * Edge orientation doesn't depend on gradient polarity, only magnitude.
 * Both positive and negative Gx should produce direction 0.
 */
TEST(Direction, NegativeHorizontalSameDir) {
    DirBuffers a(4, 4), b(4, 4);
    for (int i = 0; i < 4 * 4; ++i) { a.gx[i] =  100; a.gy[i] = 0; }
    for (int i = 0; i < 4 * 4; ++i) { b.gx[i] = -100; b.gy[i] = 0; }
    a.run(); b.run();

    for (int i = 0; i < 4 * 4; ++i)
        EXPECT_EQ(a.dir[i], b.dir[i])
            << "Direction differs for ±Gx at index " << i;
}

// ──────────────────────────────────────────────────────────────────────────────
// 8. Nearly-horizontal gradient is quantised to 0
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test NearlyHorizontalQuantisedTo0
 * @brief Verify small Gy relative to Gx quantizes to horizontal bin
 * 
 * When |Gy|/|Gx| < tan(22.5°) ≈ 0.414, the gradient should be binned as 0°.
 */
TEST(Direction, NearlyHorizontalQuantisedTo0) {
    DirBuffers b(1, 1);
    // Very small Gy relative to Gx → unambiguously horizontal
    b.gx[0] = 100;
    b.gy[0] = 10;   // 10/100 = 0.1, well inside the horizontal sector
    b.run();

    EXPECT_EQ(b.dir[0], 0);
}

// ──────────────────────────────────────────────────────────────────────────────
// 9. Rotational symmetry: rotating gradient by 90° increments shifts direction
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test RotationShiftsDirection
 * @brief Verify 90° rotation produces expected direction shifts
 * 
 * Tests rotational symmetry: 0° → 90° → 180° produce direction 0, 2, 0
 * respectively (180° is same orientation as 0° for edge detection).
 */
TEST(Direction, RotationShiftsDirection) {
    // 0° → dir 0
    { DirBuffers b(1,1); b.gx[0]=100; b.gy[0]=0;   b.run(); EXPECT_EQ(b.dir[0], 0); }
    // 90° → dir 2
    { DirBuffers b(1,1); b.gx[0]=0;   b.gy[0]=100; b.run(); EXPECT_EQ(b.dir[0], 2); }
    // 180° → dir 0 (same line as 0°)
    { DirBuffers b(1,1); b.gx[0]=-100;b.gy[0]=0;   b.run(); EXPECT_EQ(b.dir[0], 0); }
}

// ──────────────────────────────────────────────────────────────────────────────
// 10. Single-pixel buffer: no crash
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SinglePixelNoCrash
 * @brief Verify function handles minimal 1x1 image without crashing
 * 
 * Edge case: Smallest possible image size (1 pixel) should be processed
 * without buffer overflows or crashes.
 */
TEST(Direction, SinglePixelNoCrash) {
    DirBuffers b(1, 1);
    b.gx[0] = 50; b.gy[0] = 50;
    EXPECT_NO_FATAL_FAILURE(b.run());
    EXPECT_LE(b.dir[0], 3);
}