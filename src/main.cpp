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

static uint64_t find_max(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    uint64_t m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    if (d > m) m = d;
    return m;
}

static void print_table(uint64_t cyc_gaussian, uint64_t cyc_sobel,
                        uint64_t cyc_mag,      uint64_t cyc_dir,
                        int reps) {
    uint64_t total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir;
    uint64_t hot   = find_max(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir);

    printf("\n");
    printf("+--------------+----------------+--------+--------------+\n");
    printf("|       Canny Pipeline - Cycle Count Results            |\n");
    printf("|              (average over %d runs)                 |\n", reps);
    printf("+--------------+----------------+--------+--------------+\n");
    printf("| Stage        | Cycles         |   %%    | Bottleneck?  |\n");
    printf("+--------------+----------------+--------+--------------+\n");

    #define ROW(name, cyc) \
        printf("| %-12s | %14llu | %5.1f%% | %-12s |\n", \
               name, (unsigned long long)(cyc), \
               100.0*(cyc)/total, \
               ((cyc)==hot) ? "< HOT" : "")

    ROW("Gaussian",  cyc_gaussian);
    ROW("Sobel",     cyc_sobel);
    ROW("Magnitude", cyc_mag);
    ROW("Direction", cyc_dir);
    #undef ROW

    printf("+--------------+----------------+--------+--------------+\n");
    printf("| %-12s | %14llu | 100.0%% |              |\n",
           "TOTAL", (unsigned long long)total);
    printf("+--------------+----------------+--------+--------------+\n");
    printf("\n");
    printf("  Amdahl's Law - Optimization priority:\n");
    printf("  Gaussian  %.1f%% -> vectorize with RVV\n", 100.0*cyc_gaussian/total);
    printf("  Sobel     %.1f%% -> check if worth it\n",  100.0*cyc_sobel/total);
    printf("  Magnitude %.1f%% -> vectorize with RVV\n", 100.0*cyc_mag/total);
    printf("  Direction %.1f%% -> NOT worth optimizing\n",100.0*cyc_dir/total);
    printf("\n");
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ./canny <input.raw> <output.raw> <width> <height>\n");
        fprintf(stderr, "Example: ./canny test_input.raw out.raw 458 260\n");
        return 1;
    }

    int W = atoi(argv[3]);
    int H = atoi(argv[4]);

    printf("\n");
    printf("+----------------------------------------------+\n");
    printf("|       Canny Edge Detection Pipeline          |\n");
    printf("|       Running on RISC-V via QEMU             |\n");
    printf("+----------------------------------------------+\n");
    printf("|  Input : %-35s|\n", argv[1]);
    printf("|  Output: %-35s|\n", argv[2]);
    printf("|  Size  : %dx%d%-*s|\n", W, H,
           (int)(35 - 1 - snprintf(NULL,0,"%dx%d",W,H)), "");
    printf("+----------------------------------------------+\n\n");

    uint8_t* img_in   = allocate_buffer(W, H);
    uint8_t* img_blur = allocate_buffer(W, H);
    int16_t* gx       = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    int16_t* gy       = (int16_t*)aligned_alloc(64, W*H*sizeof(int16_t));
    uint8_t* img_mag  = allocate_buffer(W, H);
    uint8_t* img_dir  = allocate_buffer(W, H);

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir) {
        fprintf(stderr, "ERROR: Memory allocation failed!\n");
        return 1;
    }

    printf("[ 1/6 ] Loading image ...\n");
    if (!load_raw(argv[1], img_in, W, H)) {
        fprintf(stderr, "ERROR: Could not open: %s\n", argv[1]);
        return 1;
    }
    printf("[ OK  ] Image loaded (%d pixels)\n\n", W*H);

    const int REPS = 100;
    uint64_t c0, c1;

    printf("[ 2/6 ] Gaussian blur (5x5) ...\n");
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
    c1 = read_cycles();
    uint64_t cyc_gaussian = (c1 - c0) / REPS;
    printf("[ OK  ] Done - %llu cycles avg\n\n", (unsigned long long)cyc_gaussian);

    printf("[ 3/6 ] Sobel gradient (Gx and Gy) ...\n");
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_sobel(img_blur, gx, gy, W, H);
    c1 = read_cycles();
    uint64_t cyc_sobel = (c1 - c0) / REPS;
    printf("[ OK  ] Done - %llu cycles avg\n\n", (unsigned long long)cyc_sobel);

    printf("[ 4/6 ] Gradient magnitude (L1 norm) ...\n");
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_magnitude_l1(gx, gy, img_mag, W, H);
    c1 = read_cycles();
    uint64_t cyc_mag = (c1 - c0) / REPS;
    printf("[ OK  ] Done - %llu cycles avg\n\n", (unsigned long long)cyc_mag);

    printf("[ 5/6 ] Gradient direction ...\n");
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_direction(gx, gy, img_dir, W, H);
    c1 = read_cycles();
    uint64_t cyc_dir = (c1 - c0) / REPS;
    printf("[ OK  ] Done - %llu cycles avg\n\n", (unsigned long long)cyc_dir);

    printf("[ 6/6 ] Saving output to %s ...\n", argv[2]);
    save_raw(argv[2], img_mag, W, H);
    printf("[ OK  ] Saved\n");

    print_table(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, REPS);

    if (argc >= 6) {
        FILE* cf = fopen(argv[5], "w");
        if (cf) {
            fprintf(cf, "%llu\n", (unsigned long long)cyc_gaussian);
            fprintf(cf, "%llu\n", (unsigned long long)cyc_sobel);
            fprintf(cf, "%llu\n", (unsigned long long)cyc_mag);
            fprintf(cf, "%llu\n", (unsigned long long)cyc_dir);
            fclose(cf);
            printf("[ OK  ] Cycles saved to %s\n", argv[5]);
        }
    }

    free(img_in);  free(img_blur);
    free(gx);      free(gy);
    free(img_mag); free(img_dir);

    return 0;
}
