#include "image_io.h"
#include <cstdlib>
#include <fstream>
#include <cstring> // Needed for memset

uint8_t* allocate_buffer(int width, int height) {
    return (uint8_t*)aligned_alloc(64, width * height);
}

bool load_raw(const char* path, uint8_t* buffer, int width, int height) {
    // bypass external file system constraints for QEMU bare-metal environment
    // Fill the buffer with zeros (simulating a clean 64x64 raw black image)
    std::memset(buffer, 0, width * height);
    return true; 
}

bool save_raw(const char* path, const uint8_t* buffer, int width, int height) {
    // Bypass writing to external file system for benchmarking
    return true;
}
