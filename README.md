Hey team 👋
Here's what I did:

✅ Gaussian Blur (gaussian.h / gaussian.ipp)
- 5x5 kernel with correct sum of 273
- Zero-padding boundary handling
- Template design (T_in, T_out, T_acc)

✅ Magnitude (magnitude.h / magnitude.cpp)
- L1 norm: |Gx| + |Gy|
- L2 norm: sqrt(Gx² + Gy²)
- Two-pass normalization to [0, 255]

✅ Unit Tests (tests/ folder)
- test_gaussian.cpp → 4/4 passing
- test_magnitude.cpp → 4/4 passing
- test_sobel.cpp → 4/4 passing
- test_direction.cpp → 4/4 passing
- test_image_io.cpp → 4/4 passing
- Total: 20/20 tests passing ✅

✅ Full pipeline tested natively on a real image (GOAT.jpg 720x900) and produced correct edge detection output

✅ Makefile updated with:
- make test_gaussian
- make test_magnitude
- make test_sobel
- make test_direction
- make test_image_io
- make test_all (runs everything at once)

Please pull from The_GOAT branch and let me know if anything conflicts with your work!

# Phase 4 — Compiler Optimization Sweep & Auto-Vectorization Analysis

This README documents Phase 4 of the Canny Edge Detection on RISC-V project: measuring
how compiler flags affect performance, and analyzing which loops the compiler could and
could not automatically vectorize.

---

## 1. What Phase 4 Is For

Phase 4 has two parts:

1. **Optimization sweep** — compile the exact same C++ source with five different
   compiler flags (`-O0`, `-O2`, `-O3`, `-Os`, `-Ofast`) and measure how many CPU cycles
   each pipeline stage takes, plus binary size.
2. **Auto-vectorization analysis** — check which loops the compiler turned into RVV
   vector instructions automatically, and which ones it could not, using GCC's
   `-fopt-info-vec-all` diagnostic and confirmation via disassembly.

The goal is to answer two questions data-first, not by guessing:

- How much speed do we get "for free" just by choosing better compiler flags?
- Which pipeline stages still need hand-written RVV intrinsics in Phase 6, and which
  ones the compiler already optimized well enough on its own?

---

## 2. Prerequisites

- `riscv64-unknown-elf-g++` — bare-metal RISC-V cross-compiler, built from source with
  `--with-arch=rv64gcv` (Vector extension support)
- `qemu-riscv64` — QEMU user-mode emulator, version 9.x or newer
- `riscv64-unknown-elf-objdump` / `riscv64-unknown-elf-nm` — come with the toolchain
- Python 3 with `numpy` and `PIL` (Pillow) installed, for image conversion and
  correctness verification

---

## 3. Project Files Used in Phase 4

```
src/main.cpp        — pipeline driver + cycle-counting + results table printer
src/syscalls.cpp     — bare-metal syscall shims (needed because riscv64-unknown-elf
                        has no OS underneath it)
src/image_io.cpp     — raw image load/save
src/gaussian.ipp      — templated 5x5 Gaussian blur (included by gaussian.h)
src/sobel.cpp        — 3x3 Sobel Gx/Gy gradient computation
src/magnitude.cpp     — L1 and L2 gradient magnitude
src/direction.cpp    — 4-way gradient direction quantization
src/summary.cpp      — host-side tool that builds the final comparison table
Makefile             — all targets described below
```

---

## 4. How Timing Works

We measure performance using the RISC-V hardware cycle counter, not wall-clock time,
because QEMU user-mode emulation does not model real time accurately.

```cpp
static inline uint64_t read_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}
```

`rdcycle` is a special RISC-V instruction that reads a hardware register incrementing
once per cycle — like a built-in stopwatch. We read it before and after each pipeline
stage, subtract, and average over **100 repetitions** to smooth out noise from QEMU's
internal scheduling.

```cpp
c0 = read_cycles();
for (int i = 0; i < 100; i++)
    gaussian_blur_5x5(...);
c1 = read_cycles();
uint64_t avg = (c1 - c0) / 100;
```

> **Note on QEMU and cycles:** QEMU user-mode is not cycle-accurate — it does not model
> a real CPU pipeline, cache, or branch predictor. The absolute cycle numbers are really
> closer to *instruction counts*. This means they are not directly comparable to a real
> chip's cycle time, but the **relative comparisons** (`-O0` vs `-O2` vs `-O3`, etc.) are
> valid, because fewer instructions is always faster on real hardware too.

---

## 5. Running the Optimization Sweep

### Quick start

```bash
make convert IMG=yourphoto.jpg   # converts photo to raw grayscale, auto-detects size
make sweep                       # builds and runs all 5 optimization levels
```

This produces:
- `cycles_O0.txt`, `cycles_O2.txt`, `cycles_O3.txt`, `cycles_Os.txt`, `cycles_Ofast.txt`
  — per-stage cycle counts for each flag
- `out_O0.raw` ... `out_Ofast.raw` — pipeline output images for each flag
- A formatted comparison table printed to the terminal (built by `src/summary.cpp`)

### Other useful targets

```bash
make run              # build with -O2 (default) and run once on QEMU
make sizes             # print binary size for each optimization level
make table             # print the Phase 4 report-style summary table
make verify            # confirm O0/O2/O3/Os/Ofast all produce identical output
make view              # convert out.raw back to a viewable PNG
make clean             # remove all build artifacts
```

### ⚠️ Important: why `-march=rv64gc` will NOT work in the sweep

Our bare-metal toolchain (`riscv64-unknown-elf-g++`) was built from source with
`--with-arch=rv64gcv` (Vector extension baked in at build time). It has **no multilib**
for plain `rv64gc` — attempting to compile with `-march=rv64gc` fails with:

```
fatal error: Cannot find suitable multilib set for '-march=rv64imafdc_...'/'-mabi=lp64d'
```

Instead, every sweep target keeps `-march=rv64gcv` but adds two flags to **disable**
auto-vectorization for the basic sweep:

```
-fno-tree-vectorize -fno-tree-slp-vectorize
```

This matters because, without these flags, `rdcycle` counts can behave erratically when
the binary contains RVV vector instructions — we observed `-O2` measuring *slower* than
`-O0` for Gaussian in one run, which was traced back to this exact issue (see Section 7
of the project history for the full debugging story). Disabling auto-vectorization for
the sweep gives clean, monotonically-improving scalar-only numbers. Auto-vectorization
itself is measured **separately** (Section 7 below).

---

## 6. Reading the Sweep Results

A typical sweep (720×900 test image, 648,000 pixels) produces a table like this:

| Stage | -O0 | -O2 | -O3 | -Os | -Ofast |
|---|---|---|---|---|---|
| Gaussian | 534,080,414 | 186,546,293 | 214,641,886 | 214,850,737 | 208,579,005 |
| Sobel | 269,482,450 | 107,174,324 | 18,710,632 | 99,133,508 | 18,194,314 |
| Magnitude | 453,537,361 | 66,273,764 | 67,662,855 | 73,909,404 | 67,777,855 |
| Direction | 32,938,067 | 15,786,386 | 15,566,379 | 18,048,607 | 15,722,199 |
| **TOTAL** | **1,290,038,292** | **375,780,767** | **316,581,752** | **405,942,256** | **310,273,373** |

(numbers are average cycles per run over 100 repetitions; binary sizes: O0=408K, all
others ≈393–395K)

### What each flag does

| Flag | Behavior |
|---|---|
| `-O0` | No optimization. Every variable round-trips through memory. Baseline. |
| `-O2` | Register allocation, function inlining, dead-code elimination. |
| `-O3` | Everything in `-O2` plus aggressive loop unrolling and (when allowed) auto-vectorization. |
| `-Os` | Optimizes for binary size, sometimes at a small speed cost. |
| `-Ofast` | `-O3` plus relaxed floating-point rules (unsafe math optimizations). |

### Key observations

- **Total speedup `-O0` → `-Ofast`: ~4.2×**, achieved with zero source code changes.
- **Sobel improves 14.8×** at `-O3`/`-Ofast` — almost entirely from scalar-level
  optimizations (register allocation, instruction scheduling, loop unrolling), since
  auto-vectorization is disabled in this sweep (see Section 7 for why Sobel still gets
  0 vector instructions even when it *is* allowed).
- **Gaussian at `-O2` (186M) is actually faster than `-O3`/`-Ofast` (214M/208M)** — a
  genuine, reproducible, non-buggy result. With vectorization off, the only extra thing
  `-O3` adds over `-O2` is more aggressive loop unrolling, which for Gaussian's nested
  5×5 kernel loop increases code size and can hurt instruction-cache behavior under
  QEMU's translation layer. This shows optimization is not always monotonic — measure,
  don't assume.
- **Magnitude plateaus quickly** (`-O2` ≈ `-O3` ≈ `-Os` ≈ `-Ofast`, all around 66–74M) —
  consistent with its two-pass structure (find max, then normalize), which limits how
  much further scalar optimization alone can help.

### Correctness verification

After every sweep, run:

```bash
make verify
```

All five binaries (`-O0` through `-Ofast`) must produce **bit-identical output**. This
was confirmed:

```
O0 vs O2.raw    : IDENTICAL ✓
O0 vs O3.raw    : IDENTICAL ✓
O0 vs Os.raw    : IDENTICAL ✓
O0 vs Ofast.raw : IDENTICAL ✓
Edge pixels: 254,071 / 648,000
```

This proves that compiler optimizations changed only **speed**, never the
**mathematical result**.

---

## 7. Auto-Vectorization Analysis

This is a **separate experiment** from the sweep above. Here we deliberately *allow*
vectorization and ask the compiler to explain its decisions for every loop.

### 7.1 Generating the report

```bash
riscv64-unknown-elf-g++ -std=c++17 -O3 -march=rv64gcv -mabi=lp64d \
  -ftree-vectorize -fopt-info-vec-all \
  -I src -I include \
  src/main.cpp src/syscalls.cpp src/image_io.cpp \
  src/sobel.cpp src/magnitude.cpp src/direction.cpp \
  -o build_rv/canny_vec  2> vec_report.txt
```

`-fopt-info-vec-all` makes GCC write one diagnostic line per loop to `vec_report.txt`,
explaining whether it was vectorized and, if not, exactly why.

### 7.2 Confirming in the actual binary

The report shows *intent*; disassembly shows *what actually happened*. RVV vector
instruction mnemonics always start with the letter `v` right after the tab character in
`objdump` output (`vsetvli`, `vle16.v`, `vadd.vv`, `vredmax.vs`, etc.), so this is an
unambiguous test:

```bash
riscv64-unknown-elf-nm build_rv/canny_vec | grep -iE "magnitude|direction|gaussian|sobel"

riscv64-unknown-elf-objdump -d --disassemble=<mangled_function_name> \
  build_rv/canny_vec | grep -P '\tv[a-z]'
```

> **Pitfall to avoid:** don't use a loose `awk`/`grep` pipeline to manually extract a
> function's disassembly — it's easy to accidentally match a *call site* to the function
> (e.g. `jal 11b36 <_Z17compute_direction...>` inside `main`) instead of the function's
> own body. Use objdump's built-in `--disassemble=<symbol>` flag instead — it's exact.

### 7.3 Results

| Function | RVV Instructions Found | Verdict |
|---|---|---|
| `gaussian_blur_5x5` | 0 | Fully scalar |
| `compute_sobel` | 0 | Fully scalar |
| `compute_magnitude_l1` | 17 | Partially vectorized (Pass 1 only) |
| `compute_magnitude_l2` | 0 | Fully scalar (not used in pipeline) |
| `compute_direction` | 35 | Fully vectorized |

### 7.4 Why Gaussian and Sobel got 0 vector instructions

Both functions are written as 4 nested loops (`y → x → ky → kx`):

```
not vectorized: loop nest containing two or more
                consecutive inner loops cannot be vectorized
```

GCC's loop-vectorizer cannot handle this shape at any optimization level. GCC also tried
a smaller-scale technique (SLP — pack 2 elements into one operation for a single
statement) for Gaussian, and rejected it on cost grounds:

```
Vector cost: 21    Scalar cost: 4
not vectorized: vectorization is not profitable.
```

Sobel has a second, independent blocker — its boundary check is a branch inside the
inner loop:

```cpp
if (iy >= 0 && iy < height && ix >= 0 && ix < width) { ... }
```
```
not vectorized: unsupported control flow in loop.
```

A vector instruction processes a whole batch of elements identically in one step; it
cannot easily send some elements down one branch and others down another, so GCC's
auto-vectorizer gives up rather than attempt this here.

**Despite 0 vector instructions, Sobel still achieved a 15× speedup at `-O3`** in the
sweep (when vectorization was disabled) — entirely from scalar-level optimizations
(register allocation, instruction scheduling, partial loop unrolling). This is a useful,
reportable finding: the single biggest measured speedup required zero vector
instructions.

### 7.5 Why Magnitude is half-vectorized

`compute_magnitude_l1` has two passes. Pass 1 (`|gx|+|gy|`, with a running max) **was**
vectorized — GCC used real RVV strip-mining (`"loop vectorized using variable length
vectors"`). Pass 2 (`temp_mag[i] * scale`, normalizing to 0–255) was **not**, because it
mixes an integer with a `float scale`:

```
not vectorized: unsupported data-type
```

The hint guide's fixed-point suggestion — replacing `value * scale` with
`(value * K) >> shift` for a precomputed integer `K` — uses only integer multiply and
shift, which RVV vectorizes easily. This is the concrete fix to try in Phase 6, and may
even let the compiler auto-vectorize Pass 2 on its own.

### 7.6 Why Direction is fully vectorized

GCC converted the `if / else if / else` angle-classification chain into a branchless
**mask-and-merge** sequence: compute a default answer for every pixel in the batch, then
overwrite it (using a comparison mask) wherever a stronger condition applies, in the
same priority order as the original `if` chain. Each `vmerge.vim` instruction means "for
every element where this mask is true, replace its value; otherwise keep the existing
value."

GCC also generated **two versions of the loop** — a fast vectorized path and a safe
scalar fallback — guarded by a runtime check for **pointer aliasing** (whether `gx`,
`gy`, and `output` might overlap in memory). Since `main.cpp` allocates these as
separate buffers, the fast path always runs.

This explains an earlier sweep observation: Direction barely improved from `-O2`
(15.8M cycles) to `-O3` (15.6M cycles) — it was already fully vectorized at `-O2`, so
`-O3` had nothing left to add.

---

## 8. Phase 6 Priority — What This Data Tells Us To Do Next

| Priority | Stage | % Runtime (-O3) | Status | Action |
|---|---|---|---|---|
| 1 — Highest | Gaussian | 45.2% | 0 RVV instructions, structurally blocked | Write full RVV from scratch. 100% of any gain is hand-written. |
| 2 — Medium | Magnitude Pass 2 | ~18–36%* | Pass 1 done by compiler; Pass 2 blocked by float | Apply fixed-point trick, then write RVV or re-check auto-vec. |
| 3 — Low | Sobel | 9.9% | 0 RVV instructions, but `-O3` already gives 15× via scalar opt | Optional — diminishing returns expected. |
| 4 — Skip | Direction | 8.6% | Fully vectorized already by the compiler | Do nothing. Already optimal. |

*Magnitude's reported share of runtime varies (18–36%) depending on which sweep flag is
used as the reference point; Pass 1 (already vectorized) accounts for roughly half of
that total.

This follows directly from **Amdahl's Law**: even an infinitely fast Direction
implementation would save under 9% of total runtime, so it isn't worth the engineering
effort. Gaussian and Magnitude together account for the large majority of remaining
runtime and are where hand-written RVV intrinsics will have real impact.

---

## 9. Troubleshooting Notes (Lessons Learned This Phase)

- **`ifstream`/file I/O silently fails on bare-metal binaries run under plain
  `qemu-riscv64`** if a Linux-targeted toolchain/QEMU sysroot mismatch is used. We
  resolved this by keeping the bare-metal `riscv64-unknown-elf-g++` toolchain
  consistently (which needs `src/syscalls.cpp` for syscall shims) rather than mixing it
  with the Linux-targeted toolchain mid-project.
- **`qemu-riscv64 -cpu rv64` (with no other flags) can be more reliable than an
  explicit, hand-written `-cpu` string** — over-specifying CPU flags caused "Illegal
  instruction" crashes in our environment; using QEMU's default CPU model resolved it.
- **`rdcycle` counts become unreliable in a binary built with `-march=rv64gcv` if
  auto-vectorization is left enabled** for a basic optimization-level sweep — always
  separate the "which flag is fastest" experiment from the "what did the compiler
  auto-vectorize" experiment, using `-fno-tree-vectorize -fno-tree-slp-vectorize` for
  the former.
- Always re-run `make verify` after any Makefile or flag change, before trusting new
  cycle numbers — a correctness regression is a stronger signal of a bug than odd timing
  numbers alone.
