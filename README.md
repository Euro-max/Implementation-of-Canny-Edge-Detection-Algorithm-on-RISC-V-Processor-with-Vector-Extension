# Canny Edge Detection on RISC-V — Project README

Implementation of a Canny edge detection pipeline (Gaussian blur → Sobel gradients →
magnitude → direction), cross-compiled for RISC-V and run on QEMU. This README covers
setup, build/run instructions, and what's been done so far.

---

## Prerequisites

- **Toolchain:** `riscv64-unknown-elf-g++` built from source with `--with-arch=rv64gcv`
  (the apt package does not support RVV intrinsics reliably)
- **Emulator:** `qemu-riscv64` (QEMU 9.x+, built from source recommended)
- **Host tools:** `g++` (for host-side tests), Python 3 with `numpy` + `Pillow`
- WSL2 + Ubuntu 24.04 if on Windows; native Linux otherwise

---

## Project Structure

```
src/
  main.cpp        — pipeline driver, timing, results table
  syscalls.cpp     — bare-metal syscall shims (needed for file I/O under QEMU)
  image_io.cpp     — raw grayscale image load/save
  gaussian.h/.ipp   — templated 5x5 Gaussian blur
  sobel.cpp        — 3x3 Sobel gradient (Gx, Gy)
  magnitude.cpp     — L1 and L2 gradient magnitude
  direction.cpp     — 4-way gradient direction (0/45/90/135°)
  summary.cpp      — builds the sweep comparison table
Makefile
```

---

## Quick Start

```bash
make convert IMG=yourphoto.jpg   # convert any photo to raw grayscale (auto-detects size)
make run                         # build (-O2) and run the pipeline on QEMU
make view                        # convert output back to PNG and open it
```

You never need to manually set width/height — `make convert` saves them and every other
target reads them automatically.

---

## Makefile Targets

| Target | What it does |
|---|---|
| `make run` | Build with `-O2`, run once on QEMU, print per-stage cycle counts |
| `make convert IMG=x.jpg` | Convert a photo to `test_input.raw`, auto-detect size |
| `make view` | Convert `out.raw` to `out.png` and open it |
| `make sweep` | Build and run at `-O0 -O2 -O3 -Os -Ofast`, print comparison table |
| `make verify` | Confirm all optimization levels produce identical output |
| `make sizes` | Print binary size at each optimization level |
| `make table` | Print the Phase 4 report-style summary table |
| `make clean` | Remove all build artifacts |
| `make help` | Print this list in the terminal |

Override image size manually if needed: `make run W=640 H=480`. Override vector length:
`make run VLEN=256`.

---

## Phase 1 — Environment Setup ✅

- Built `riscv64-unknown-elf-g++` from source with RVV (Vector extension) support
- Built `qemu-riscv64` from source (user-mode only, for speed)
- Verified the toolchain by running an RVV test program on QEMU
- Set up dual-target Makefile (host tests vs. RISC-V cross-compile)

**Gotcha:** standard `qemu-riscv64 -cpu rv64,...` flag strings can sometimes cause
"Illegal instruction" crashes depending on QEMU build — using QEMU's **default CPU**
(no `-cpu` flag, or just `-cpu rv64,v=true,vlen=N`) is more reliable in this setup.

---

## Phase 2 — Scalar Baseline Pipeline ✅

Implemented in clean, templated C++:

- **Image I/O** — raw grayscale format (just `width*height` bytes, no headers)
- **Gaussian blur** — 5×5 kernel, integer arithmetic, zero-padding boundary, templated
  on pixel/output/accumulator type
- **Sobel gradient** — 3×3 Gx/Gy kernels, output stored as separate int16 arrays
  (Structure-of-Arrays layout, for easier vectorization later)
- **Magnitude** — both L1 (`|Gx|+|Gy|`, fast) and L2 (`sqrt(Gx²+Gy²)`, accurate)
  implemented; pipeline uses L1
- **Direction** — quantized to 4 bins (0°/45°/90°/135°) using integer
  cross-multiplication instead of `atan2()`

---

## Phase 3 — Testing ✅

- Correctness verified by checking all optimization levels (`-O0` through `-Ofast`)
  produce **bit-identical output**: `make verify`
- Verified against a real photo (720×900), confirming reasonable edge detection
  (~39% of pixels flagged as edges, which is expected for natural images with texture)

---

## Phase 4 — Compiler Optimization Sweep & Auto-Vectorization ✅

Two parts — full detail in `README_Phase4.md`, summary here:

### Optimization sweep

Measured cycle counts (via `rdcycle`, averaged over 100 runs) at 5 optimization levels.
Result: **~4.2× total speedup from `-O0` to `-Ofast`**, with zero source code changes.

| Stage | -O0 | -O2 | -O3 | -Os | -Ofast |
|---|---|---|---|---|---|
| Gaussian | 534M | 187M | 215M | 215M | 209M |
| Sobel | 269M | 107M | 18.7M | 99M | 18.2M |
| Magnitude | 454M | 66M | 68M | 74M | 68M |
| Direction | 33M | 16M | 16M | 18M | 16M |
| **TOTAL** | **1290M** | **376M** | **317M** | **406M** | **310M** |

All five produce identical output (`make verify`).

### Auto-vectorization analysis

Using `-fopt-info-vec-all` and disassembly, checked which functions the compiler
auto-vectorized with RVV:

| Function | Vectorized? |
|---|---|
| Gaussian | ❌ No — 4-level nested loop too complex for GCC |
| Sobel | ❌ No — boundary check (`if`) inside loop blocks it |
| Magnitude | ⚠️ Half — the "find max" pass is vectorized, the "normalize" pass isn't (float math) |
| Direction | ✅ Fully — compiler converted if/else chain into branchless masked operations |

**Conclusion for Phase 6:** Gaussian needs full hand-written RVV (biggest runtime share,
zero compiler help). Magnitude needs RVV only for the normalization step. Sobel and
Direction are lower priority — the compiler already did most or all of the work.

---

## Phase 5 — Profiling ✅ (folded into Phase 4)

Per-stage percentage breakdown is printed automatically every run:

```
Gaussian  : 45.2%   ◄ HOT
Sobel     :  9.9%
Magnitude : 36.2%
Direction :  8.6%
```

Gaussian + Magnitude = ~81% of runtime → these are the Phase 6 targets. Per Amdahl's
Law, optimizing Direction further would save under 9% even if made infinitely fast, so
it's deprioritized.

---

## Phase 6 — RVV Intrinsic Optimization 🔜 (next up)

Plan based on Phase 4 findings:

1. **Gaussian** — rewrite using RVV intrinsics. Process the image interior with no
   boundary checks (handle the 2-pixel border separately in scalar code) to avoid the
   structural issues that blocked auto-vectorization.
2. **Magnitude** — keep the compiler's auto-vectorized "find max" pass; hand-write RVV
   only for the normalization pass, using a fixed-point multiply+shift instead of
   floating-point divide.
3. **Sobel / Direction** — lower priority, optional.
4. Verify scalar vs. RVV output match (±1 tolerance) at VLEN = 128, 256, and 512.

---

## Phase 7 — Report & Presentation 🔜

- Final optimization table (scalar through RVV, multiple VLEN values)
- Code walkthrough comments on every RVV intrinsic (what it does, why this LMUL, what
  changes at a different VLEN)
- Live demo on QEMU

---

## Key Files Generated So Far

- `README_Phase4.md` — detailed write-up of the optimization sweep and vectorization
  analysis, with annotated disassembly
- `vec_report.txt` — raw compiler vectorization diagnostic output
- `cycles_*.txt` — per-stage cycle counts for each optimization level
