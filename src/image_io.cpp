// image_io.cpp
// Uses stdio (fopen/fread/fwrite) instead of C++ streams (ifstream/ofstream)
// WHY: C++ streams require more C++ runtime support which is problematic
// with riscv64-unknown-elf bare-metal toolchain + newlib

#include "image_io.h"
#include <stdlib.h>    // aligned_alloc
#include <stdio.h>     // fopen, fread, fwrite, fclose

// Allocate a 64-byte-aligned buffer for width*height pixels
// 64-byte alignment is required for RVV vector loads/stores
uint8_t* allocate_buffer(int width, int height) {
    return (uint8_t*)aligned_alloc(64, width * height);
}

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
