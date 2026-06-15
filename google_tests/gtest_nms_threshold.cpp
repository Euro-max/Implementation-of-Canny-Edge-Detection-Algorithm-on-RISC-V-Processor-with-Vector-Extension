/**
 * @file    test_nms_threshold.cpp
 * @brief   Google Test suite for Non-Maximum Suppression and Thresholding stages.
 * @details Validates the post-processing components of the Canny Edge Detection
 * pipeline on the host machine. Ensures edges are properly thinned, categorized, 
 * and linked before moving to QEMU deployment.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include "nms_threshold.h"

/**
 * @brief Test Suite for Double Thresholding
 * @details Verifies that pixels are correctly categorized into STRONG (255), 
 * WEAK (128), and ZERO (0) based on the provided low and high thresholds.
 */
TEST(ThresholdTests, CategorizesPixelsCorrectly) {
    int W = 4, H = 1;
    uint8_t in[4]  = {20, 80, 150, 200};
    uint8_t out[4] = {0};
    
    // Set thresholds: low=50, high=150
    apply_double_threshold(in, out, W, H, 50, 150);
    
    EXPECT_EQ(out[0], 0)   << "Pixel below low threshold should be suppressed to 0.";
    EXPECT_EQ(out[1], 128) << "Pixel between thresholds should be WEAK (128).";
    EXPECT_EQ(out[2], 255) << "Pixel exactly on high threshold should be STRONG (255).";
    EXPECT_EQ(out[3], 255) << "Pixel above high threshold should be STRONG (255).";
}

/**
 * @brief Test Suite for Non-Maximum Suppression (NMS)
 * @details Validates that the NMS algorithm correctly preserves local maximums 
 * and suppresses non-maximums along the specified gradient direction.
 */
TEST(NMSTests, KeepsLocalMaximum) {
    int W = 3, H = 3;
    // Center pixel (idx 4) is 100, left is 50, right is 50
    uint8_t mag[9] = {
        0,  0,  0,
        50, 100, 50,
        0,  0,  0
    };
    uint8_t dir[9] = {0}; // 0 degrees (horizontal)
    uint8_t out[9] = {0};

    apply_nms(mag, dir, out, W, H);

    // Center pixel is > neighbors, must be preserved
    EXPECT_EQ(out[4], 100); 
}

TEST(NMSTests, SuppressesNonMaximum) {
    int W = 3, H = 3;
    // Center pixel is 100, but left pixel is larger (150)
    uint8_t mag[9] = {
        0,   0,   0,
        150, 100, 50,
        0,   0,   0
    };
    uint8_t dir[9] = {0}; // 0 degrees (horizontal)
    uint8_t out[9] = {0};

    apply_nms(mag, dir, out, W, H);

    // Center pixel is NOT >= left neighbor, must be crushed to 0
    EXPECT_EQ(out[4], 0); 
}

/**
 * @brief Test Suite for Hysteresis Edge Tracking
 * @details Ensures the hysteresis algorithm accurately promotes weak edges 
 * connected to strong edges, and deletes isolated weak edges.
 */
TEST(HysteresisTests, PromotesConnectedWeakEdges) {
    int W = 3, H = 3;
    // Center is WEAK (128), Top-Left is STRONG (255)
    uint8_t img[9] = {
        255, 0,   0,
        0,   128, 0,
        0,   0,   0
    };

    apply_hysteresis(img, W, H);

    // Center touches a 255, should be promoted
    EXPECT_EQ(img[4], 255); 
}

TEST(HysteresisTests, SuppressesIsolatedWeakEdges) {
    int W = 3, H = 3;
    // Center is WEAK (128), all neighbors are 0
    uint8_t img[9] = {
        0, 0,   0,
        0, 128, 0,
        0, 0,   0
    };

    apply_hysteresis(img, W, H);

    // Center is isolated, should be suppressed
    EXPECT_EQ(img[4], 0); 
}