# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include
RVV_FLAGS  = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src -I include

# ─── QEMU ────────────────────────────────────────────────────────────────────
QEMU       = qemu-riscv64
QEMU_FLAGS = -cpu rv64,v=true,vlen=128

# ─── Source files ────────────────────────────────────────────────────────────
SRCS = src/main.cpp src/syscalls.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp src/nms_threshold.cpp

RV_TEST_SRCS   = $(filter-out src/main.cpp, $(SRCS))
HOST_TEST_SRCS = $(filter-out src/main.cpp src/syscalls.cpp, $(SRCS))

# Phase 6: RVV binary uses its own entry point + the two new RVV kernels.
# Does NOT include main.cpp (different entry point) or the scalar gaussian.cpp
# (the template is header-only via gaussian.ipp, included by main_rvv.cpp).
# syscalls.cpp is included because this also runs on QEMU (bare-metal newlib).
RVV_SRCS = src/main_rvv.cpp \
            src/syscalls.cpp \
            src/image_io.cpp \
            src/sobel.cpp \
            src/magnitude.cpp \
            src/direction.cpp \
            src/nms_threshold.cpp \
            src/rvv_gaussian.cpp \
            src/rvv_magnitude.cpp

# ─── Image / VLEN settings ───────────────────────────────────────────────────
IMG ?= test_input.raw
_SIZE := $(shell cat .img_size 2>/dev/null || echo "640 480")
W     ?= $(word 1,$(_SIZE))
H     ?= $(word 2,$(_SIZE))
VLEN ?= 128

# ─── .PHONY ──────────────────────────────────────────────────────────────────
.PHONY: all run sweep sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast \
        clean help test_all test_legacy test_gtest test_legacy_single \
        test_gtest_single convert verify \
        rvv vlen_128 vlen_256 vlen_512 vlen_sweep \
        view view_scalar view_rvv view_rvv_128 view_rvv_256 view_rvv_512 \
        view_O0 view_O2 view_O3 view_Os view_Ofast view_all view_input

# ─── Default ─────────────────────────────────────────────────────────────────
all: build_rv/canny_rv

# ─────────────────────────────────────────────────────────────────────────────
# HELP
# ─────────────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════╗"
	@echo "║              Canny Edge Detection - RISC-V                  ║"
	@echo "╠══════════════════════════════════════════════════════════════╣"
	@echo "║  Image Preparation                                          ║"
	@echo "║    make convert IMG=photo.jpg  -> JPG/PNG to test_input.raw ║"
	@echo "║    make view_input             -> view the input image      ║"
	@echo "╠══════════════════════════════════════════════════════════════╣"
	@echo "║  Scalar Pipeline                                            ║"
	@echo "║    make run          -> run scalar pipeline -> out.raw      ║"
	@echo "║    make view_scalar  -> view out.raw as out_scalar.png      ║"
	@echo "║    make sweep        -> -O0/-O2/-O3/-Os/-Ofast benchmark    ║"
	@echo "║    make view_O0      -> view out_O0.raw                     ║"
	@echo "║    make view_O2      -> view out_O2.raw                     ║"
	@echo "║    make view_O3      -> view out_O3.raw                     ║"
	@echo "║    make view_Os      -> view out_Os.raw                     ║"
	@echo "║    make view_Ofast   -> view out_Ofast.raw                  ║"
	@echo "╠══════════════════════════════════════════════════════════════╣"
	@echo "║  RVV Pipeline (Phase 6)                                     ║"
	@echo "║    make rvv          -> build RVV binary                    ║"
	@echo "║    make vlen_sweep   -> run at VLEN=128/256/512             ║"
	@echo "║    make vlen_128     -> run RVV at VLEN=128 -> out_rvv_128  ║"
	@echo "║    make vlen_256     -> run RVV at VLEN=256 -> out_rvv_256  ║"
	@echo "║    make vlen_512     -> run RVV at VLEN=512 -> out_rvv_512  ║"
	@echo "║    make view_rvv     -> view out_rvv_128.raw (default)      ║"
	@echo "║    make view_rvv_128 -> view VLEN=128 output                ║"
	@echo "║    make view_rvv_256 -> view VLEN=256 output                ║"
	@echo "║    make view_rvv_512 -> view VLEN=512 output                ║"
	@echo "╠══════════════════════════════════════════════════════════════╣"
	@echo "║  Utilities                                                  ║"
	@echo "║    make view_all     -> convert ALL .raw outputs to .png    ║"
	@echo "║    make verify       -> compare -O0 vs -Ofast output        ║"
	@echo "║    make test_all     -> run GoogleTest + QEMU assert tests  ║"
	@echo "║    make clean        -> remove all build and output files   ║"
	@echo "╚══════════════════════════════════════════════════════════════╝"

# ─────────────────────────────────────────────────────────────────────────────
# SCALAR BUILD / RUN / SWEEP
# ─────────────────────────────────────────────────────────────────────────────
build_rv/canny_rv: $(SRCS)
	@mkdir -p build_rv
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $@

run: build_rv/canny_rv
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_rv ./$(IMG) ./out.raw $(W) $(H)

sweep_O0:
	@echo "\n>>> [SWEEP] Running -O0 <<<"
	@mkdir -p build_rv
	$(RV_CXX) -std=c++17 -O0 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O0
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O0 ./test_input.raw ./out_O0.raw $(W) $(H) cycles_O0.txt

sweep_O2:
	@echo "\n>>> [SWEEP] Running -O2 <<<"
	@mkdir -p build_rv
	$(RV_CXX) -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O2
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O2 ./test_input.raw ./out_O2.raw $(W) $(H) cycles_O2.txt

sweep_O3:
	@echo "\n>>> [SWEEP] Running -O3 <<<"
	@mkdir -p build_rv
	$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O3
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O3 ./test_input.raw ./out_O3.raw $(W) $(H) cycles_O3.txt

sweep_Os:
	@echo "\n>>> [SWEEP] Running -Os <<<"
	@mkdir -p build_rv
	$(RV_CXX) -std=c++17 -Os -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_Os
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Os ./test_input.raw ./out_Os.raw $(W) $(H) cycles_Os.txt

sweep_Ofast:
	@echo "\n>>> [SWEEP] Running -Ofast <<<"
	@mkdir -p build_rv
	$(RV_CXX) -std=c++17 -Ofast -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_Ofast
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Ofast ./test_input.raw ./out_Ofast.raw $(W) $(H) cycles_Ofast.txt

build_host/summary: src/summary.cpp
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) src/summary.cpp -o build_host/summary

sweep: sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast build_host/summary
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt

# ─────────────────────────────────────────────────────────────────────────────
# RVV BUILD / VLEN SWEEP
# ─────────────────────────────────────────────────────────────────────────────
rvv: build_rv/canny_rvv

build_rv/canny_rvv: $(RVV_SRCS)
	@mkdir -p build_rv
	$(RV_CXX) $(RVV_FLAGS) $(RVV_SRCS) -o $@

vlen_128: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=128 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=128 ./build_rv/canny_rvv \
	    ./test_input.raw ./out_rvv_128.raw $(W) $(H)

vlen_256: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=256 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=256 ./build_rv/canny_rvv \
	    ./test_input.raw ./out_rvv_256.raw $(W) $(H)

vlen_512: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=512 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=512 ./build_rv/canny_rvv \
	    ./test_input.raw ./out_rvv_512.raw $(W) $(H)

vlen_sweep: vlen_128 vlen_256 vlen_512
	@echo "\n>>> [VLEN SWEEP] Comparing outputs across VLEN values <<<"
	@python3 -c "\
import numpy as np; \
a=np.fromfile('out_rvv_128.raw',dtype=np.uint8); \
b=np.fromfile('out_rvv_256.raw',dtype=np.uint8); \
c=np.fromfile('out_rvv_512.raw',dtype=np.uint8); \
ok_ab = np.array_equal(a,b); \
ok_bc = np.array_equal(b,c); \
print('VLEN=128 vs 256:', 'MATCH' if ok_ab else 'DIFFER'); \
print('VLEN=256 vs 512:', 'MATCH' if ok_bc else 'DIFFER'); \
print('VLA correctness:', 'PASS' if (ok_ab and ok_bc) else 'FAIL'); \
"

# ─────────────────────────────────────────────────────────────────────────────
# IMAGE CONVERSION
# ─────────────────────────────────────────────────────────────────────────────
convert:
	@python3 -c "\
from PIL import Image; import numpy as np; \
img = Image.open('$(IMG)').convert('L'); \
arr = np.array(img); \
arr.tofile('test_input.raw'); \
open('.img_size','w').write(str(arr.shape[1])+' '+str(arr.shape[0])); \
print('[ OK ] $(IMG) -> test_input.raw  (' + str(arr.shape[1]) + 'x' + str(arr.shape[0]) + ')'); \
"

# ─────────────────────────────────────────────────────────────────────────────
# VIEW TARGETS  (each .raw -> .png)
# ─────────────────────────────────────────────────────────────────────────────

# View the original input (grayscale, before any processing)
view_input:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('test_input.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('view_input.png'); \
print('[ OK ] Saved view_input.png  (original grayscale)'); \
"
	@xdg-open view_input.png 2>/dev/null || echo "      Open view_input.png manually"

# View scalar pipeline output (make run -> out.raw)
view view_scalar:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_scalar.png'); \
print('[ OK ] Saved out_scalar.png  (scalar pipeline output)'); \
"
	@xdg-open out_scalar.png 2>/dev/null || echo "      Open out_scalar.png manually"

# View sweep outputs
view_O0:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_O0.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_O0.png'); \
print('[ OK ] Saved out_O0.png'); \
"
	@xdg-open out_O0.png 2>/dev/null || echo "      Open out_O0.png manually"

view_O2:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_O2.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_O2.png'); \
print('[ OK ] Saved out_O2.png'); \
"
	@xdg-open out_O2.png 2>/dev/null || echo "      Open out_O2.png manually"

view_O3:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_O3.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_O3.png'); \
print('[ OK ] Saved out_O3.png'); \
"
	@xdg-open out_O3.png 2>/dev/null || echo "      Open out_O3.png manually"

view_Os:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_Os.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_Os.png'); \
print('[ OK ] Saved out_Os.png'); \
"
	@xdg-open out_Os.png 2>/dev/null || echo "      Open out_Os.png manually"

view_Ofast:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_Ofast.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_Ofast.png'); \
print('[ OK ] Saved out_Ofast.png'); \
"
	@xdg-open out_Ofast.png 2>/dev/null || echo "      Open out_Ofast.png manually"

# View RVV outputs
view_rvv view_rvv_128:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_rvv_128.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_rvv_128.png'); \
print('[ OK ] Saved out_rvv_128.png  (RVV VLEN=128 output)'); \
"
	@xdg-open out_rvv_128.png 2>/dev/null || echo "      Open out_rvv_128.png manually"

view_rvv_256:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_rvv_256.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_rvv_256.png'); \
print('[ OK ] Saved out_rvv_256.png  (RVV VLEN=256 output)'); \
"
	@xdg-open out_rvv_256.png 2>/dev/null || echo "      Open out_rvv_256.png manually"

view_rvv_512:
	@python3 -c "\
import numpy as np; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
arr = np.fromfile('out_rvv_512.raw', dtype=np.uint8).reshape(H,W); \
Image.fromarray(arr).save('out_rvv_512.png'); \
print('[ OK ] Saved out_rvv_512.png  (RVV VLEN=512 output)'); \
"
	@xdg-open out_rvv_512.png 2>/dev/null || echo "      Open out_rvv_512.png manually"

# Convert ALL existing .raw files to .png in one shot
view_all:
	@python3 -c "\
import numpy as np, os; from PIL import Image; \
W,H = map(int, open('.img_size').read().split()); \
raws = [f for f in os.listdir('.') if f.endswith('.raw') and f != 'test_input.raw']; \
[( \
    Image.fromarray(np.fromfile(r, dtype=np.uint8).reshape(H,W)).save(r.replace('.raw','.png')), \
    print('[ OK ] ' + r + ' -> ' + r.replace('.raw','.png')) \
) for r in sorted(raws)]; \
print('Done. ' + str(len(raws)) + ' file(s) converted.') if raws else print('No .raw output files found yet.'); \
"

# ─────────────────────────────────────────────────────────────────────────────
# UTILITIES
# ─────────────────────────────────────────────────────────────────────────────
verify:
	@python3 -c "\
import numpy as np; \
o0=np.fromfile('out_O0.raw',dtype=np.uint8); \
ofast=np.fromfile('out_Ofast.raw',dtype=np.uint8); \
diff=np.abs(o0.astype(int)-ofast.astype(int)); \
print('Max diff:', diff.max(), '| Pixels changed:', np.sum(diff>0), '/', len(o0)); \
print('Result:', 'MATCH' if np.array_equal(o0,ofast) else 'DIFFER (within-1 ok for rounding)'); \
"

# ─────────────────────────────────────────────────────────────────────────────
# TESTING
# ─────────────────────────────────────────────────────────────────────────────
GTEST_DIR   ?= $(HOME)/googletest-install
GTEST_LIBS   = -L$(GTEST_DIR)/lib -I$(GTEST_DIR)/include -lgtest -lgtest_main -lpthread

test_gtest:
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) $(HOST_TEST_SRCS) google_tests/*.cpp $(GTEST_LIBS) -o build_host/gtest_runner
	./build_host/gtest_runner

test_legacy:
	@mkdir -p build_rv
	@for test_file in $(wildcard tests/test_*.cpp); do \
		test_name=$$(basename $$test_file .cpp); \
		echo ">>> Running $$test_name <<<"; \
		$(RV_CXX) $(RV_FLAGS) $(RV_TEST_SRCS) $$test_file -o build_rv/$$test_name; \
		$(QEMU) $(QEMU_FLAGS) ./build_rv/$$test_name || exit 1; \
	done

test_legacy_single:
	@if [ -z "$(TEST)" ]; then echo "ERROR: Set TEST=name"; exit 1; fi
	@mkdir -p build_rv
	$(RV_CXX) $(RV_FLAGS) $(RV_TEST_SRCS) tests/$(TEST).cpp -o build_rv/$(TEST)
	$(QEMU) $(QEMU_FLAGS) ./build_rv/$(TEST)

test_gtest_single:
	@if [ -z "$(TEST)" ]; then echo "ERROR: Set TEST=name"; exit 1; fi
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) $(HOST_TEST_SRCS) google_tests/$(TEST).cpp $(GTEST_LIBS) -o build_host/$(TEST)
	./build_host/$(TEST)

test_all: test_gtest test_legacy

# ─────────────────────────────────────────────────────────────────────────────
# CLEAN
# ─────────────────────────────────────────────────────────────────────────────
clean:
	@rm -rf build_rv/* build_host/*
	@rm -f out*.raw out*.png view_*.png cycles_*.txt
	@echo "[ OK ] Cleaned all build and output files"
