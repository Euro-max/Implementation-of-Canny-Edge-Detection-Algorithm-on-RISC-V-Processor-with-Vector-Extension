# ─── Compilers ───────────────────────────────────────────────────
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++
QEMU      = qemu-riscv64
VLEN      ?= 128

# ─── Flags ───────────────────────────────────────────────────────
HOST_FLAGS = -std=c++17 -O2 -Wall -I src
RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src

# ─── GoogleTest ──────────────────────────────────────────────────
GTEST_DIR   = $(HOME)/googletest-install
GTEST_FLAGS = -I$(GTEST_DIR)/include -L$(GTEST_DIR)/lib \
              -lgtest -lgtest_main -lpthread

# ─── Targets ─────────────────────────────────────────────────────
.PHONY: all test_gaussian test_magnitude test_sobel test_direction \
        test_image_io test_all \
        gtest_gaussian gtest_magnitude gtest_sobel gtest_direction \
        gtest_image_io gtest_all gtest \
        canny_rv run clean

all: canny_rv

# ─── Original host unit tests (tests/ folder) ────────────────────

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

# ─── Run all original tests ──────────────────────────────────────
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

# ─── Cross-compile for RISC-V ────────────────────────────────────
canny_rv:
	$(RV_CXX) $(RV_FLAGS) \
	  src/main.cpp src/image_io.cpp \
	  src/sobel.cpp src/magnitude.cpp \
	  src/direction.cpp \
	  -o build_rv/canny_rv

# ─── Run on QEMU ─────────────────────────────────────────────────
run: canny_rv
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) \
	  ./build_rv/canny_rv test_input.raw out.raw 64 64

clean:
	rm -rf build_host/* build_rv/*