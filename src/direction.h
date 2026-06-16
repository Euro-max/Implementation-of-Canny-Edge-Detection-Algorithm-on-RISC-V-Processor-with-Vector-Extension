#ifndef DIRECTION_H
#define DIRECTION_H

#include <cstdint>

/**
 * Quantizes gradient direction into 4 bins:
 * 0: 0 degrees (Horizontal edge / Vertical gradient)
 * 1: 45 degrees
 * 2: 90 degrees (Vertical edge / Horizontal gradient)
 * 3: 135 degrees
 */
void compute_direction(const int16_t* gx, const int16_t* gy, uint8_t* output, int width, int height);

#endif
