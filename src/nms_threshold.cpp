/**
 * @file nms_threshold.cpp
 * @brief Implementation of NMS, Double Thresholding, and Hysteresis.
 */

#include "nms_threshold.h"
#include <stdint.h>
#include <algorithm>

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
 */
static void trace_edge(uint8_t* data, int x, int y, int W, int H) {
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int nx = x + j;
            int ny = y + i;
            if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                if (data[ny * W + nx] == 128) {
                    data[ny * W + nx] = 255;
                    trace_edge(data, nx, ny, W, H);
                }
            }
        }
    }
}

void apply_hysteresis(uint8_t* thresh, int W, int H) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (thresh[y * W + x] == 255) {
                trace_edge(thresh, x, y, W, H);
            }
        }
    }
    // Final pass: cleanup any remaining weak edges
    for (int i = 0; i < W * H; i++) {
        if (thresh[i] == 128) thresh[i] = 0;
    }
}