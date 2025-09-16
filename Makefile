# TSDB Project Makefile
# All project-level builds and cleans MUST be done via these makefile targets

.PHONY: all configure build clean clean-all test test-all test-unit test-integration test-parser test-summary install help

# Default target
all: configure build

# Build directory
BUILD_DIR := build

# CMake configuration
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DTSDB_SEMVEC=OFF

# Configure the project with CMake
configure:
	@echo "Configuring TSDB project with CMake..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. $(CMAKE_FLAGS)

# Build all targets
build: configure
	@echo "Building TSDB project..."
	$(MAKE) -C $(BUILD_DIR) -j$(shell nproc 2>/dev/null || echo 4)

# Clean build artifacts (keep configuration)
clean:
	@echo "Cleaning build artifacts..."
	if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) clean; fi

# Clean everything including CMake cache and build directory
clean-all:
	@echo "Performing complete clean..."
	rm -rf $(BUILD_DIR) test_results CMakeFiles CMakeCache.txt CTestTestfile.cmake tsdb-config-version.cmake tsdb-config.cmake compile_commands.json

# Test targets - all tests must go through CMake build system
test-all: build
	@echo "Running all tests (632+ test cases)..."
	$(MAKE) -C $(BUILD_DIR) test-all

test-unit: build
	@echo "Running unit tests (357 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-unit

test-integration: build
	@echo "Running integration tests (177 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-integration

test-parser: build
	@echo "Running PromQL parser tests (34 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-parser

test-main-integration: build
	@echo "Running main integration test suite (124 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-main-integration

test-storageimpl-phases: build
	@echo "Running StorageImpl phases tests (64 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-storageimpl-phases

test-core-unit: build
	@echo "Running core unit tests (38 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-core-unit

test-storage-unit: build
	@echo "Running storage unit tests (60 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-storage-unit

test-cache-unit: build
	@echo "Running cache unit tests (28 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-cache-unit

test-compression-unit: build
	@echo "Running compression unit tests (19 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-compression-unit

test-histogram-unit: build
	@echo "Running histogram unit tests (22 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-histogram-unit

test-individual-integration: build
	@echo "Running individual integration tests (53 tests)..."
	$(MAKE) -C $(BUILD_DIR) test-individual-integration

# Clean test results
test-clean:
	@echo "Cleaning test results..."
	if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) test-clean; fi
	rm -rf test_results

# Show test summary
test-summary: build
	@echo "Test targets summary:"
	$(MAKE) -C $(BUILD_DIR) test-summary

# Install the project
install: build
	@echo "Installing TSDB..."
	$(MAKE) -C $(BUILD_DIR) install

# Rebuild from scratch
rebuild: clean-all all

# Help target
help:
	@echo "TSDB Project Makefile Targets:"
	@echo ""
	@echo "Build Targets:"
	@echo "  all              - Configure and build everything (default)"
	@echo "  configure        - Run CMake configuration"
	@echo "  build            - Build all components"
	@echo "  rebuild          - Clean everything and rebuild from scratch"
	@echo "  install          - Install the built components"
	@echo ""
	@echo "Clean Targets:"
	@echo "  clean            - Clean build artifacts (keep configuration)"
	@echo "  clean-all        - Complete clean (remove build dir and cache)"
	@echo "  test-clean       - Clean test results only"
	@echo ""
	@echo "Test Targets (632+ total tests):"
	@echo "  test-all                    - Run ALL tests (632+ tests, 120s timeout)"
	@echo "  test-main-integration      - Main integration suite (124 tests, 5min)"
	@echo "  test-storageimpl-phases    - StorageImpl phases (64 tests, 10min)"
	@echo "  test-unit                  - All unit tests (357 tests, 30s timeout)"
	@echo "  test-integration           - All integration tests (177 tests, 60s)"
	@echo "  test-parser                - PromQL parser tests (34 tests, 15s)"
	@echo "  test-core-unit             - Core unit tests (38 tests, 1min)"
	@echo "  test-storage-unit          - Storage unit tests (60 tests, 2min)"
	@echo "  test-cache-unit            - Cache unit tests (28 tests, 1min)"
	@echo "  test-compression-unit      - Compression unit tests (19 tests, 1min)"
	@echo "  test-histogram-unit        - Histogram unit tests (22 tests, 1min)"
	@echo "  test-individual-integration - Individual integration tests (53 tests)"
	@echo "  test-summary               - Show available test targets"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make test-storage-unit     # Run just storage unit tests"
	@echo "  make clean && make test-all # Clean and run all tests"
	@echo "  make rebuild               # Complete rebuild"
