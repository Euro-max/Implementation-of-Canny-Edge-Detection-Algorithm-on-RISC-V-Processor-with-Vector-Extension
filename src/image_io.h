/**
 * @file image_io.h
 * @brief Declarations for raw image file I/O and aligned memory allocation.
 * * This header defines utility functions for reading and writing raw 8-bit 
 * grayscale image data to and from disk. It also provides a specialized 
 * memory allocator to ensure all image buffers meet the strict alignment 
 * requirements necessary for high-performance vector processing.
 */

#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include <cstdint>

/**
 * @brief Allocates a dynamically sized, memory-aligned image buffer.
 * * This function guarantees that the starting address of the returned buffer 
 * is aligned to a 64-byte boundary. This is critical for maximizing memory 
 * throughput and preventing unaligned access faults when using RISC-V Vector 
 * (RVV) load/store instructions in the downstream processing pipeline.
 * * @param width  Width of the image to allocate in pixels.
 * @param height Height of the image to allocate in pixels.
 * @return A 64-byte aligned pointer to the newly allocated uint8_t buffer.
 */
// Aligned allocation (64-byte) for better SIMD/RVV performance
uint8_t* allocate_buffer(int width, int height);

/**
 * @brief Loads raw 8-bit grayscale pixel data from a binary file.
 * * Reads the exact number of expected bytes (width * height) from a flat, 
 * headerless binary file directly into the provided image buffer.
 * * @param path   Filepath to the raw binary image file.
 * @param buffer Pointer to the pre-allocated destination memory buffer.
 * @param width  Expected width of the image in pixels.
 * @param height Expected height of the image in pixels.
 * @return true if the file was successfully opened and all bytes were read; false otherwise.
 */
bool load_raw(const char* path, uint8_t* buffer, int width, int height);

/**
 * @brief Saves an 8-bit grayscale image buffer to a raw binary file.
 * * Writes the raw pixel data from memory directly to disk as a flat, 
 * headerless binary file, dumping exactly (width * height) bytes.
 * * @param path   Filepath where the raw binary data will be written.
 * @param buffer Pointer to the image memory buffer to be saved.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 * @return true if the file was successfully opened and all bytes were written; false otherwise.
 */
bool save_raw(const char* path, const uint8_t* buffer, int width, int height);

#endif
