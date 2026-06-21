/**
 * @file nms_threshold.cpp
 * @brief Implementation of NMS, Double Thresholding, and Hysteresis.
 */

#include "nms_threshold.h"
#include <stdint.h>
#include <algorithm>
#include <vector>

/**
 * @brief Non-Maximum Suppression
 * Thins edges by suppressing pixels that are not local maxima along the gradient direction.
 */
void apply_nms(const uint8_t* mag, const uint8_t* dir, uint8_t* nms_out, int W, int H) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // Skip boundaries (set to 0)
            if (x == 0 || x == W - 1 || y == 0 || y == H - 1) {
                nms_out[y * W + x] = 0;
                continue;
            }

            int idx = y * W + x;
            uint8_t direction = dir[idx];
            uint8_t m = mag[idx];
            uint8_t m1 = 0, m2 = 0;

            // Check neighbors based on quantized direction
            if (direction == 0) { // Horizontal
                m1 = mag[idx - 1]; m2 = mag[idx + 1];
            } else if (direction == 1) { // 45 degrees
                m1 = mag[(y - 1) * W + (x + 1)]; m2 = mag[(y + 1) * W + (x - 1)];
            } else if (direction == 2) { // Vertical
                m1 = mag[(y - 1) * W + x]; m2 = mag[(y + 1) * W + x];
            } else { // 135 degrees
                m1 = mag[(y - 1) * W + (x - 1)]; m2 = mag[(y + 1) * W + (x + 1)];
            }

            if (m >= m1 && m >= m2) nms_out[idx] = m;
            else nms_out[idx] = 0;
        }
    }
}

/**
 * @brief Double Thresholding
 * Classifies pixels: 255 (Strong), 128 (Weak), 0 (Non-edge)
 */
void apply_double_threshold(const uint8_t* nms, uint8_t* thresh_out, int W, int H, uint8_t low, uint8_t high) {
    for (int i = 0; i < W * H; i++) {
        if (nms[i] >= high)      thresh_out[i] = 255;
        else if (nms[i] >= low)  thresh_out[i] = 128;
        else                     thresh_out[i] = 0;
    }
}

/**
 * @brief Hysteresis
 * Promotes weak edges to strong edges if they are connected to strong edges.
 * Iterative implementation to protect the hardware call stack.
 */
void apply_hysteresis(uint8_t* thresh, int W, int H) {
    // Explicit stack for Depth-First Search. 
    // This moves memory allocation to the heap, preventing bare-metal stack overflows.
    std::vector<int> stack;
    
    // Pre-allocate memory to prevent costly reallocations during the trace
    stack.reserve(1024);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (thresh[y * W + x] == 255) {
                // We found a Strong pixel. Push its 1D index onto the stack to start tracing.
                stack.push_back(y * W + x);

                // Iterative trace (replaces the recursive trace_edge function)
                while (!stack.empty()) {
                    // Pop the current pixel index off the stack
                    int idx = stack.back();
                    stack.pop_back();

                    // Convert 1D index back to 2D coordinates
                    int cx = idx % W;
                    int cy = idx / W;

                    // Inspect all 8 surrounding neighbors
                    for (int i = -1; i <= 1; i++) {
                        for (int j = -1; j <= 1; j++) {
                            int nx = cx + j;
                            int ny = cy + i;

                            // Ensure neighbor is within image boundaries
                            if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                                int n_idx = ny * W + nx;
                                
                                // If the neighbor is a Weak edge, promote it and push it to the stack
                                if (thresh[n_idx] == 128) {
                                    thresh[n_idx] = 255;
                                    stack.push_back(n_idx);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Final pass: cleanup any remaining weak edges that were never connected
    for (int i = 0; i < W * H; i++) {
        if (thresh[i] == 128) thresh[i] = 0;
    }
}
