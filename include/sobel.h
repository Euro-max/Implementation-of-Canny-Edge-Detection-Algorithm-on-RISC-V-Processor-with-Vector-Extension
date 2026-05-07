#ifndef SOBEL_H
#define SOBEL_H

#include "common.h"

// This function takes an input image and fills the 
// magnitudes and angles vectors in the output image.
void applySobel(const Image& input, Image& output);

#endif
