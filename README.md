# Phase 4: Compiler Optimization Sweep

## What We Did In This Phase

We took the scalar pipeline from Phase 3 and compiled it at **5 different optimization levels** to see how much speedup the compiler gives us for free — before writing any RVV code.

We also added **cycle-accurate timing** to every pipeline stage so we know exactly where the time is going.

---

## New Files Added

```
src/main.cpp        → rewrote with rdcycle timing + saves results to .txt files
src/summary.cpp     → reads saved results, prints the comparison table
makefile            → added sweep, sweep_O0/O2/O3, table, convert, verify targets
```

---

## How to Run

```bash
# Step 1: convert your photo (only need to do this once per image)
make convert IMG=images/pic.jpg

# Step 2: run the full optimization sweep
make sweep

# Step 3: reprint the table anytime without re-running
make table
```

---

## How The Timing Works

We use the RISC-V `rdcycle` hardware instruction — reads the CPU cycle counter before and after each stage, subtracts to get elapsed cycles. Each stage runs **100 times** and we average to eliminate noise from QEMU.

```cpp
static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// Usage:
c0 = read_cycles();
for (int i = 0; i < 100; i++)
    gaussian_blur_5x5(...);
c1 = read_cycles();
uint64_t avg = (c1 - c0) / 100;
```

> **Why not clock_gettime?** Our bare-metal toolchain (riscv64-unknown-elf) has conflicts between time.h and C++ headers. `rdcycle` needs zero headers and works everywhere.

> **Why 100 repetitions?** QEMU runs on your host OS alongside other processes. A single measurement might catch an OS interrupt and read 20% too high. 100 runs averaged is stable within 1-2%.

---

## How Results Are Saved

When the sweep runs each binary, it passes a 6th argument — a filename to save the cycle counts:

```bash
# In the Makefile:
qemu-riscv64 ... ./build_rv/canny_O2 test_input.raw out_O2.raw 458 260 cycles_O2.txt
```

`main.cpp` writes 4 numbers (one per stage) to that file:

```
35462501      ← Gaussian cycles
33187203      ← Sobel cycles
11831807      ← Magnitude cycles
3012386       ← Direction cycles
```

Then `summary.cpp` reads all 3 files (O0, O2, O3) and prints the comparison table from the **exact saved numbers** — no re-running, no variability.

---

## Results (image: pic.jpg, 458×260 pixels)

```
┌────────────┬────────────────────────┬────────────────────────┬────────────────────────┬─────────────┐
│ Stage      │ -O0 Cycles   % total   │ -O2 Cycles   % total   │ -O3 Cycles   % total   │ O0 → O3     │
├────────────┼────────────────────────┼────────────────────────┼────────────────────────┼─────────────┤
│ Gaussian   │  100,112,203   25.1%   │   35,462,501   42.5%   │   15,225,381   45.8%   │   6.6x      │
│ Sobel      │  225,083,352   56.4%   │   33,187,203   39.7%   │    3,193,041    9.6%   │  70.5x !!   │
│ Magnitude  │   67,527,451   16.9%   │   11,831,807   14.2%   │   11,778,039   35.4%   │   5.7x      │
│ Direction  │    6,624,799    1.7%   │    3,012,386    3.6%   │    3,030,317    9.1%   │   2.2x      │
├────────────┼────────────────────────┼────────────────────────┼────────────────────────┼─────────────┤
│ TOTAL      │  399,347,805  100.0%   │   83,493,897  100.0%   │   33,226,778  100.0%   │  12.0x      │
└────────────┴────────────────────────┴────────────────────────┴────────────────────────┴─────────────┘
```

### Binary Sizes
| Flag | Size |
|------|------|
| -O0  | 64 KB |
| -O2  | 30 KB |
| -O3  | 30 KB |

---

## What The Numbers Mean

### Sobel: 70.5x speedup (O0 → O3)
The compiler **auto-vectorized** Sobel at `-O3`. The Sobel inner loop is simple and branchless, so GCC rewrote it using SIMD instructions automatically. This is why Sobel went from 56% of total time at O0 down to only 9.6% at O3.

### Gaussian: only 6.6x speedup
The Gaussian inner loop has a boundary check:
```cpp
if (img_y < 0 || img_y >= height || img_x < 0 || img_x >= width)
    continue;  // zero padding
```
This `if` statement inside the loop **prevents auto-vectorization** — the compiler cannot process multiple pixels in parallel if different pixels take different code paths. This is exactly why we need RVV intrinsics in Phase 7.

### Magnitude: 5.7x speedup
Two-pass structure (find max, then normalize) creates a data dependency between passes. The compiler can't vectorize across passes. Also a target for Phase 7.

### Direction: 2.2x speedup
Already fast enough. Not worth optimizing further (see Amdahl's Law below).

---

## Amdahl's Law — Where to Focus Phase 7

Amdahl's Law says: the total speedup you gain is limited by the percentage of time spent in the part you optimize.

After -O3, the bottlenecks are:

```
Gaussian:  45.8% of total time  → RVV will help a lot here
Magnitude: 35.4% of total time  → RVV will help here too
Sobel:      9.6% of total time  → compiler already handled it, skip
Direction:  9.1% of total time  → too small, skip
```

If we make Gaussian + Magnitude 10x faster with RVV:

```
New total = (15.2M ÷ 10) + 3.2M + (11.8M ÷ 10) + 3.0M
          = 1.52M + 3.2M + 1.18M + 3.0M
          = ~8.9M cycles

Speedup vs O3   = 33.2M ÷ 8.9M  = 3.7x more
Speedup vs O0   = 12.0x × 3.7x  = ~44x total
```

---

AH
