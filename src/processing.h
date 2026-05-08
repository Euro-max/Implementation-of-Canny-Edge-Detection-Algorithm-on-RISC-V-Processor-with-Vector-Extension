#ifndef PROCESSING_H
#define PROCESSING_H

#include <cstdint>

uint8_t* allocate_image(int width, int height);
bool load_raw(const char* filename, uint8_t* buffer, int width, int height);
void save_raw(const char* filename, uint8_t* buffer, int width, int height);
void gaussian_blur_scalar(const uint8_t* input, uint8_t* output, int width, int height);

#endif

