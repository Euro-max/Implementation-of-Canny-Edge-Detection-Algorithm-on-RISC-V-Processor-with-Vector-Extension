/**
 * @file main.cpp
 * @brief Main execution entry point for Canny Edge Detection (7-stage pipeline).
 */

#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "nms_threshold.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// 7-arg bottleneck finder
static uint64_t find_max(uint64_t cyc_gaussian, uint64_t cyc_sobel, uint64_t cyc_mag, 
                        uint64_t cyc_dir, uint64_t cyc_nms, uint64_t cyc_thresh, uint64_t cyc_hyst) {
    uint64_t m = cyc_gaussian;
    if (cyc_sobel > m) m = cyc_sobel;
    if (cyc_mag > m) m = cyc_mag;
    if (cyc_dir > m) m = cyc_dir;
    if (cyc_nms > m) m = cyc_nms;
    if (cyc_thresh > m) m = cyc_thresh;
    if (cyc_hyst > m) m = cyc_hyst;
    return m;
}

static void print_table(uint64_t cyc_gaussian, uint64_t cyc_sobel,
                        uint64_t cyc_mag,      uint64_t cyc_dir,
                        uint64_t cyc_nms,      uint64_t cyc_thresh,
                        uint64_t cyc_hyst,     int reps) {
    
    uint64_t total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir + cyc_nms + cyc_thresh + cyc_hyst;
    uint64_t hot   = find_max(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, cyc_nms, cyc_thresh, cyc_hyst);

    printf("\n+--------------+----------------+--------+--------------+\n");
    printf("|          Canny Pipeline - Cycle Count Results         |\n");
    printf("|              (average over %d runs)                 |\n", reps);
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
    ROW("NMS",        cyc_nms);
    ROW("Threshold",  cyc_thresh);
    ROW("Hysteresis", cyc_hyst);
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
    printf("|        Canny Edge Detection Pipeline         |\n");
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
    uint8_t* img_nms   = allocate_buffer(W, H);
    uint8_t* img_edges = allocate_buffer(W, H);

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir || !img_nms || !img_edges) {
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

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) apply_nms(img_mag, img_dir, img_nms, W, H);
    uint64_t cyc_nms = (read_cycles() - c0) / REPS;

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) apply_double_threshold(img_nms, img_edges, W, H, 30, 90);
    uint64_t cyc_thresh = (read_cycles() - c0) / REPS;

    c0 = read_cycles();
    for (int i = 0; i < REPS; i++) apply_hysteresis(img_edges, W, H);
    uint64_t cyc_hyst = (read_cycles() - c0) / REPS;

    save_raw(argv[2], img_edges, W, H);

    print_table(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, cyc_nms, cyc_thresh, cyc_hyst, REPS);

    if (argc >= 6) {
        FILE* cf = fopen(argv[5], "w");
        if (cf) {
            fprintf(cf, "%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n", 
                    (unsigned long long)cyc_gaussian, (unsigned long long)cyc_sobel,
                    (unsigned long long)cyc_mag, (unsigned long long)cyc_dir,
                    (unsigned long long)cyc_nms, (unsigned long long)cyc_thresh,
                    (unsigned long long)cyc_hyst);
            fclose(cf);
        }
    }

    free(img_in); free(img_blur); free(gx); free(gy); 
    free(img_mag); free(img_dir); free(img_nms); free(img_edges);

    return 0;
}
