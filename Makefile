# --- Compilers ---
HOST_CXX  = g++
RV_CXX    = riscv64-unknown-elf-g++
QEMU      = qemu-riscv64
VLEN      ?= 128

# --- Directories ---
SRC_DIR    = src
INC_DIR    = include
TEST_DIR   = tests
BUILD_HOST = build_host
BUILD_RV   = build_rv

# --- Flags ---
# -I$(INC_DIR) tells the compiler to look in the include/ folder for headers
COMMON_FLAGS = -std=c++17 -O3 -Wall -I$(INC_DIR)
RV_FLAGS     = $(COMMON_FLAGS) -march=rv64gcv -mabi=lp64d -static
GTEST_FLAGS  = -lgtest -lgtest_main -lpthread

# --- Source Discovery ---
# Finds all .cpp files in src/
SRCS        = $(wildcard $(SRC_DIR)/*.cpp)
# Finds all .cpp files in tests/
TEST_SRCS   = $(wildcard $(TEST_DIR)/*.cpp)

# Filter out main.cpp when building tests to avoid "multiple definition of main"
SRCS_NO_MAIN = $(filter-out $(SRC_DIR)/main.cpp, $(SRCS))

# --- Targets ---
.PHONY: all test canny_rv run clean directories

all: directories canny_rv test

# Create build folders
directories:
	@mkdir -p $(BUILD_HOST) $(BUILD_RV)

# 1. Build & Run Host-side Tests
# Uses all source files EXCEPT main.cpp, plus the test files
test: directories
	$(HOST_CXX) $(COMMON_FLAGS) $(SRCS_NO_MAIN) $(TEST_SRCS) -o $(BUILD_HOST)/test_runner $(GTEST_FLAGS)
	@echo "--- Running Host Tests ---"
	./$(BUILD_HOST)/test_runner

# 2. Build RISC-V Binary
# Uses ALL source files including main.cpp
canny_rv: directories
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o $(BUILD_RV)/canny_rv

# 3. Execute on QEMU
run: canny_rv
	@echo "--- Running on QEMU (VLEN=$(VLEN)) ---"
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN) ./$(BUILD_RV)/canny_rv

# 4. Clean up
clean:
	rm -rf $(BUILD_HOST) $(BUILD_RV)