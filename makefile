# ─── Compilers ───────────────────────────────────────────────────
HOST_CXX  = g++
#RV_CXX    = riscv64-unknown-elf-g++
#QEMU      = qemu-riscv64
VLEN      ?= 128
RV_CXX  = riscv64-linux-gnu-g++
RV_FLAGS = -std=c++17 -O2 -march=rv64gc -mabi=lp64d -I src -I include
QEMU     = qemu-riscv64
QEMU_FLAGS = -L /usr/riscv64-linux-gnu
# ─── Flags ───────────────────────────────────────────────────────
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
#RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src -I include

# ─── GoogleTest ──────────────────────────────────────────────────
GTEST_DIR   = $(HOME)/googletest-install
GTEST_FLAGS = -I$(GTEST_DIR)/include -L$(GTEST_DIR)/lib \
              -lgtest -lgtest_main -lpthread

# ─── Targets ─────────────────────────────────────────────────────
.PHONY: all test_gaussian canny_rv run clean
help:
	@echo "Available commands:"
	@echo "  make canny_rv  → cross-compile for RISC-V"
	@echo "  make run       → run on QEMU (default VLEN=128)"
	@echo "  make run VLEN=256 → run with VLEN=256"
	@echo "  make run VLEN=512 → run with VLEN=512"
	@echo "  make test_{gaussian,sobel,magnitude,direction,image_io,all}      → build and run host-side tests"
	@echo "  make clean     → remove all build files"
all: canny_rv

# Test Gaussian blur on host
test_gaussian:
	$(HOST_CXX) $(HOST_FLAGS) \
	  tests/test_gaussian.cpp \
	  -o build_host/test_gaussian
	./build_host/test_gaussian
	
	
test_magnitude:
	$(HOST_CXX) $(HOST_FLAGS) \
	  tests/test_magnitude.cpp src/magnitude.cpp \
	  -o build_host/test_magnitude
	./build_host/test_magnitude

test_sobel:
	$(HOST_CXX) $(HOST_FLAGS) \
	  tests/test_sobel.cpp src/sobel.cpp \
	  -o build_host/test_sobel
	./build_host/test_sobel


test_direction:
	$(HOST_CXX) $(HOST_FLAGS) \
	  tests/test_direction.cpp src/direction.cpp \
	  -o build_host/test_direction
	./build_host/test_direction


test_image_io:
	$(HOST_CXX) $(HOST_FLAGS) \
	  tests/test_image_io.cpp src/image_io.cpp \
	  -o build_host/test_image_io
	./build_host/test_image_io


test_all: test_gaussian test_magnitude test_sobel test_direction test_image_io

# ─── GoogleTest suites (google_tests/ folder) ────────────────────

gtest_gaussian:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_gaussian.cpp \
	  -o build_host/gtest_gaussian \
	  $(GTEST_FLAGS)
	./build_host/gtest_gaussian

gtest_magnitude:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_magnitude.cpp src/magnitude.cpp \
	  -o build_host/gtest_magnitude \
	  $(GTEST_FLAGS)
	./build_host/gtest_magnitude

gtest_sobel:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_sobel.cpp src/sobel.cpp \
	  -o build_host/gtest_sobel \
	  $(GTEST_FLAGS)
	./build_host/gtest_sobel

gtest_direction:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_direction.cpp src/direction.cpp \
	  -o build_host/gtest_direction \
	  $(GTEST_FLAGS)
	./build_host/gtest_direction

gtest_image_io:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_image_io.cpp src/image_io.cpp \
	  -o build_host/gtest_image_io \
	  $(GTEST_FLAGS)
	./build_host/gtest_image_io

# ─── Run all GoogleTest suites ───────────────────────────────────
gtest_all: gtest_gaussian gtest_magnitude gtest_sobel gtest_direction gtest_image_io

# ─── Build all GoogleTest binaries without running ───────────────
gtest:
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_gaussian.cpp \
	  -o build_host/gtest_gaussian \
	  $(GTEST_FLAGS)
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_magnitude.cpp src/magnitude.cpp \
	  -o build_host/gtest_magnitude \
	  $(GTEST_FLAGS)
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_sobel.cpp src/sobel.cpp \
	  -o build_host/gtest_sobel \
	  $(GTEST_FLAGS)
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_direction.cpp src/direction.cpp \
	  -o build_host/gtest_direction \
	  $(GTEST_FLAGS)
	$(HOST_CXX) $(HOST_FLAGS) \
	  google_tests/gtest_image_io.cpp src/image_io.cpp \
	  -o build_host/gtest_image_io \
	  $(GTEST_FLAGS)
# Host-side unit tests using GoogleTest
test:
	$(HOST_CXX) $(HOST_FLAGS) \
	    -o build/host/canny_test \
	    tests/test_pipeline.cpp \
	    src/sobel.cpp \
	    src/magnitude.cpp \
	    src/direction.cpp \
	    src/image_io.cpp \
	    -lgtest -lgtest_main -lpthread
	./build/host/canny_test

# Cross-compile for RISC-V
canny_rv: 
	$(RV_CXX) $(RV_FLAGS) \
	  src/main.cpp src/image_io.cpp \
	  src/sobel.cpp src/magnitude.cpp \
	  src/direction.cpp \
	  -o build_rv/canny_rv

# Run on QEMU
run: canny_rv
	$(QEMU) $(QEMU_FLAGS) \
	  ./build_rv/canny_rv ./test_input.raw out.raw 640 480
# ─── Optimization Sweep ──────────────────────────────────────────
sweep: sweep_O0 sweep_O2 sweep_O3

sweep_O0:
	$(RV_CXX) -std=c++17 -O0 -march=rv64gcv -mabi=lp64d -I src -I include \
	  src/main.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp \
	  -o build_rv/canny_O0
	@echo "\n--- O0 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 ./build_rv/canny_O0 test_input.raw out_O0.raw 640 480

sweep_O2:
	$(RV_CXX) -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src -I include \
	  src/main.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp \
	  -o build_rv/canny_O2
	@echo "\n--- O2 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 ./build_rv/canny_O2 test_input.raw out_O2.raw 640 480

sweep_O3:
	$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -I src -I include \
	  src/main.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp \
	  -o build_rv/canny_O3
	@echo "\n--- O3 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 ./build_rv/canny_O3 test_input.raw out_O3.raw 640 480

# Binary sizes
sizes:
	@echo "\n=== Binary Sizes ==="
	@ls -lh build_rv/canny_O0 build_rv/canny_O2 build_rv/canny_O3 2>/dev/null | awk '{print $$5, $$9}'

clean:
	rm -rf build_host/* build_rv/*
	@echo "Cleaned build directories"
	
	
# ─── Image Processing ────────────────────────────────────────────
IMG     ?= image.jpg
WIDTH   ?= 640
HEIGHT  ?= 480

convert:
	python3 -c "\
	from PIL import Image; import numpy as np; \
	img = Image.open('$(IMG)').convert('L'); \
	arr = np.array(img); \
	arr.tofile('test_input.raw'); \
	print(f'Size: {arr.shape[1]}x{arr.shape[0]}')"

view:
	python3 -c "\
	import numpy as np; from PIL import Image; \
	arr = np.fromfile('out.raw', dtype=np.uint8).reshape($(HEIGHT), $(WIDTH)); \
	img = Image.fromarray(arr); \
	img.save('out.png')"
	eog out.png

process:
	./build_host/canny_host \
	./test_input.raw \
	./out.raw \
	$(WIDTH) $(HEIGHT)
	
# ── FULL PIPELINE: convert photo → run → view ──
photo: canny_rv
	@echo ""
	@echo "Step 1: Converting $(IMG) to RAW ($(WIDTH)x$(HEIGHT))..."
	python3 -c "\
from PIL import Image; import numpy as np; \
img = Image.open('$(IMG)').convert('L').resize(($(WIDTH), $(HEIGHT))); \
arr = np.array(img); \
arr.tofile('test_input.raw'); \
print(f'Saved test_input.raw: {arr.shape[1]}x{arr.shape[0]} pixels')"
	@echo ""
	@echo "Step 2: Running Canny pipeline on QEMU (VLEN=$(VLEN))..."
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) \
	    ./build_rv/canny_rv \
	    test_input.raw out.raw $(WIDTH) $(HEIGHT)
	@echo ""
	@echo "Step 3: Saving comparison PNG..."
	python3 -c "\
import numpy as np; from PIL import Image; \
inp = np.fromfile('test_input.raw', dtype=np.uint8).reshape($(HEIGHT), $(WIDTH)); \
out = np.fromfile('out.raw',        dtype=np.uint8).reshape($(HEIGHT), $(WIDTH)); \
side = Image.new('L', ($(WIDTH)*2+10, $(HEIGHT)), 128); \
side.paste(Image.fromarray(inp), (0, 0)); \
side.paste(Image.fromarray(out), ($(WIDTH)+10, 0)); \
side.save('comparison.png'); \
print(f'Edge pixels: {(out>0).sum()} / {$(WIDTH)*$(HEIGHT)} ({100*(out>0).mean():.1f}%)'); \
print('Saved: comparison.png')"
	@echo ""
	@echo "============================================"
	@echo " Done! View result in Windows Explorer:"
	@echo " \\\\wsl.localhost\\Ubuntu\\home\\$$USER\\canny-edge\\comparison.png"
	@echo "============================================"
	

