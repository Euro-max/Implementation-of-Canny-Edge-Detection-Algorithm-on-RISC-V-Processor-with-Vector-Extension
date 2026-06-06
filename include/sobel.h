#ifndef SOBEL_H
#define SOBEL_H

#include <cstdint>

// Uses int16_t for gradients as it's sufficient for 8-bit input [cite: 76, 77]
void compute_sobel(const uint8_t* input, int16_t* gx, int16_t* gy, int width, int height);

#endif
