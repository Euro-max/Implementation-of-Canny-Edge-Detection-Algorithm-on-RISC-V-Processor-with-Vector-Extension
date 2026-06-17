#include "image_io.h"
#include <stdio.h>
#include <string.h>    // memset

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

