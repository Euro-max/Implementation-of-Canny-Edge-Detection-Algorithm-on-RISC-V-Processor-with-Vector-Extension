#ifndef MAGNITUDE_H
#define MAGNITUDE_H
#include <cstdint>

// L1 norm: |Gx| + |Gy| - faster, no floating point
void compute_magnitude_l1(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

// L2 norm: sqrt(Gx^2 + Gy^2) - more accurate
void compute_magnitude_l2(const int16_t* gx, const int16_t* gy, 
                          uint8_t* output, int width, int height);

#endif
