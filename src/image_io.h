#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include <cstdint>

// Aligned allocation (64-byte) for better SIMD/RVV performance [cite: 59, 60]
uint8_t* allocate_buffer(int width, int height);
bool load_raw(const char* path, uint8_t* buffer, int width, int height);
bool save_raw(const char* path, const uint8_t* buffer, int width, int height);

#endif
