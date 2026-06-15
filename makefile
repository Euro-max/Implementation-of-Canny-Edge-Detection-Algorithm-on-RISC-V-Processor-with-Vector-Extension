# ─── Compilers ───────────────────────────────────────────────────────────────
# HOST compiler  : native g++ on your Ubuntu machine (for tests)
# RV compiler    : bare-metal cross-compiler (built from source, full RVV support)
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++                        # CHANGED

# ─── Flags ───────────────────────────────────────────────────────────────────
# rv64gcv = RISC-V 64-bit + standard extensions + Vector Extension (v added!)
# lp64d   = 64-bit pointers, double-precision float ABI
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include   # was rv64gcv

# ─── QEMU ────────────────────────────────────────────────────────────────────
# No -L flag needed: bare-metal binaries are statically linked
# (riscv64-unknown-elf links against newlib, not glibc)
QEMU       = qemu-riscv64
QEMU_FLAGS = -cpu rv64,v=true,vlen=128                     # CHANGED

# ─── Source files (all pipeline stages) ──────────────────────────────────────
SRCS = src/main.cpp src/syscalls.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp

# ─── Image settings (override with: make run IMG=myfile.raw W=320 H=240) ─────
IMG ?= test_input.raw
# Auto-read size from last convert — override with W=xxx H=xxx if needed
_SIZE := $(shell cat .img_size 2>/dev/null || echo "640 480")
W     ?= $(word 1,$(_SIZE))
H     ?= $(word 2,$(_SIZE))

# ─── VLEN setting (override with: make run VLEN=256) ─────────────────────────
VLEN ?= 128

# ─── Default target ───────────────────────────────────────────────────────────
.PHONY: all run canny_rv sweep sweep_O0 sweep_O2 sweep_O3 sizes convert view clean help

all: build_rv/canny_rv

help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════╗"
	@echo "║           Canny Edge Detection - RISC-V              ║"
	@echo "╠══════════════════════════════════════════════════════╣"
	@echo "║  make run              -> build O2 and run on QEMU   ║"
	@echo "║  make run VLEN=256     -> run with wider vectors      ║"
	@echo "║  make sweep            -> run O0 / O2 / O3 and table ║"
	@echo "║  make convert IMG=x.jpg-> convert photo to .raw      ║"
	@echo "║  make view             -> view output as PNG          ║"
	@echo "║  make clean            -> remove build files          ║"
	@echo "╚══════════════════════════════════════════════════════╝"
	@echo ""

# Shortcut alias
canny_rv: build_rv/canny_rv
# ─── Build (O2, default) ──────────────────────────────────────────────────────
build_rv/canny_rv: $(SRCS)
	@mkdir -p build_rv
	@echo "[ BUILD ] Compiling for RISC-V with -O2 ..."
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $@
	@echo "[ OK    ] build_rv/canny_rv ready"

# ─── Run on QEMU ─────────────────────────────────────────────────────────────
# ─── Run on QEMU ─────────────────────────────────────────────────────────────
run: build_rv/canny_rv
	@echo ""
	@echo "[ QEMU  ] Running on RISC-V emulator (VLEN=$(VLEN)) ..."
	@echo "[ INPUT ] $(IMG)  $(W)x$(H)"
	@echo ""
	@touch out.raw
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_rv ./$(IMG) ./out.raw $(W) $(H)

# ─── Optimization Sweep ──────────────────────────────────────────────────────
sweep_O0:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O0 ..."
	@$(RV_CXX) -std=c++17 -O0 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include \
	  $(SRCS) -o build_rv/canny_O0
	@echo "[ RUN   ] -O0 on QEMU ($(W)x$(H)) ..."
	@touch out_O0.raw cycles_O0.txt
	@$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_O0 \
	  ./test_input.raw ./out_O0.raw $(W) $(H) cycles_O0.txt

sweep_O2:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O2 ..."
	@$(RV_CXX) -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include \
	  $(SRCS) -o build_rv/canny_O2
	@echo "[ RUN   ] -O2 on QEMU ($(W)x$(H)) ..."
	@touch out_O2.raw cycles_O2.txt
	@$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_O2 \
	  ./test_input.raw ./out_O2.raw $(W) $(H) cycles_O2.txt

sweep_O3:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -O3 ..."
	@$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include \
	  $(SRCS) -o build_rv/canny_O3
	@echo "[ RUN   ] -O3 on QEMU ($(W)x$(H)) ..."
	@touch out_O3.raw cycles_O3.txt
	@$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_O3 \
	  ./test_input.raw ./out_O3.raw $(W) $(H) cycles_O3.txt

sweep_Os:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -Os ..."
	@$(RV_CXX) -std=c++17 -Os -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include \
	  $(SRCS) -o build_rv/canny_Os
	@echo "[ RUN   ] -Os on QEMU ($(W)x$(H)) ..."
	@touch out_Os.raw cycles_Os.txt
	@$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_Os \
	  ./test_input.raw ./out_Os.raw $(W) $(H) cycles_Os.txt

sweep_Ofast:
	@mkdir -p build_rv build_host
	@echo "[ BUILD ] Compiling -Ofast ..."
	@$(RV_CXX) -std=c++17 -Ofast -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include \
	  $(SRCS) -o build_rv/canny_Ofast
	@echo "[ RUN   ] -Ofast on QEMU ($(W)x$(H)) ..."
	@touch out_Ofast.raw cycles_Ofast.txt
	@$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./build_rv/canny_Ofast \
	  ./test_input.raw ./out_Ofast.raw $(W) $(H) cycles_Ofast.txt
# ─── Summary table ────────────────────────────────────────────────────────────
build_host/summary:
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) src/summary.cpp -o build_host/summary

sweep: sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast build_host/summary sizes
	@echo ""
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt
	@echo "[ DONE  ] Sweep complete."

# ─── Summary table ────────────────────────────────────────────────────────────
table:
	@echo ""
	@echo "Phase 4: Optimization Sweep Results"
	@echo "==================================="
	@echo "The following table summarizes the impact of compiler optimization flags on binary size, runtime, and vectorization behavior for the Canny Edge pipeline:"
	@echo ""
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@echo "| Optimization | Binary Size | Runtime (Cycles & Seconds)    | Vectorization Notes |"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@printf "| %-12s | %-11s | %-29s | %-19s |\n" "-O0" $$(ls -lh build_rv/canny_O0 2>/dev/null | awk '{print $$5}') "$$(awk '{s+=$$1} END {printf "%d (%.4fs)", s, s/1000000000}' cycles_O0.txt 2>/dev/null)" "None"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@printf "| %-12s | %-11s | %-29s | %-19s |\n" "-O2" $$(ls -lh build_rv/canny_O2 2>/dev/null | awk '{print $$5}') "$$(awk '{s+=$$1} END {printf "%d (%.4fs)", s, s/1000000000}' cycles_O2.txt 2>/dev/null)" "Some loops"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@printf "| %-12s | %-11s | %-29s | %-19s |\n" "-O3" $$(ls -lh build_rv/canny_O3 2>/dev/null | awk '{print $$5}') "$$(awk '{s+=$$1} END {printf "%d (%.4fs)", s, s/1000000000}' cycles_O3.txt 2>/dev/null)" "More aggressive"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@printf "| %-12s | %-11s | %-29s | %-19s |\n" "-Os" $$(ls -lh build_rv/canny_Os 2>/dev/null | awk '{print $$5}') "$$(awk '{s+=$$1} END {printf "%d (%.4fs)", s, s/1000000000}' cycles_Os.txt 2>/dev/null)" "Size-focused"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@printf "| %-12s | %-11s | %-29s | %-19s |\n" "-Ofast" $$(ls -lh build_rv/canny_Ofast 2>/dev/null | awk '{print $$5}') "$$(awk '{s+=$$1} END {printf "%d (%.4fs)", s, s/1000000000}' cycles_Ofast.txt 2>/dev/null)" "Max speed"
	@echo "+--------------+-------------+-------------------------------+---------------------+"
	@echo ""
sizes:
	@echo ""
	@echo "+-----------------------+"
	@echo "|  Binary Size Summary  |"
	@echo "+----------+------------+"
	@echo "| Flag     | Size       |"
	@echo "+----------+------------+"
	@printf "| -O0      | %-10s |\n" $$(ls -lh build_rv/canny_O0 | awk '{print $$5}')
	@printf "| -O2      | %-10s |\n" $$(ls -lh build_rv/canny_O2 | awk '{print $$5}')
	@printf "| -O3      | %-10s |\n" $$(ls -lh build_rv/canny_O3 | awk '{print $$5}')
	@printf "| -Os      | %-10s |\n" $$(ls -lh build_rv/canny_Os | awk '{print $$5}')
	@printf "| -Ofast   | %-10s |\n" $$(ls -lh build_rv/canny_Ofast | awk '{print $$5}')
	@echo "+----------+------------+"

# ─── Image conversion ─────────────────────────────────────────────────────────
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
view:
	@echo "[ VIEW  ] Converting out.raw to out.png ..."
	@python3 -c "\
import numpy as np; from PIL import Image; \
arr = np.fromfile('out.raw', dtype=np.uint8).reshape($(H), $(W)); \
Image.fromarray(arr).save('out.png'); \
print('[ OK    ] Saved out.png')"
	@echo "[ INFO  ] Open in Windows Explorer:"
	@echo "          \\\\wsl.localhost\\Ubuntu\\home\\$$USER\\canny-edge\\out.png"

# ─── Verify outputs match ─────────────────────────────────────────────────────
verify:
	@echo "[ CHECK ] Verifying outputs match ..."
	@python3 -c "\
import numpy as np; \
o0=np.fromfile('out_O0.raw',dtype=np.uint8); \
ofast=np.fromfile('out_Ofast.raw',dtype=np.uint8); \
print('  O0 vs Ofast:', 'IDENTICAL' if np.array_equal(o0,ofast) else 'DIFFER  max diff='+str(np.max(np.abs(o0.astype(int)-ofast.astype(int))))); \
print('  Edge pixels:', np.count_nonzero(o0), 'out of', len(o0))"

# ─── Clean ────────────────────────────────────────────────────────────────────
clean:
	@rm -rf build_rv/* build_host/*
	@rm -f out*.raw out.png cycles_*.txt
	@echo "[ OK    ] Cleaned all build files"
