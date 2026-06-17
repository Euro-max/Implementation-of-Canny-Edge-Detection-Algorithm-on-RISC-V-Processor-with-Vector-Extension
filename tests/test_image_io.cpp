/**
 * @file test_image_io.cpp
 * @brief Unit tests for raw image input/output operations and memory allocation.
 * * This file contains bare-metal assertion tests to verify that image buffers 
 * are correctly allocated with strict memory alignment (for RVV vectorization) 
 * and that raw binary file reading and writing work flawlessly without data loss.
 */

#include <iostream>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "image_io.h"

/**
 * @brief Verifies that buffer allocation succeeds and returns a valid pointer.
 * * This is a fundamental sanity check to ensure the underlying dynamic memory 
 * allocation routine (e.g., aligned_alloc) does not fail and return nullptr 
 * when requested to allocate standard image dimensions.
 */
// Test 1: Allocate buffer is not null
void test_allocate_buffer() {
    uint8_t* buf = allocate_buffer(64, 64);
    assert(buf != nullptr);
    free(buf);
    std::cout << "✓ Test 1 PASSED: Buffer allocation works\n";
}

/**
 * @brief Verifies a full round-trip of saving and loading raw image data.
 * * Generates a known mathematical pixel pattern, writes it to a temporary file 
 * on the disk, reads it back into a new buffer, and compares every single byte 
 * to guarantee there is no silent data corruption or file truncation.
 */
// Test 2: Save and reload gives same data
void test_save_and_load() {
    int W = 8, H = 8;
    uint8_t* original = allocate_buffer(W, H);
    uint8_t* loaded   = allocate_buffer(W, H);

    // Fill with known pattern
    for (int i = 0; i < W * H; i++)
        original[i] = (uint8_t)(i * 3);

    // Save then load
    save_raw("/tmp/test_io.raw", original, W, H);
    bool ok = load_raw("/tmp/test_io.raw", loaded, W, H);

    assert(ok);
    for (int i = 0; i < W * H; i++)
        assert(original[i] == loaded[i]);

    free(original);
    free(loaded);
    std::remove("/tmp/test_io.raw");
    std::cout << "✓ Test 2 PASSED: Save and load gives same data\n";
}

/**
 * @brief Verifies graceful error handling when loading a non-existent file.
 * * Ensures that the system correctly catches the file open error and returns 
 * false, rather than crashing with a segmentation fault or attempting to 
 * read from a null file descriptor.
 */
// Test 3: Load nonexistent file returns false
void test_load_missing_file() {
    uint8_t* buf = allocate_buffer(8, 8);
    bool ok = load_raw("/tmp/doesnotexist.raw", buf, 8, 8);
    assert(!ok);
    free(buf);
    std::cout << "✓ Test 3 PASSED: Missing file returns false\n";
}

/**
 * @brief Verifies strict 64-byte memory alignment for the allocated buffers.
 * * 64-byte alignment is a critical hardware requirement for efficient 
 * cache-line utilization and maximum performance when using RISC-V Vector 
 * (RVV) load/store instructions in the downstream pipeline.
 */
// Test 4: Buffer is 64-byte aligned
void test_alignment() {
    uint8_t* buf = allocate_buffer(64, 64);
    assert(((uintptr_t)buf % 64) == 0);
    free(buf);
    std::cout << "✓ Test 4 PASSED: Buffer is 64-byte aligned\n";
}

/**
 * @brief Main execution entry point for Image I/O tests.
 * * Runs all test cases sequentially. If any assert fails, the program 
 * will abort immediately. Otherwise, it prints a success summary.
 * * @return 0 on successful execution of all assertions.
 */
int main() {
    std::cout << "=== Image I/O Tests ===\n\n";
    test_allocate_buffer();
    test_save_and_load();
    test_load_missing_file();
    test_alignment();
    std::cout << "\n✅ All Image I/O tests PASSED!\n";
    return 0;
}