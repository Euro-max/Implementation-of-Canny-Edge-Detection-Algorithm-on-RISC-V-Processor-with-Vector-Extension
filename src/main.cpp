#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include <iostream>
#include <iomanip>   // for setw() — controls column width in output
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
// read_cycles() — reads the RISC-V hardware cycle counter
//
// "rdcycle" is a special RISC-V assembly instruction that reads a register
// counting CPU cycles since boot. We read it before and after each stage,
// subtract to get how many cycles that stage consumed.
//
// This is like a stopwatch that counts in CPU steps instead of seconds.
// ─────────────────────────────────────────────────────────────────────────────
static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
// print_table() — prints a nicely formatted results table
// ─────────────────────────────────────────────────────────────────────────────
static void print_table(uint64_t cyc_gaussian, uint64_t cyc_sobel,
                         uint64_t cyc_mag,      uint64_t cyc_dir,
                         int reps) {
    uint64_t total = cyc_gaussian + cyc_sobel + cyc_mag + cyc_dir;

    std::cout << "\n";
    std::cout << "┌────────────────────────────────────────────────────────┐\n";
    std::cout << "│         Canny Pipeline — Cycle Count Results           │\n";
    std::cout << "│              (average over " << reps << " runs)                  │\n";
    std::cout << "├──────────────┬────────────────┬────────┬──────────────┤\n";
    std::cout << "│ Stage        │ Cycles         │   %    │ Bottleneck?  │\n";
    std::cout << "├──────────────┼────────────────┼────────┼──────────────┤\n";

    // Lambda to print one row
    auto row = [&](const char* name, uint64_t cyc) {
        double pct = 100.0 * cyc / total;
        // Mark the biggest consumer as the bottleneck
        const char* note = (cyc == std::max({cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir}))
                           ? "◄ HOT"  : "";
        std::cout << "│ " << std::left  << std::setw(12) << name
                  << " │ " << std::right << std::setw(14) << cyc
                  << " │ " << std::fixed << std::setprecision(1)
                  << std::setw(5) << pct << "% │ "
                  << std::left << std::setw(12) << note << " │\n";
    };

    row("Gaussian",  cyc_gaussian);
    row("Sobel",     cyc_sobel);
    row("Magnitude", cyc_mag);
    row("Direction", cyc_dir);

    std::cout << "├──────────────┼────────────────┼────────┼──────────────┤\n";
    std::cout << "│ " << std::left  << std::setw(12) << "TOTAL"
              << " │ " << std::right << std::setw(14) << total
              << " │ 100.0% │              │\n";
    std::cout << "└──────────────┴────────────────┴────────┴──────────────┘\n";

    // Amdahl's Law note — tells you where to focus optimization effort
    std::cout << "\n";
    std::cout << "  Optimization priority (Amdahl's Law):\n";
    std::cout << "  Gaussian  takes " << std::fixed << std::setprecision(1)
              << 100.0*cyc_gaussian/total << "% → worth vectorizing with RVV\n";
    std::cout << "  Magnitude takes " << 100.0*cyc_mag/total
              << "% → worth vectorizing with RVV\n";
    std::cout << "  Direction takes " << 100.0*cyc_dir/total
              << "% → NOT worth optimizing (too small)\n";
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main()
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: ./canny <input.raw> <output.raw> <width> <height>\n";
        std::cerr << "Example: ./canny test_input.raw out.raw 640 480\n";
        return 1;
    }

    int W = std::stoi(argv[3]);   // image width  (pixels)
    int H = std::stoi(argv[4]);   // image height (pixels)

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║       Canny Edge Detection Pipeline          ║\n";
    std::cout << "║       Running on RISC-V via QEMU             ║\n";
    std::cout << "╠══════════════════════════════════════════════╣\n";
    std::cout << "║  Input : " << std::left << std::setw(36) << argv[1] << "║\n";
    std::cout << "║  Output: " << std::left << std::setw(36) << argv[2] << "║\n";
    std::cout << "║  Size  : " << W << "x" << H
              << std::string(36 - std::to_string(W).size()
                                - std::to_string(H).size() - 1, ' ') << "║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n\n";

    // ── Allocate memory buffers ───────────────────────────────────────────────
    // aligned_alloc(64, ...) allocates memory aligned to 64-byte boundaries.
    // This is required for SIMD/vector operations later (RVV phase).
    uint8_t* img_in   = allocate_buffer(W, H);  // original image
    uint8_t* img_blur = allocate_buffer(W, H);  // after gaussian blur
    int16_t* gx = (int16_t*)aligned_alloc(64, W * H * 2);  // horizontal gradient
    int16_t* gy = (int16_t*)aligned_alloc(64, W * H * 2);  // vertical gradient
    uint8_t* img_mag  = allocate_buffer(W, H);  // gradient magnitude (edge strength)
    uint8_t* img_dir  = allocate_buffer(W, H);  // gradient direction (edge angle)

    if (!img_in || !img_blur || !gx || !gy || !img_mag || !img_dir) {
        std::cerr << "ERROR: Memory allocation failed!\n";
        return 1;
    }

    // ── Load input image ──────────────────────────────────────────────────────
    std::cout << "[ 1/6 ] Loading image ...\n";
    if (!load_raw(argv[1], img_in, W, H)) {
        std::cerr << "ERROR: Could not open file: " << argv[1] << "\n";
        std::cerr << "       Make sure the file exists and the path is correct.\n";
        return 1;
    }
    std::cout << "[ OK  ] Image loaded (" << W*H << " pixels)\n\n";

    // ── How many times to repeat each stage for stable timing ────────────────
    // Running once gives noisy results (QEMU may be doing other things).
    // Running 100 times and averaging gives stable, reproducible numbers.
    const int REPS = 100;
    uint64_t c0, c1;

    // ── Stage 1: Gaussian Blur ────────────────────────────────────────────────
    // Purpose: reduce noise in the image before finding edges.
    // A noisy image has many tiny fake edges — blur removes them.
    // Uses a 5×5 kernel (25 multiplications per pixel).
    std::cout << "[ 2/6 ] Gaussian blur (5x5, sigma≈1.0) ...\n";
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        gaussian_blur_5x5<uint8_t, uint8_t, int32_t>(img_in, img_blur, W, H);
    c1 = read_cycles();
    uint64_t cyc_gaussian = (c1 - c0) / REPS;
    std::cout << "[ OK  ] Done — " << cyc_gaussian << " cycles avg\n\n";

    // ── Stage 2: Sobel Gradient ───────────────────────────────────────────────
    // Purpose: find where pixel intensity changes sharply (= edges).
    // Applies two 3×3 kernels: one detects horizontal changes (Gx),
    // one detects vertical changes (Gy).
    std::cout << "[ 3/6 ] Sobel gradient (Gx and Gy) ...\n";
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_sobel(img_blur, gx, gy, W, H);
    c1 = read_cycles();
    uint64_t cyc_sobel = (c1 - c0) / REPS;
    std::cout << "[ OK  ] Done — " << cyc_sobel << " cycles avg\n\n";

    // ── Stage 3: Gradient Magnitude ───────────────────────────────────────────
    // Purpose: combine Gx and Gy into one number = "how strong is this edge?"
    // L1 norm: |Gx| + |Gy|  (fast, good enough)
    // L2 norm: sqrt(Gx²+Gy²) (more accurate but slower)
    // Result is normalized to 0–255 so we can save it as an image.
    std::cout << "[ 4/6 ] Gradient magnitude (L1 norm) ...\n";
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_magnitude_l1(gx, gy, img_mag, W, H);
    c1 = read_cycles();
    uint64_t cyc_mag = (c1 - c0) / REPS;
    std::cout << "[ OK  ] Done — " << cyc_mag << " cycles avg\n\n";

    // ── Stage 4: Gradient Direction ───────────────────────────────────────────
    // Purpose: figure out which direction each edge is pointing.
    // We quantize to 4 bins: 0°(horizontal), 45°, 90°(vertical), 135°
    // Used later in non-maximum suppression (Phase 6 bonus).
    std::cout << "[ 5/6 ] Gradient direction ...\n";
    c0 = read_cycles();
    for (int i = 0; i < REPS; i++)
        compute_direction(gx, gy, img_dir, W, H);
    c1 = read_cycles();
    uint64_t cyc_dir = (c1 - c0) / REPS;
    std::cout << "[ OK  ] Done — " << cyc_dir << " cycles avg\n\n";

    // ── Save output ───────────────────────────────────────────────────────────
    std::cout << "[ 6/6 ] Saving output to " << argv[2] << " ...\n";
    save_raw(argv[2], img_mag, W, H);
    std::cout << "[ OK  ] Saved\n";

    // ── Print results table ───────────────────────────────────────────────────
    print_table(cyc_gaussian, cyc_sobel, cyc_mag, cyc_dir, REPS);

    return 0;
}
