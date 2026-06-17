#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <cstdint>

// Template design as recommended for professional vision libraries [cite: 67, 68]
template <typename T_in, typename T_out, typename T_acc>
void gaussian_blur_5x5(const T_in* input, T_out* output, int width, int height);

#include "gaussian.ipp" // Implementation of template
#endif
