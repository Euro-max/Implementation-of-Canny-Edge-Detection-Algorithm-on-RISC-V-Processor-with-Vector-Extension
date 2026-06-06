#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <iostream>
#include <stdint.h>

// RISC-V cycle counter — works on QEMU
// rdcycle reads the hardware cycle counter register
static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: ./canny <in.raw> <out.raw> <width> <height>\n";
        return 1;
    }

    int W = std::stoi(argv[3]);
    int H = std::stoi(argv[4]);

    std::cerr << "W=" << W << " H=" << H << "\n";

    uint8_t* img_in   = allocate_buffer(W, H);
    uint8_t* img_blur = allocate_buffer(W, H);
    int16_t* gx = (int16_t*)aligned_alloc(64, W * H * 2);
    int16_t* gy = (int16_t*)aligned_alloc(64, W * H * 2);
    uint8_t* img_mag = allocate_buffer(W, H);
    uint8_t* img_dir = allocate_buffer(W, H);

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir) {
        std::cerr << "ERROR: Buffer allocation failed!\n";
        return 1;
    }

    if (!load_raw(argv[1], img_in, W, H)) {
        std::cerr << "ERROR: Failed to load: " << argv[1] << "\n";
        return 1;
    }

    const int REPS = 100;
    uint64_t c0, c1;

    // ── Gaussian ─────────────────────────────────────────────────
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
    c1 = read_cycles();
    uint64_t cyc_gaussian = (c1 - c0) / REPS;

    // ── Sobel ────────────────────────────────────────────────────
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_sobel(img_blur, gx, gy, W, H);
    c1 = read_cycles();
    uint64_t cyc_sobel = (c1 - c0) / REPS;

    // ── Magnitude ────────────────────────────────────────────────
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_magnitude_l1(gx, gy, img_mag, W, H);
    c1 = read_cycles();
    uint64_t cyc_mag = (c1 - c0) / REPS;

    // ── Direction ────────────────────────────────────────────────
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_direction(gx, gy, img_dir, W, H);
    c1 = read_cycles();
    uint64_t cyc_dir = (c1 - c0) / REPS;

    // ── Save output ───────────────────────────────────────────────
    save_raw(argv[2], img_mag, W, H);

    // ── Print results ─────────────────────────────────────────────
    uint64_t total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir;
    std::cout << "\n=== Cycle Counts (avg over " << REPS << " runs) ===\n";
    std::cout << "Gaussian  : " << cyc_gaussian
              << " cycles  (" << 100.0*cyc_gaussian/total << "%)\n";
    std::cout << "Sobel     : " << cyc_sobel
              << " cycles  (" << 100.0*cyc_sobel/total    << "%)\n";
    std::cout << "Magnitude : " << cyc_mag
              << " cycles  (" << 100.0*cyc_mag/total      << "%)\n";
    std::cout << "Direction : " << cyc_dir
              << " cycles  (" << 100.0*cyc_dir/total      << "%)\n";
    std::cout << "TOTAL     : " << total << " cycles\n";

    return 0;
}
