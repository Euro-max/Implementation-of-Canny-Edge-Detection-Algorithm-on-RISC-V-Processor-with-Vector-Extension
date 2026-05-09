#include "image_io.h"
#include <cstdlib>
#include <fstream>

uint8_t* allocate_buffer(int width, int height) {
    return (uint8_t*)aligned_alloc(64, width * height);
}

bool load_raw(const char* path, uint8_t* buffer, int width, int height) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read((char*)buffer, width * height);
    return !f.fail();  // good() fails at EOF, fail() is more reliable
}

bool save_raw(const char* path, const uint8_t* buffer, int width, int height) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((char*)buffer, width * height);
    return !f.fail();
}
