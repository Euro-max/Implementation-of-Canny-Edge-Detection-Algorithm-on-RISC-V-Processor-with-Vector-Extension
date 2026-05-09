#include <iostream>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "image_io.h"

// Test 1: Allocate buffer is not null
void test_allocate_buffer() {
    uint8_t* buf = allocate_buffer(64, 64);
    assert(buf != nullptr);
    free(buf);
    std::cout << "✓ Test 1 PASSED: Buffer allocation works\n";
}

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

// Test 3: Load nonexistent file returns false
void test_load_missing_file() {
    uint8_t* buf = allocate_buffer(8, 8);
    bool ok = load_raw("/tmp/doesnotexist.raw", buf, 8, 8);
    assert(!ok);
    free(buf);
    std::cout << "✓ Test 3 PASSED: Missing file returns false\n";
}

// Test 4: Buffer is 64-byte aligned
void test_alignment() {
    uint8_t* buf = allocate_buffer(64, 64);
    assert(((uintptr_t)buf % 64) == 0);
    free(buf);
    std::cout << "✓ Test 4 PASSED: Buffer is 64-byte aligned\n";
}

int main() {
    std::cout << "=== Image I/O Tests ===\n\n";
    test_allocate_buffer();
    test_save_and_load();
    test_load_missing_file();
    test_alignment();
    std::cout << "\n✅ All Image I/O tests PASSED!\n";
    return 0;
}
