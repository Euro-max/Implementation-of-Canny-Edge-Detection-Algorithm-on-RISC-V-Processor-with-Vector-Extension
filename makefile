# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include

# ─── QEMU ────────────────────────────────────────────────────────────────────
QEMU       = qemu-riscv64
QEMU_FLAGS = -cpu rv64,v=true,vlen=128

# ─── Source files ────────────────────────────────────────────────────────────
SRCS = src/main.cpp src/syscalls.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp src/nms_threshold.cpp

RV_TEST_SRCS   = $(filter-out src/main.cpp, $(SRCS))
HOST_TEST_SRCS = $(filter-out src/main.cpp src/syscalls.cpp, $(SRCS))

# ─── Image/VLEN settings ─────────────────────────────────────────────────────
IMG ?= test_input.raw
_SIZE := $(shell cat .img_size 2>/dev/null || echo "640 480")
W     ?= $(word 1,$(_SIZE))
H     ?= $(word 2,$(_SIZE))
VLEN ?= 128

# ─── Default target ──────────────────────────────────────────────────────────
.PHONY: all run sweep sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast clean help test_all test_legacy test_gtest test_legacy_single test_gtest_single convert view sizes verify

all: build_rv/canny_rv

help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════╗"
	@echo "║            Canny Edge Detection - RISC-V             ║"
	@echo "╠══════════════════════════════════════════════════════╣"
	@echo "║  make run               -> Run optimized on QEMU     ║"
	@echo "║  make test_all          -> Run ALL tests (Host+RV)   ║"
	@echo "║  make test_legacy_single TEST=test_name              ║"
	@echo "║  make test_gtest_single TEST=gtest_name              ║"
	@echo "║  make sweep             -> Perform full opt. sweep   ║"
	@echo "║  make convert IMG=x.jpg -> Convert photo to .raw     ║"
	@echo "║  make verify            -> Compare O0 vs Ofast       ║"
	@echo "╚══════════════════════════════════════════════════════╝"

# ─── Build / Run / Sweep ─────────────────────────────────────────────────────
build_rv/canny_rv: $(SRCS)
	@mkdir -p build_rv
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $@

run: build_rv/canny_rv
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_rv ./$(IMG) ./out.raw $(W) $(H)

# Optimization Sweep Targets (with verbose logging)
sweep_O0: ; @echo "\n>>> [SWEEP] Running -O0 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O0 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O0; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O0 ./test_input.raw ./out_O0.raw $(W) $(H) cycles_O0.txt
sweep_O2: ; @echo "\n>>> [SWEEP] Running -O2 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O2; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O2 ./test_input.raw ./out_O2.raw $(W) $(H) cycles_O2.txt
sweep_O3: ; @echo "\n>>> [SWEEP] Running -O3 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_O3; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O3 ./test_input.raw ./out_O3.raw $(W) $(H) cycles_O3.txt
sweep_Os: ; @echo "\n>>> [SWEEP] Running -Os <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -Os -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_Os; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Os ./test_input.raw ./out_Os.raw $(W) $(H) cycles_Os.txt
sweep_Ofast: ; @echo "\n>>> [SWEEP] Running -Ofast <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -Ofast -march=rv64gcv -mabi=lp64d -I src -I include $(SRCS) -o build_rv/canny_Ofast; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Ofast ./test_input.raw ./out_Ofast.raw $(W) $(H) cycles_Ofast.txt

build_host/summary: src/summary.cpp
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) src/summary.cpp -o build_host/summary

sweep: sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast build_host/summary
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt

# ─── Image Utils ─────────────────────────────────────────────────────────────
convert:
	@python3 -c "from PIL import Image; import numpy as np; img = Image.open('$(IMG)').convert('L'); arr = np.array(img); arr.tofile('test_input.raw'); open('.img_size', 'w').write(str(arr.shape[1])+' '+str(arr.shape[0]))"
	@echo "[ OK ] Converted $(IMG) to test_input.raw"

view:
	@python3 -c "import numpy as np; from PIL import Image; arr = np.fromfile('out.raw', dtype=np.uint8).reshape($(H), $(W)); Image.fromarray(arr).save('out.png')"
	@echo "[ OK ] Saved out.png"

verify:
	@python3 -c "import numpy as np; o0=np.fromfile('out_O0.raw',dtype=np.uint8); ofast=np.fromfile('out_Ofast.raw',dtype=np.uint8); print('Diff:', 'OK' if np.array_equal(o0,ofast) else 'FAIL')"

# ─── Testing Targets ─────────────────────────────────────────────────────────
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

clean:
	@rm -rf build_rv/* build_host/*
	@rm -f out*.raw out.png cycles_*.txt
