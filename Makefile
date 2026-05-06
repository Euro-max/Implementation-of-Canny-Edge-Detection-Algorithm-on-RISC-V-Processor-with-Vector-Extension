# --- Compilers ---
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++
QEMU      = qemu-riscv64
VLEN      ?= 128   # Use: make run VLEN=256

# --- Flags ---
# Use -O3 for Phase 4 optimization requirements later [cite: 61]
HOST_FLAGS  = -std=c++17 -O3 -Wall
RV_FLAGS    = -std=c++17 -O3 -march=rv64gcv -mabi=lp64d -static

# --- GoogleTest ---
# Adjust GTEST_DIR if yours is different (e.g., /usr/local)
GTEST_FLAGS = -lgtest -lgtest_main -lpthread

# --- Directories ---
BUILD_HOST = build_host
BUILD_RV   = build_rv

# --- Targets ---
.PHONY: all test canny_rv run clean directories

all: directories canny_rv test

# Ensure build directories exist 
directories:
	@mkdir -p $(BUILD_HOST) $(BUILD_RV)

# Host-side GoogleTest suite [cite: 39, 53]
test: directories
	$(HOST_CXX) $(HOST_FLAGS) tests/test_env.cpp -o $(BUILD_HOST)/test_runner $(GTEST_FLAGS)
	@echo "--- Running Host Tests ---"
	./$(BUILD_HOST)/test_runner

# Cross-compile for RISC-V [cite: 29, 30]
canny_rv: directories
	$(RV_CXX) $(RV_FLAGS) runner.cpp -o $(BUILD_RV)/canny_rv

# Run on QEMU [cite: 35, 69]
run: canny_rv
	@echo "--- Running on QEMU (VLEN=$(VLEN)) ---"
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./$(BUILD_RV)/canny_rv

clean:
	rm -rf $(BUILD_HOST) $(BUILD_RV)
