/**
 * @file test_rvv_equivalence.cpp
 * @brief Tests RVV Gaussian and Magnitude outputs against the scalar baseline.
 * * Runs on QEMU (RISC-V) using randomized data to catch boundary/rounding edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "gaussian.h"
#include "rvv_gaussian.h"
#include "magnitude.h"
#include "rvv_magnitude.h"

// GTest-like macros for clean output
#define EXPECT_LE(val1, val2) \
    if ((val1) > (val2)) { \
        fprintf(stderr, "    [  FAILED  ] Expected %s <= %s, but %d > %d at index %d\n", #val1, #val2, (int)(val1), (int)(val2), i); \
        errors++; \
    }

int main() {
    printf("[==========] Running RVV Equivalence Tests on QEMU.\n");

    int W = 128;
    int H = 128;
    int size = W * H;

    uint8_t* img_in = (uint8_t*)malloc(size);
    uint8_t* out_scalar_gauss = (uint8_t*)malloc(size);
    uint8_t* out_rvv_gauss = (uint8_t*)malloc(size);
    
    int16_t* gx = (int16_t*)malloc(size * sizeof(int16_t));
    int16_t* gy = (int16_t*)malloc(size * sizeof(int16_t));
    uint8_t* out_scalar_mag = (uint8_t*)malloc(size);
    uint8_t* out_rvv_mag = (uint8_t*)malloc(size);

    // Seed pseudo-random numbers
    srand(42);
    for (int i = 0; i < size; i++) {
        img_in[i] = rand() % 256;
        gx[i] = (rand() % 512) - 256; // random gradients -256 to 255
        gy[i] = (rand() % 512) - 256;
    }

    int total_errors = 0;

    // ---------------------------------------------------------
    // Test 1: Gaussian Blur
    // ---------------------------------------------------------
    printf("[ RUN      ] RVV_Equivalence.GaussianRandomized\n");
    gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, out_scalar_gauss, W, H);
    gaussian_blur_rvv(img_in, out_rvv_gauss, W, H);

    int errors = 0;
    for (int i = 0; i < size; i++) {
        int diff = abs((int)out_scalar_gauss[i] - (int)out_rvv_gauss[i]);
        EXPECT_LE(diff, 1);
        if (errors >= 5) {
            printf("    [   INFO   ] Suppressing further Gaussian errors...\n");
            break;
        }
    }
    
    if (errors == 0) {
        printf("[       OK ] RVV_Equivalence.GaussianRandomized\n");
    } else {
        total_errors += errors;
    }

    // ---------------------------------------------------------
    // Test 2: Magnitude
    // ---------------------------------------------------------
    printf("[ RUN      ] RVV_Equivalence.MagnitudeRandomized\n");
    errors = 0;
    
    compute_magnitude_l1(gx, gy, out_scalar_mag, W, H);
    compute_magnitude_l1_rvv(gx, gy, out_rvv_mag, W, H);

    for (int i = 0; i < size; i++) {
        int diff = abs((int)out_scalar_mag[i] - (int)out_rvv_mag[i]);
        EXPECT_LE(diff, 1);
        if (errors >= 5) {
            printf("    [   INFO   ] Suppressing further Magnitude errors...\n");
            break;
        }
    }

    if (errors == 0) {
        printf("[       OK ] RVV_Equivalence.MagnitudeRandomized\n");
    } else {
        total_errors += errors;
    }

    // ---------------------------------------------------------
    // Cleanup & Exit
    // ---------------------------------------------------------
    free(img_in); free(out_scalar_gauss); free(out_rvv_gauss);
    free(gx); free(gy); free(out_scalar_mag); free(out_rvv_mag);

    if (total_errors == 0) {
        printf("[==========] 2 tests ran. (PASSED)\n");
        return 0;
    } else {
        printf("[==========] 2 tests ran. (FAILED)\n");
        return 1;
    }
}
