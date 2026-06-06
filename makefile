# ─── Compilers ───────────────────────────────────────────────────────────────
# HOST compiler  : native g++ on your Ubuntu machine (for tests)
# RV compiler    : cross-compiler that produces RISC-V binaries
HOST_CXX  = g++
RV_CXX    = riscv64-linux-gnu-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
# rv64gc  = RISC-V 64-bit + standard extensions (no vector yet)
# lp64d   = 64-bit pointers, double-precision float ABI
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
RV_FLAGS   = -std=c++17 -O2 -march=rv64gc -mabi=lp64d -I src -I include

# ─── QEMU ────────────────────────────────────────────────────────────────────
# -L flag tells QEMU where the RISC-V Linux libraries are on your Ubuntu machine
QEMU       = qemu-riscv64
QEMU_FLAGS = -L /usr/riscv64-linux-gnu

# ─── Source files (all pipeline stages) ──────────────────────────────────────
SRCS = src/main.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp

# ─── Image settings (override with: make run IMG=myfile.raw W=320 H=240) ─────
IMG ?= test_input.raw
# Auto-read size from last convert — override with W=xxx H=xxx if needed
_SIZE := $(shell cat .img_size 2>/dev/null || echo "640 480")
W     ?= $(word 1,$(_SIZE))
H     ?= $(word 2,$(_SIZE))

# ─── Default target ───────────────────────────────────────────────────────────
.PHONY: all run sweep sweep_O0 sweep_O2 sweep_O3 sizes convert view clean help

all: build_rv/canny_rv

help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════╗"
	@echo "║           Canny Edge Detection - RISC-V              ║"
	@echo "╠══════════════════════════════════════════════════════╣"
	@echo "║  make run              → build O2 and run on QEMU    ║"
	@echo "║  make sweep            → run O0 / O2 / O3 and table  ║"
	@echo "║  make convert IMG=x.jpg→ convert photo to .raw       ║"
	@echo "║  make view             → view output as PNG           ║"
	@echo "║  make clean            → remove build files           ║"
	@echo "╚══════════════════════════════════════════════════════╝"
	@echo ""

# ─── Build (O2, default) ──────────────────────────────────────────────────────
build_rv/canny_rv: $(SRCS)
	@mkdir -p build_rv
	@echo "[ BUILD ] Compiling for RISC-V with -O2 ..."
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $@
	@echo "[ OK    ] build_rv/canny_rv ready"

# ─── Run on QEMU ─────────────────────────────────────────────────────────────
run: build_rv/canny_rv
	@echo ""
	@echo "[ QEMU  ] Running on RISC-V emulator ..."
	@echo "[ INPUT ] $(IMG)  $(W)x$(H)"
	@echo ""
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_rv ./$(IMG) ./out.raw $(W) $(H)

# ─── Optimization Sweep ──────────────────────────────────────────────────────
sweep_O0:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O0 ..."
	@$(RV_CXX) -std=c++17 -O0 -march=rv64gc -mabi=lp64d -I src -I include \
	  $(SRCS) -o build_rv/canny_O0
	@echo "[ RUN   ] -O0 on QEMU ($(W)x$(H)) ..."
	@$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O0 \
	  ./test_input.raw ./out_O0.raw $(W) $(H) cycles_O0.txt

sweep_O2:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O2 ..."
	@$(RV_CXX) -std=c++17 -O2 -march=rv64gc -mabi=lp64d -I src -I include \
	  $(SRCS) -o build_rv/canny_O2
	@echo "[ RUN   ] -O2 on QEMU ($(W)x$(H)) ..."
	@$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O2 \
	  ./test_input.raw ./out_O2.raw $(W) $(H) cycles_O2.txt

sweep_O3:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O3 ..."
	@$(RV_CXX) -std=c++17 -O3 -march=rv64gc -mabi=lp64d -I src -I include \
	  $(SRCS) -o build_rv/canny_O3
	@echo "[ RUN   ] -O3 on QEMU ($(W)x$(H)) ..."
	@$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O3 \
	  ./test_input.raw ./out_O3.raw $(W) $(H) cycles_O3.txt

# Build the summary binary (host-side, reads saved cycle files)
build_host/summary:
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) src/summary.cpp -o build_host/summary

# Run sweep then immediately print comparison table
sweep: sweep_O0 sweep_O2 sweep_O3 build_host/summary sizes
	@echo ""
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt
	@echo "[ DONE  ] Sweep complete."

# Print table from last sweep (no re-running!)
table: build_host/summary
	@echo "[ TABLE ] Reading results from last sweep..."
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt
sizes:
	@echo ""
	@echo "┌─────────────────────────────────────┐"
	@echo "│         Binary Size Summary          │"
	@echo "├──────────────────┬──────────────────┤"
	@echo "│ Flag             │ Size             │"
	@echo "├──────────────────┼──────────────────┤"
	@printf "│ -O0              │ %-16s │\n" $$(ls -lh build_rv/canny_O0 | awk '{print $$5}')
	@printf "│ -O2              │ %-16s │\n" $$(ls -lh build_rv/canny_O2 | awk '{print $$5}')
	@printf "│ -O3              │ %-16s │\n" $$(ls -lh build_rv/canny_O3 | awk '{print $$5}')
	@echo "└──────────────────┴──────────────────┘"

# ─── Image conversion ─────────────────────────────────────────────────────────
# Usage: make convert IMG=photo.jpg
# This converts any photo to the raw grayscale format our pipeline needs
convert:
	@echo "[ CONV  ] Converting $(IMG) to raw grayscale ..."
	@python3 -c "\
from PIL import Image; import numpy as np; \
img = Image.open('$(IMG)').convert('L'); \
arr = np.array(img); \
arr.tofile('test_input.raw'); \
h, w = arr.shape; \
open('.img_size', 'w').write(str(w)+' '+str(h)); \
print('[ OK    ] Saved test_input.raw  size: {}x{}'.format(w, h)); \
print('[ INFO  ] Now just run: make run')"

# ─── View output ──────────────────────────────────────────────────────────────
# Converts out.raw back to PNG so you can see the edge detection result
view:
	@echo "[ VIEW  ] Converting out.raw to out.png ..."
	@python3 -c "\
import numpy as np; from PIL import Image; \
arr = np.fromfile('out.raw', dtype=np.uint8).reshape($(H), $(W)); \
Image.fromarray(arr).save('out.png'); \
print('[ OK    ] Saved out.png')"
	@echo "[ OPEN  ] Opening image ..."
	@eog out.png 2>/dev/null || xdg-open out.png 2>/dev/null || \
	  echo "[ INFO  ] Cannot open GUI. Copy out.png to Windows and open it."

# ─── Verify outputs match ─────────────────────────────────────────────────────
verify:
	@echo "[ CHECK ] Verifying O0 == O2 == O3 (correctness check) ..."
	@python3 -c "\
import numpy as np; \
o0=np.fromfile('out_O0.raw',dtype=np.uint8); \
o2=np.fromfile('out_O2.raw',dtype=np.uint8); \
o3=np.fromfile('out_O3.raw',dtype=np.uint8); \
print('  O0 vs O2:', 'IDENTICAL ✓' if np.array_equal(o0,o2) else 'DIFFER ✗  max diff='+str(np.max(np.abs(o0.astype(int)-o2.astype(int))))); \
print('  O0 vs O3:', 'IDENTICAL ✓' if np.array_equal(o0,o3) else 'DIFFER ✗  max diff='+str(np.max(np.abs(o0.astype(int)-o3.astype(int))))); \
print('  Non-zero pixels (edges found):', np.count_nonzero(o0), 'out of', len(o0))"

# ─── Clean ────────────────────────────────────────────────────────────────────
clean:
	@rm -rf build_rv/* build_host/*
	@rm -f out.raw out_O0.raw out_O2.raw out_O3.raw out.png
	@echo "[ OK    ] Cleaned all build files"
	
	
