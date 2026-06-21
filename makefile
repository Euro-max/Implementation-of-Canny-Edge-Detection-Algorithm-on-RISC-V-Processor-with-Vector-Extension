# ─── Compilers ───────────────────────────────────────────────────────────────
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++

# ─── Flags ───────────────────────────────────────────────────────────────────
HOST_FLAGS = -std=c++17 -O2 -Wall -I src -I include
RV_FLAGS   = -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include

# Special flags for the RVV Phase 6 build (-Ofast for max scalar baseline performance)
RVV_FLAGS  = -std=c++17 -Ofast -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include

# ─── QEMU ────────────────────────────────────────────────────────────────────
QEMU       = qemu-riscv64
QEMU_FLAGS = -cpu rv64,v=true,vlen=128

# ─── Source files ────────────────────────────────────────────────────────────
SRCS = src/main.cpp src/syscalls.cpp src/image_io.cpp src/sobel.cpp src/magnitude.cpp src/direction.cpp src/nms_threshold.cpp

RV_TEST_SRCS   = $(filter-out src/main.cpp, $(SRCS)) src/rvv_gaussian.cpp src/rvv_magnitude.cpp
HOST_TEST_SRCS = $(filter-out src/main.cpp src/syscalls.cpp, $(SRCS))

# Phase 6 RVV Sources (Removes main.cpp, adds main_rvv.cpp + RVV implementations)
RVV_SRCS = $(filter-out src/main.cpp, $(SRCS)) src/main_rvv.cpp src/rvv_gaussian.cpp src/rvv_magnitude.cpp

# ─── Image/VLEN settings ─────────────────────────────────────────────────────
IMG ?= test_input.raw
_SIZE := $(shell cat .img_size 2>/dev/null || echo "640 480")
W     ?= $(word 1,$(_SIZE))
H     ?= $(word 2,$(_SIZE))
VLEN ?= 128

# ─── Default target ──────────────────────────────────────────────────────────
.PHONY: all run sweep sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast vect vect-asm clean help test_all test_legacy test_gtest test_legacy_single test_gtest_single convert view sizes verify table rvv vlen_128 vlen_256 vlen_512 vlen_sweep view_rvv view_rvv_128 view_rvv_256 view_rvv_512 view_all stages view_stages

all: build_rv/canny_rv

help:
	@echo ""
	@echo "╔════════════════════════════════════════════════════════════════════╗"
	@echo "║                Canny Edge Detection - RISC-V                       ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  Image Preparation & Viewing                                       ║"
	@echo "║    make convert IMG=x.jpg -> Convert photo to .raw                 ║"
	@echo "║    make view              -> View scalar output as .png            ║"
	@echo "║    make view_all          -> Convert ALL output .raw to .png       ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  Scalar Pipeline (Phase 4/5)                                       ║"
	@echo "║    make run               -> Run optimized on QEMU                 ║"
	@echo "║    make sweep             -> Perform full optimization sweep       ║"
	@echo "║    make table             -> Show runtime/size table               ║"
	@echo "║    make sizes             -> Show binary sizes for all opt flags   ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  RVV Pipeline (Phase 6)                                            ║"
	@echo "║    make rvv               -> Build RVV binary                      ║"
	@echo "║    make vlen_sweep        -> Run RVV at VLEN=128/256/512           ║"
	@echo "║    make vlen_128          -> Run RVV at VLEN=128                   ║"
	@echo "║    make vlen_256          -> Run RVV at VLEN=256                   ║"
	@echo "║    make vlen_512          -> Run RVV at VLEN=512                   ║"
	@echo "║    make view_rvv          -> View default VLEN output              ║"
	@echo "║    make view_rvv_128      -> View VLEN=128 output                  ║"
	@echo "║    make view_rvv_256      -> View VLEN=256 output                  ║"
	@echo "║    make view_rvv_512      -> View VLEN=512 output                  ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  Pipeline Debugging & Stages                                       ║"
	@echo "║    make stages            -> Dump intermediate stage .raw files    ║"
	@echo "║    make view_stages       -> Convert intermediate .raw to .png     ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  Vectorization Analysis                                            ║"
	@echo "║    make vect              -> Generate vectorization report         ║"
	@echo "║    make vect-asm FUNC=x   -> View assembly for specific function   ║"
	@echo "╠════════════════════════════════════════════════════════════════════╣"
	@echo "║  Testing & Utilities                                               ║"
	@echo "║    make test_all          -> Run ALL tests (Host+RV)               ║"
	@echo "║    make test_gtest_single TEST=x -> Run a single host test         ║"
	@echo "║    make test_legacy_single TEST=x -> Run single legacy RV test     ║"
	@echo "║    make verify            -> Compare O0 vs Ofast outputs           ║"
	@echo "║    make clean             -> Remove all build & output files       ║"
	@echo "╚════════════════════════════════════════════════════════════════════╝"

# ─── Phase 4/5: Build / Run / Sweep ──────────────────────────────────────────
build_rv/canny_rv: $(SRCS)
	@mkdir -p build_rv
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $@

run: build_rv/canny_rv
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_rv ./$(IMG) ./out.raw $(W) $(H)

sweep_O0: ; @echo "\n>>> [SWEEP] Running -O0 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O0 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include $(SRCS) -o build_rv/canny_O0; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O0 ./test_input.raw ./out_O0.raw $(W) $(H) cycles_O0.txt
sweep_O2: ; @echo "\n>>> [SWEEP] Running -O2 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O2 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include $(SRCS) -o build_rv/canny_O2; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O2 ./test_input.raw ./out_O2.raw $(W) $(H) cycles_O2.txt
sweep_O3: ; @echo "\n>>> [SWEEP] Running -O3 <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include $(SRCS) -o build_rv/canny_O3; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_O3 ./test_input.raw ./out_O3.raw $(W) $(H) cycles_O3.txt
sweep_Os: ; @echo "\n>>> [SWEEP] Running -Os <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -Os -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include $(SRCS) -o build_rv/canny_Os; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Os ./test_input.raw ./out_Os.raw $(W) $(H) cycles_Os.txt
sweep_Ofast: ; @echo "\n>>> [SWEEP] Running -Ofast <<<"; mkdir -p build_rv; $(RV_CXX) -std=c++17 -Ofast -march=rv64gcv -mabi=lp64d -fno-tree-vectorize -fno-tree-slp-vectorize -I src -I include $(SRCS) -o build_rv/canny_Ofast; $(QEMU) $(QEMU_FLAGS) ./build_rv/canny_Ofast ./test_input.raw ./out_Ofast.raw $(W) $(H) cycles_Ofast.txt

build_host/summary: src/summary.cpp
	@mkdir -p build_host
	$(HOST_CXX) $(HOST_FLAGS) src/summary.cpp -o build_host/summary

sweep: sweep_O0 sweep_O2 sweep_O3 sweep_Os sweep_Ofast build_host/summary table sizes
	@./build_host/summary cycles_O0.txt cycles_O2.txt cycles_O3.txt cycles_Os.txt cycles_Ofast.txt

# ─── Phase 6: RVV Pipeline ───────────────────────────────────────────────────
build_rv/canny_rvv: $(RVV_SRCS)
	@mkdir -p build_rv
	@echo "\n[ BUILD ] Compiling RVV binary (with -Ofast)..."
	$(RV_CXX) $(RVV_FLAGS) $(RVV_SRCS) -o $@

rvv: build_rv/canny_rvv

vlen_128: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=128 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=128 ./build_rv/canny_rvv ./$(IMG) ./out_rvv_128.raw $(W) $(H)

vlen_256: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=256 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=256 ./build_rv/canny_rvv ./$(IMG) ./out_rvv_256.raw $(W) $(H)

vlen_512: build_rv/canny_rvv
	@echo "\n>>> [VLEN SWEEP] VLEN=512 <<<"
	$(QEMU) -cpu rv64,v=true,vlen=512 ./build_rv/canny_rvv ./$(IMG) ./out_rvv_512.raw $(W) $(H)

vlen_sweep: vlen_128 vlen_256 vlen_512

# ─── Summary Table ────────────────────────────────────────────────────────────
table:
	@echo ""
	@echo "Phase 4: Optimization Sweep Results"
	@echo "==================================="
	@echo "The following table summarizes the impact of compiler optimization flags on binary size, runtime, and vectorization behavior for the Canny Edge pipeline:"
	@echo ""
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@echo "| Optimization | Binary Size | Cycles       | Time (ms) | Vectorization Notes |"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@c=$$(awk 'NR<=7 {c+=$$1} END {print c+0}' cycles_O0.txt 2>/dev/null); t=$$(awk 'NR>=8 && NR<=14 {t+=$$1} END {printf "%.3f", t/1000000}' cycles_O0.txt 2>/dev/null); s=$$(ls -lh build_rv/canny_O0 2>/dev/null | awk '{print $$5}'); s=$${s:-"N/A"}; printf "| %-12s | %-11s | %-12s | %-9s | %-19s |\n" "-O0" "$$s" "$$c" "$$t" "None"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@c=$$(awk 'NR<=7 {c+=$$1} END {print c+0}' cycles_O2.txt 2>/dev/null); t=$$(awk 'NR>=8 && NR<=14 {t+=$$1} END {printf "%.3f", t/1000000}' cycles_O2.txt 2>/dev/null); s=$$(ls -lh build_rv/canny_O2 2>/dev/null | awk '{print $$5}'); s=$${s:-"N/A"}; printf "| %-12s | %-11s | %-12s | %-9s | %-19s |\n" "-O2" "$$s" "$$c" "$$t" "Some loops"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@c=$$(awk 'NR<=7 {c+=$$1} END {print c+0}' cycles_O3.txt 2>/dev/null); t=$$(awk 'NR>=8 && NR<=14 {t+=$$1} END {printf "%.3f", t/1000000}' cycles_O3.txt 2>/dev/null); s=$$(ls -lh build_rv/canny_O3 2>/dev/null | awk '{print $$5}'); s=$${s:-"N/A"}; printf "| %-12s | %-11s | %-12s | %-9s | %-19s |\n" "-O3" "$$s" "$$c" "$$t" "More aggressive"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@c=$$(awk 'NR<=7 {c+=$$1} END {print c+0}' cycles_Os.txt 2>/dev/null); t=$$(awk 'NR>=8 && NR<=14 {t+=$$1} END {printf "%.3f", t/1000000}' cycles_Os.txt 2>/dev/null); s=$$(ls -lh build_rv/canny_Os 2>/dev/null | awk '{print $$5}'); s=$${s:-"N/A"}; printf "| %-12s | %-11s | %-12s | %-9s | %-19s |\n" "-Os" "$$s" "$$c" "$$t" "Size-focused"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@c=$$(awk 'NR<=7 {c+=$$1} END {print c+0}' cycles_Ofast.txt 2>/dev/null); t=$$(awk 'NR>=8 && NR<=14 {t+=$$1} END {printf "%.3f", t/1000000}' cycles_Ofast.txt 2>/dev/null); s=$$(ls -lh build_rv/canny_Ofast 2>/dev/null | awk '{print $$5}'); s=$${s:-"N/A"}; printf "| %-12s | %-11s | %-12s | %-9s | %-19s |\n" "-Ofast" "$$s" "$$c" "$$t" "Max speed"
	@echo "+--------------+-------------+--------------+-----------+---------------------+"
	@echo ""
sizes:
	@echo ""
	@echo "+-----------------------+"
	@echo "|  Binary Size Summary  |"
	@echo "+----------+------------+"
	@echo "| Flag     | Size       |"
	@echo "+----------+------------+"
	@printf "| -O0      | %-10s |\n" $$(ls -lh build_rv/canny_O0 2>/dev/null | awk '{print $$5}')
	@printf "| -O2      | %-10s |\n" $$(ls -lh build_rv/canny_O2 2>/dev/null | awk '{print $$5}')
	@printf "| -O3      | %-10s |\n" $$(ls -lh build_rv/canny_O3 2>/dev/null | awk '{print $$5}')
	@printf "| -Os      | %-10s |\n" $$(ls -lh build_rv/canny_Os 2>/dev/null | awk '{print $$5}')
	@printf "| -Ofast   | %-10s |\n" $$(ls -lh build_rv/canny_Ofast 2>/dev/null | awk '{print $$5}')
	@echo "+----------+------------+"

# ─── Phase 4: Auto-Vectorization Report ──────────────────────────────────────
VECT_SRCS = $(filter-out src/main.cpp, $(SRCS))

vect:
	@mkdir -p build_rv
	@echo "\n[ BUILD ] Compiling with -fopt-info-vec-all (vectorization report)..."
	@$(RV_CXX) -std=c++17 -O3 -march=rv64gcv -mabi=lp64d \
	  -ftree-vectorize -fopt-info-vec-all \
	  -I src -I include \
	  $(VECT_SRCS) -o build_rv/canny_vect 2> vec_report.txt; \
	  true
	@echo "[ OK    ] Report saved to vec_report.txt ($$(wc -l < vec_report.txt) lines)"
	@echo ""
	@echo "+----------------------------------------------------------------+"
	@echo "|              Auto-Vectorization Schedule (per function)        |"
	@echo "+----------------------------------------------------------------+"
	@for f in gaussian sobel magnitude direction nms_threshold; do \
		count=$$(grep -c "note: vectorized" vec_report.txt 2>/dev/null); \
		line=$$(grep "$$f.*note: vectorized" vec_report.txt | head -1); \
		if [ -n "$$line" ]; then \
			n=$$(echo "$$line" | grep -oE "vectorized [0-9]+ loops"); \
			printf "| %-14s | %-46s |\n" "$$f" "$$n in function"; \
		else \
			printf "| %-14s | %-46s |\n" "$$f" "(no report line found)"; \
		fi; \
	done
	@echo "+----------------------------------------------------------------+"
	@echo ""
	@echo "[ INFO  ] Full diagnostic detail: less vec_report.txt"
	@echo "[ INFO  ] Filter one stage:       grep gaussian.ipp vec_report.txt"
	@echo "[ INFO  ] Confirm in disassembly: make vect-asm FUNC=<mangled_name>"

# ─── Confirm vectorization in actual disassembly for one function ───────────
vect-asm: build_rv/canny_vect
	@if [ -z "$(FUNC)" ]; then \
		echo "ERROR: Set FUNC=<mangled_name>. Find names with:"; \
		echo "  riscv64-unknown-elf-nm build_rv/canny_vect | grep -iE 'gaussian|sobel|magnitude|direction'"; \
		exit 1; \
	fi
	@echo "\n[ ASM   ] RVV instructions found in $(FUNC):"
	@riscv64-unknown-elf-objdump -d --disassemble=$(FUNC) build_rv/canny_vect | grep -P '\tv[a-z]' || echo "  (none found — fully scalar)"

# ─── Image Utils ─────────────────────────────────────────────────────────────
convert:
	@python3 -c "from PIL import Image; import numpy as np; img = Image.open('$(IMG)').convert('L'); arr = np.array(img); arr.tofile('test_input.raw'); open('.img_size', 'w').write(str(arr.shape[1])+' '+str(arr.shape[0]))"
	@echo "[ OK ] Converted $(IMG) to test_input.raw"

view:
	@python3 -c "import numpy as np; from PIL import Image; arr = np.fromfile('out.raw', dtype=np.uint8).reshape($(H), $(W)); Image.fromarray(arr).save('out.png')"
	@echo "[ OK ] Saved out.png"

stages: build_rv/canny_rv
	@w=$$(cat .img_size 2>/dev/null | awk '{print $$1}' || echo "640"); \
	h=$$(cat .img_size 2>/dev/null | awk '{print $$2}' || echo "480"); \
	$(QEMU) $(QEMU_FLAGS) ./build_rv/canny_rv ./$(IMG) ./out.raw $$w $$h
	@echo "[ OK ] Stage .raw files written (Detected $$w x $$h): stage1_grayscale.raw ... stage8_hysteresis.raw"

view_stages:
	@python3 -c "import os, numpy as np; from PIL import Image; \
	s = open('.img_size').read().split() if os.path.exists('.img_size') else [640, 480]; \
	W, H = int(s[0]), int(s[1]); \
	names=['stage1_grayscale','stage2_gaussian','stage3_sobel_gx','stage3_sobel_gy','stage4_magnitude','stage5_direction','stage6_nms','stage7_threshold','stage8_hysteresis']; \
	saved = []; \
	[ (Image.fromarray(np.fromfile(n+'.raw',dtype=np.uint8).reshape(H,W)).save(n+'.png'), saved.append(n+'.png')) for n in names if os.path.exists(n+'.raw') ]; \
	print('[ OK ] Saved ' + str(len(saved)) + ' PNGs (Size: ' + str(W) + 'x' + str(H) + ')')"
view_rvv_128:
	@python3 -c "import numpy as np; from PIL import Image; arr = np.fromfile('out_rvv_128.raw', dtype=np.uint8).reshape($(H), $(W)); Image.fromarray(arr).save('out_rvv_128.png')"
	@echo "[ OK ] Saved out_rvv_128.png"

view_rvv_256:
	@python3 -c "import numpy as np; from PIL import Image; arr = np.fromfile('out_rvv_256.raw', dtype=np.uint8).reshape($(H), $(W)); Image.fromarray(arr).save('out_rvv_256.png')"
	@echo "[ OK ] Saved out_rvv_256.png"

view_rvv_512:
	@python3 -c "import numpy as np; from PIL import Image; arr = np.fromfile('out_rvv_512.raw', dtype=np.uint8).reshape($(H), $(W)); Image.fromarray(arr).save('out_rvv_512.png')"
	@echo "[ OK ] Saved out_rvv_512.png"

view_rvv: view_rvv_128

view_all: view view_rvv_128 view_rvv_256 view_rvv_512

verify:
	@python3 -c "import numpy as np; o0=np.fromfile('out_O0.raw',dtype=np.uint8); ofast=np.fromfile('out_Ofast.raw',dtype=np.uint8); print('Diff:', 'OK' if np.array_equal(o0,ofast) else 'FAIL')"

# ─── Testing Targets ─────────────────────────────────────────────────────────
GTEST_DIR   ?= $(HOME)/googletest-install
GTEST_LIBS   = -L$(GTEST_DIR)/lib -I$(GTEST_DIR)/include -lgtest -lgtest_main -lpthread

test_all: test_gtest test_legacy
	@echo "\n[ OK ] All tests (GoogleTest & Legacy) completed!"

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
	@rm -f out*.raw out*.png cycles_*.txt
