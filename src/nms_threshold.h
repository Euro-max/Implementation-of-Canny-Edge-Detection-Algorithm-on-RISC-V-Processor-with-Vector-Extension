/**
 * @file nms_threshold.h
 * @brief Header file for Non-Maximum Suppression, Thresholding, and Hysteresis.
 */

#ifndef NMS_THRESHOLD_H
#define NMS_THRESHOLD_H

#include <stdint.h>

/**
 * @brief Thins edges by suppressing pixels that are not local maxima along the gradient direction.
 * @param mag      Input gradient magnitude buffer
 * @param dir      Input gradient direction buffer (quantized 0-3)
 * @param nms_out  Output buffer for suppressed edges
 * @param W        Image width
 * @param H        Image height
 */
void apply_nms(const uint8_t* mag, const uint8_t* dir, uint8_t* nms_out, int W, int H);

/**
 * @brief Classifies pixels into Strong, Weak, or Non-edge based on thresholds.
 * @param nms         Input image after NMS
 * @param thresh_out  Output buffer (255=Strong, 128=Weak, 0=None)
 * @param W           Image width
 * @param H           Image height
 * @param low         Low threshold value
 * @param high        High threshold value
 */
void apply_double_threshold(const uint8_t* nms, uint8_t* thresh_out, int W, int H, uint8_t low, uint8_t high);

/**
 * @brief Promotes weak edges to strong edges if they are connected to strong edges.
 * @param thresh  Buffer containing thresholded edges (in-place modification)
 * @param W       Image width
 * @param H       Image height
 */
void apply_hysteresis(uint8_t* thresh, int W, int H);

#endif // NMS_THRESHOLD_H