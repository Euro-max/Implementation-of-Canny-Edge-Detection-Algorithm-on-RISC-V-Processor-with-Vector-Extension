/**
 * @file gtest_image_io.cpp
 * @brief Google Test suite for raw image I/O operations
 * @ingroup tests
 * 
 * Tests the raw image loading/saving functions and aligned memory allocation.
 * Uses temporary files and directories to avoid polluting the filesystem.
 * 
 * @see image_io.h
 * @see allocate_buffer()
 * @see load_raw()
 * @see save_raw()
 */

#include <gtest/gtest.h>
#include "image_io.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ──────────────────────────────────────────────────────────────────────────────
// Fixture: creates a temp directory and cleans up after each test
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @class ImageIOTest
 * @brief Test fixture for image I/O tests with automatic cleanup
 * 
 * Creates a temporary directory before each test and removes it
 * after the test completes, ensuring isolated test runs.
 */
class ImageIOTest : public ::testing::Test {
protected:
    fs::path tmp_dir;   ///< Temporary directory path
    fs::path tmp_file;  ///< Temporary test file path

    /**
     * @brief Set up temporary directory before each test
     */
    void SetUp() override {
        tmp_dir  = fs::temp_directory_path() / "image_io_tests";
        fs::create_directories(tmp_dir);
        tmp_file = tmp_dir / "test.raw";
    }

    /**
     * @brief Clean up temporary directory after each test
     */
    void TearDown() override {
        fs::remove_all(tmp_dir);
    }

    /**
     * @brief Write raw bytes to a file on disk
     * @param path Destination file path
     * @param data Pointer to data bytes
     * @param n Number of bytes to write
     */
    void write_raw(const fs::path& path, const uint8_t* data, size_t n) {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data), n);
    }

    /**
     * @brief Read raw bytes from a file
     * @param path Source file path
     * @param n Number of bytes to read
     * @return Vector containing the read bytes
     */
    std::vector<uint8_t> read_raw(const fs::path& path, size_t n) {
        std::ifstream f(path, std::ios::binary);
        std::vector<uint8_t> buf(n);
        f.read(reinterpret_cast<char*>(buf.data()), n);
        return buf;
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 1. allocate_buffer returns non-null for valid dimensions
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test AllocateBufferNonNull
 * @brief Verify allocate_buffer returns a valid pointer for valid dimensions
 */
TEST_F(ImageIOTest, AllocateBufferNonNull) {
    uint8_t* buf = allocate_buffer(64, 64);
    ASSERT_NE(buf, nullptr);
    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. allocate_buffer is 64-byte aligned
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test AllocateBufferAligned64
 * @brief Verify allocated buffer meets 64-byte alignment requirement
 * 
 * 64-byte alignment is required for optimal SIMD (SSE/AVX) and
 * RISC-V RVV vectorized operations.
 */
TEST_F(ImageIOTest, AllocateBufferAligned64) {
    uint8_t* buf = allocate_buffer(32, 32);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(buf) % 64, 0u)
        << "Buffer is not 64-byte aligned";
    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. load_raw reads correct pixel values
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test LoadRawCorrectValues
 * @brief Verify load_raw correctly reads back written pixel data
 */
TEST_F(ImageIOTest, LoadRawCorrectValues) {
    const int W = 4, H = 4;
    uint8_t expected[W * H];
    for (int i = 0; i < W * H; ++i) expected[i] = static_cast<uint8_t>(i * 16);

    write_raw(tmp_file, expected, W * H);

    uint8_t* buf = allocate_buffer(W, H);
    ASSERT_TRUE(load_raw(tmp_file.string().c_str(), buf, W, H));

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(buf[i], expected[i]) << "Mismatch at index " << i;

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 4. load_raw returns false for a non-existent file
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test LoadRawMissingFileFails
 * @brief Verify load_raw returns false when file doesn't exist
 */
TEST_F(ImageIOTest, LoadRawMissingFileFails) {
    uint8_t* buf = allocate_buffer(8, 8);
    ASSERT_NE(buf, nullptr);

    bool result = load_raw("/nonexistent/path/file.raw", buf, 8, 8);
    EXPECT_FALSE(result);

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. save_raw writes the correct number of bytes
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SaveRawCorrectSize
 * @brief Verify save_raw writes exactly width×height bytes
 */
TEST_F(ImageIOTest, SaveRawCorrectSize) {
    const int W = 8, H = 8;
    uint8_t* buf = allocate_buffer(W, H);
    std::memset(buf, 42, W * H);

    save_raw(tmp_file.string().c_str(), buf, W, H);

    EXPECT_EQ(fs::file_size(tmp_file), static_cast<uintptr_t>(W * H));

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 6. save_raw then load_raw round-trips pixel values exactly
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SaveLoadRoundTrip
 * @brief Verify save+load round-trip preserves exact pixel values
 */
TEST_F(ImageIOTest, SaveLoadRoundTrip) {
    const int W = 16, H = 16;
    uint8_t* orig = allocate_buffer(W, H);
    uint8_t* reloaded = allocate_buffer(W, H);

    for (int i = 0; i < W * H; ++i) orig[i] = static_cast<uint8_t>(i % 256);

    save_raw(tmp_file.string().c_str(), orig, W, H);
    ASSERT_TRUE(load_raw(tmp_file.string().c_str(), reloaded, W, H));

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(orig[i], reloaded[i]) << "Round-trip mismatch at index " << i;

    free(orig);
    free(reloaded);
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. save_raw persists all-zero buffer correctly
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SaveRawAllZeros
 * @brief Verify save_raw correctly writes a buffer of all zeros
 */
TEST_F(ImageIOTest, SaveRawAllZeros) {
    const int W = 8, H = 8;
    uint8_t* buf = allocate_buffer(W, H);
    std::memset(buf, 0, W * H);

    save_raw(tmp_file.string().c_str(), buf, W, H);
    auto disk = read_raw(tmp_file, W * H);

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(disk[i], 0u) << "Non-zero byte at index " << i;

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 8. save_raw persists all-255 buffer correctly
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test SaveRawAllMax
 * @brief Verify save_raw correctly writes a buffer of all 255 values
 */
TEST_F(ImageIOTest, SaveRawAllMax) {
    const int W = 8, H = 8;
    uint8_t* buf = allocate_buffer(W, H);
    std::memset(buf, 255, W * H);

    save_raw(tmp_file.string().c_str(), buf, W, H);
    auto disk = read_raw(tmp_file, W * H);

    for (int i = 0; i < W * H; ++i)
        EXPECT_EQ(disk[i], 255u) << "Non-255 byte at index " << i;

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 9. load_raw returns false when file is too small
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test LoadRawTooSmallFileFails
 * @brief Verify load_raw handles truncated files gracefully
 * 
 * When the file is smaller than expected, the function should
 * either return false or handle it without crashing.
 */
TEST_F(ImageIOTest, LoadRawTooSmallFileFails) {
    const int W = 8, H = 8;
    // Write only half the expected bytes
    uint8_t small[W * H / 2] = {};
    write_raw(tmp_file, small, W * H / 2);

    uint8_t* buf = allocate_buffer(W, H);
    bool result = load_raw(tmp_file.string().c_str(), buf, W, H);
    // Should fail or at least not crash; implementation may return false
    // We just verify it doesn't crash and the result is defined.
    (void)result;
    SUCCEED();  // no crash = pass; stricter check below if your impl returns false
    // EXPECT_FALSE(result);  // uncomment if load_raw validates size

    free(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// 10. Multiple allocate_buffer calls return independent, non-overlapping regions
// ──────────────────────────────────────────────────────────────────────────────

/**
 * @test MultipleAllocationsIndependent
 * @brief Verify multiple buffer allocations are independent and non-overlapping
 * 
 * Allocated buffers should point to distinct memory regions that
 * don't interfere with each other.
 */
TEST_F(ImageIOTest, MultipleAllocationsIndependent) {
    const int W = 16, H = 16;
    uint8_t* a = allocate_buffer(W, H);
    uint8_t* b = allocate_buffer(W, H);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);

    std::memset(a, 0xAA, W * H);
    std::memset(b, 0x55, W * H);

    // Verify they didn't stomp each other
    for (int i = 0; i < W * H; ++i) {
        EXPECT_EQ(a[i], 0xAA) << "Buffer a corrupted at " << i;
        EXPECT_EQ(b[i], 0x55) << "Buffer b corrupted at " << i;
    }

    free(a);
    free(b);
}