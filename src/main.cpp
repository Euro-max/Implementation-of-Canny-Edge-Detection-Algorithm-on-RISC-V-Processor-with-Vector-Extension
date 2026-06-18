/**
 * @file main.cpp
 * @brief Main execution entry point for Edge Detection (4-stage pipeline).
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// 4-arg bottleneck finder
static uint64_t find_max(uint64_t cyc_gaussian, uint64_t cyc_sobel, 
                         uint64_t cyc_mag,      uint64_t cyc_dir) {
    uint64_t m = cyc_gaussian;
    if (cyc_sobel > m) m = cyc_sobel;
    if (cyc_mag > m) m = cyc_mag;
    if (cyc_dir > m) m = cyc_dir;
    return m;
}

static void print_table(uint64_t cyc_gaussian, uint64_t cyc_sobel,
                        uint64_t cyc_mag,      uint64_t cyc_dir,
                        int reps) {
    
    uint64_t total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir;
    uint64_t hot   = find_max(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir);

    printf("\n+--------------+----------------+--------+--------------+\n");
    printf("|          Pipeline - Cycle Count Results               |\n");
    printf("|              (average over %d runs)                  |\n", reps);
    printf("+--------------+----------------+--------+--------------+\n");
    printf("| Stage        | Cycles         |  %%     | Bottleneck?  |\n");
    printf("+--------------+----------------+--------+--------------+\n");

    #define ROW(name, cyc) \
        printf("| %-12s | %14llu | %5.1f%% | %-12s |\n", \
                name, (unsigned long long)(cyc), \
                100.0*(cyc)/total, \
                ((cyc)==hot) ? "< HOT" : "")

    ROW("Gaussian",   cyc_gaussian);
    ROW("Sobel",      cyc_sobel);
    ROW("Magnitude",  cyc_mag);
    ROW("Direction",  cyc_dir);
    #undef ROW

    printf("+--------------+----------------+--------+--------------+\n");
    printf("| %-12s | %14llu | 100.0%% |              |\n",
            "TOTAL", (unsigned long long)total);
    printf("+--------------+----------------+--------+--------------+\n\n");
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ./canny <input.raw> <output.raw> <width> <height> [cycles.txt]\n");
        return 1;
    }

    int W = atoi(argv[3]);
    int H = atoi(argv[4]);

    printf("\n+----------------------------------------------+\n");
    printf("|        Edge Detection Pipeline (4-Stage)     |\n");
    printf("|        Running on RISC-V via QEMU            |\n");
    printf("+----------------------------------------------+\n");
    printf("|  Input : %-35s|\n", argv[1]);
    printf("|  Output: %-35s|\n", argv[2]);
    printf("|  Size  : %dx%d%-*s|\n", W, H, (int)(35 - 1 - snprintf(NULL,0,"%dx%d",W,H)), "");
    printf("+----------------------------------------------+\n\n");

    uint8_t* img_in    = allocate_buffer(W, H);
    uint8_t* img_blur  = allocate_buffer(W, H);
    int16_t* gx        = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    int16_t* gy        = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    uint8_t* img_mag   = allocate_buffer(W, H);
    uint8_t* img_dir   = allocate_buffer(W, H);

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir) {
        fprintf(stderr, "ERROR: Memory allocation failed!\n");
        return 1;
    }

    if (!load_raw(argv[1], img_in, W, H)) return 1;

    const int REPS = 100;
    uint64_t c0;

    // Execution Loop
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
    uint64_t cyc_gaussian = (read_cycles() - c0) / REPS;

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) compute_sobel(img_blur, gx, gy, W, H);
    uint64_t cyc_sobel = (read_cycles() - c0) / REPS;

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) compute_magnitude_l1(gx, gy, img_mag, W, H);
    uint64_t cyc_mag = (read_cycles() - c0) / REPS;

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) compute_direction(gx, gy, img_dir, W, H);
    uint64_t cyc_dir = (read_cycles() - c0) / REPS;

    // --- THE FIX IS HERE ---
    // Save the Magnitude buffer (img_mag) instead of Direction (img_dir).
    // Magnitude contains the actual 0-255 edge strengths so you can see them!
    save_raw(argv[2], img_mag, W, H);

    print_table(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, REPS);

    if (argc >= 6) {
        FILE* cf = fopen(argv[5], "w");
        if (cf) {
            fprintf(cf, "%llu\n%llu\n%llu\n%llu\n", 
                    (unsigned long long)cyc_gaussian, (unsigned long long)cyc_sobel,
                    (unsigned long long)cyc_mag, (unsigned long long)cyc_dir);
            fclose(cf);
        }
    }

    free(img_in); free(img_blur); free(gx); free(gy); 
    free(img_mag); free(img_dir);

    return 0;
}