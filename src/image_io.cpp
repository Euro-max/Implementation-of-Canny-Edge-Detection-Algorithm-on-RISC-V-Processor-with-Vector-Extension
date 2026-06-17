/**
 * @file image_io.cpp
 * @brief Implementation of raw image file I/O and aligned memory allocation.
 * * Uses stdio (fopen/fread/fwrite) instead of C++ streams (ifstream/ofstream).
 * WHY: C++ streams require more C++ runtime support which is problematic
 * with riscv64-unknown-elf bare-metal toolchain + newlib.
 */

#include "image_io.h"
#include <stdlib.h>    // aligned_alloc
#include <stdio.h>     // fopen, fread, fwrite, fclose

/**
 * @brief Allocates a 64-byte-aligned buffer for width*height pixels.
 * * 64-byte alignment is required for RVV vector loads/stores to ensure optimal 
 * memory throughput and avoid alignment faults during hardware vectorization.
 * * @param width  The width of the image buffer to allocate.
 * @param height The height of the image buffer to allocate.
 * @return A 64-byte aligned pointer to the dynamically allocated uint8_t buffer.
 */
// Allocate a 64-byte-aligned buffer for width*height pixels
// 64-byte alignment is required for RVV vector loads/stores
uint8_t* allocate_buffer(int width, int height) {
    return (uint8_t*)aligned_alloc(64, width * height);
}

/**
 * @brief Loads a raw grayscale image from disk into an already-allocated buffer.
 * * Reads the raw binary pixel data directly into memory without expecting any 
 * headers. It strictly verifies that the number of bytes read matches the requested dimensions.
 * * @param path   The filepath to the binary image file to read.
 * @param buffer A pointer to the pre-allocated destination memory buffer.
 * @param width  The expected width of the image.
 * @param height The expected height of the image.
 * @return true on success, false if file cannot be opened or is too small.
 */
// Load a raw grayscale image from disk into an already-allocated buffer
// Returns true on success, false if file cannot be opened or is too small
bool load_raw(const char* path, uint8_t* buffer, int width, int height) {
    FILE* f = fopen(path, "rb");    // "rb" = read binary
    if (!f) return false;

    int total = width * height;
    int n = (int)fread(buffer, 1, total, f);
    fclose(f);

    return (n == total);    // true only if we read exactly the right number of bytes
}

/**
 * @brief Saves a raw grayscale image from a buffer to disk.
 * * Writes the raw pixel array out to a flat, headerless binary file. It strictly 
 * verifies that all intended bytes were successfully written to the filesystem.
 * * @param path   The filepath where the binary image file will be created/overwritten.
 * @param buffer A pointer to the source memory buffer to save.
 * @param width  The width of the image.
 * @param height The height of the image.
 * @return true on success, false if file cannot be created or write fails.
 */
// Save a raw grayscale image from buffer to disk
// Returns true on success, false if file cannot be created or write fails
bool save_raw(const char* path, const uint8_t* buffer, int width, int height) {
    FILE* f = fopen(path, "wb");    // "wb" = write binary
    if (!f) return false;

    int total = width * height;
    int n = (int)fwrite(buffer, 1, total, f);
    fclose(f);

    return (n == total);
}