/**
 * @file gen_test_image.cpp
 * @brief Synthetic test image generator for the Canny Edge Detection pipeline.
 * * This standalone utility creates a simple 128x128 8-bit grayscale image 
 * featuring a stark white rectangle on a solid black background. It is used 
 * to verify that the spatial gradient filters (Sobel) and Image I/O routines 
 * behave correctly under controlled, highly predictable mathematical conditions.
 */

#include "image_io.h"
#include <stdio.h>
#include <string.h>    // memset

/**
 * @brief Main execution entry point.
 * * Allocates a strictly 64-byte aligned memory buffer, clears it to black (0), 
 * and iterates over the central coordinate bounds to draw a white (255) rectangle. 
 * The resulting raw pixel buffer is written directly to disk.
 * * @return 0 on successful execution.
 */
int main() {

    int width  = 128;
    int height = 128;
    int total  = width * height;

    // Allocate and zero-fill (all black)
    uint8_t* img = (uint8_t*)aligned_alloc(64, total);
    memset(img, 0, total);   // fill everything with 0 (black)

    // Draw a white rectangle in the middle
    // from (32,32) to (96,96)
    for (int y = 32; y < 96; y++) {
        for (int x = 32; x < 96; x++) {
            img[y * width + x] = 255;    // white pixel
        }
    }

    // Save it
    if (save_image("images/test_input.raw", img, width, height)) {
        printf("Generated images/test_input.raw (%dx%d)\n", width, height);
        printf("Black background with white rectangle (32,32)-(96,96)\n");
    }

    free_image(img);
    return 0;
}