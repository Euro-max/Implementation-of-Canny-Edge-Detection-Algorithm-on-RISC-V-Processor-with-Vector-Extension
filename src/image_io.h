/**
 * @file image_io.h
 * @brief Raw image file I/O with aligned memory allocation
 * @ingroup io
 * 
 * Provides memory allocation and raw binary file operations for
 * 8-bit grayscale images. Uses 64-byte aligned allocations for
 * optimal SIMD/RVV vectorization performance.
 */

#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include <cstdint>

/**
 * @brief Allocates 64-byte aligned buffer for image data
 * 
 * Uses aligned_alloc with 64-byte alignment to facilitate SIMD
 * (SSE/AVX) and RISC-V RVV vectorized operations.
 * 
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * @return        Pointer to allocated buffer, or nullptr on failure
 * 
 * @note Buffer must be freed with free() (aligned_alloc uses standard free)
 * @note Allocation size: width × height bytes
 * 
 * @see load_raw() For reading into allocated buffer
 * @see save_raw() For writing buffer to file
 */
uint8_t* allocate_buffer(int width, int height);

/**
 * @brief Loads raw 8-bit grayscale image from binary file
 * 
 * Reads exactly width × height bytes from the specified file
 * into the provided buffer. No header parsing or format validation.
 * 
 * @param path    File path to read from
 * @param buffer  Destination buffer (must be allocated with width × height)
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * @return        true on successful read, false on error
 * 
 * @pre buffer must have capacity width × height bytes
 * @post buffer contains raw pixel data if return is true
 * 
 * @see allocate_buffer() For buffer allocation
 * @see save_raw() For writing images
 */
bool load_raw(const char* path, uint8_t* buffer, int width, int height);

/**
 * @brief Saves raw 8-bit grayscale image to binary file
 * 
 * Writes exactly width × height bytes from buffer to the specified file.
 * Overwrites existing file if it exists.
 * 
 * @param path    File path to write to
 * @param buffer  Source buffer containing image data
 * @param width   Image width in pixels
 * @param height  Image height in pixels
 * @return        true on successful write, false on error
 * 
 * @pre buffer contains valid width × height pixel data
 * @post File created/overwritten with raw binary data
 * 
 * @see load_raw() For reading images
 */
bool save_raw(const char* path, const uint8_t* buffer, int width, int height);

#endif