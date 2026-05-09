/**
 * @file image_io.cpp
 * @brief Implementation of raw image I/O functions
 * @ingroup io
 * 
 * Implements aligned memory allocation and raw binary file operations
 * for 8-bit grayscale images.
 */

#include "image_io.h"
#include <cstdlib>
#include <fstream>

/**
 * @brief Allocates 64-byte aligned buffer using aligned_alloc
 * @param width   Image width
 * @param height  Image height
 * @return        Aligned buffer pointer
 */
uint8_t* allocate_buffer(int width, int height) {
    return (uint8_t*)aligned_alloc(64, width * height);
}

/**
 * @brief Loads raw binary image file
 * @param path    File path
 * @param buffer  Destination buffer
 * @param width   Image width
 * @param height  Image height
 * @return        Success status
 */
bool load_raw(const char* path, uint8_t* buffer, int width, int height) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read((char*)buffer, width * height);
    return !f.fail();  // good() fails at EOF, fail() is more reliable
}

/**
 * @brief Saves raw binary image file
 * @param path    File path
 * @param buffer  Source buffer
 * @param width   Image width
 * @param height  Image height
 * @return        Success status
 */
bool save_raw(const char* path, const uint8_t* buffer, int width, int height) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((char*)buffer, width * height);
    return !f.fail();
}