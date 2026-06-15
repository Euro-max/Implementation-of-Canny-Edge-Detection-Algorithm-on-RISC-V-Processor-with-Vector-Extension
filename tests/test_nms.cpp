/**
 * @file test_nms.cpp
 * @brief Assert-based tests for NMS, Double Thresholding, and Hysteresis.
 */

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "nms_threshold.h"

// Helper to fill a buffer
void fill_buffer(uint8_t* buf, int W, int H, uint8_t val) {
    for(int i = 0; i < W*H; i++) buf[i] = val;
}

// Test 1: Double Thresholding Logic
void test_threshold() {
    int W = 4, H = 4;
    uint8_t input[] = { 10, 50, 100, 200,
                        10, 50, 100, 200,
                        10, 50, 100, 200,
                        10, 50, 100, 200 };
    uint8_t output[16];
    
    // Low: 40, High: 150
    apply_double_threshold(input, output, W, H, 40, 150);
    
    // Check: <40 (0), 40-149 (128), >=150 (255)
    assert(output[0] == 0);   // 10
    assert(output[1] == 128); // 50
    assert(output[2] == 128); // 100
    assert(output[3] == 255); // 200
    
    printf("* Test 1 PASSED: Double Thresholding\n");
}

// Test 2: NMS Logic (Basic)
void test_nms_suppression() {
    int W = 3, H = 3;
    // Single peak in the center
    uint8_t mag[] = { 10, 10, 10,
                      10, 50, 10,
                      10, 10, 10 };
    uint8_t dir[] = { 0, 0, 0,
                      0, 2, 0, // Dir 2 is Vertical
                      0, 0, 0 };
    uint8_t nms[9];
    
    apply_nms(mag, dir, nms, W, H);
    
    // Center should be preserved
    assert(nms[4] == 50);
    // Everything else should be suppressed (0)
    for(int i = 0; i < 9; i++) {
        if(i != 4) assert(nms[i] == 0);
    }
    printf("* Test 2 PASSED: NMS Keeps Local Maximum\n");
}

// Test 3: Hysteresis Logic
void test_hysteresis() {
    int W = 5, H = 5;
    // Create a 5x5 grid initialized to 0
    uint8_t data[25] = {0};
    
    // Strong edge at center
    data[12] = 255; 
    
    // Connected weak edge (neighbor to center)
    data[11] = 128; 
    
    // Isolated weak edge (far away)
    data[24] = 128; 
    
    apply_hysteresis(data, W, H);
    
    // Connected edge should be promoted
    assert(data[11] == 255);
    
    // Isolated edge should be suppressed (final pass converts 128 -> 0)
    assert(data[24] == 0);
    
    printf("* Test 3 PASSED: Hysteresis Logic\n");
}int main() {
    printf("=== NMS & Threshold Tests ===\n");
    test_threshold();
    test_nms_suppression();
    test_hysteresis();
    printf("All NMS tests PASSED!\n");
    return 0;
}