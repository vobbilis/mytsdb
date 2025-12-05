# TSDB Project Makefile
# All project-level builds and cleans MUST be done via these makefile targets

.PHONY: all configure build clean clean-all test test-all test-unit test-integration test-parser test-summary test-background test-background-status test-background-stop install deps deps-macos deps-linux help

# Default target
all: configure build

# Build directory
BUILD_DIR := build

# CMake configuration
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DTSDB_SEMVEC=OFF -DENABLE_PARQUET=ON

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

test-promql-full: build
	@echo "Running comprehensive PromQL tests with data generation..."
	./scripts/run_promql_comprehensive.sh

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

test-wal-performance: build
	@echo "Running WAL performance benchmarks..."
	$(MAKE) -C $(BUILD_DIR) test-wal-performance

test-memory-efficiency: build
	@echo "Running memory efficiency performance tests..."
	$(MAKE) -C $(BUILD_DIR) test-memory-efficiency

test-indexing-performance: build
	@echo "Running indexing performance tests..."
	$(MAKE) -C $(BUILD_DIR) test-indexing-performance

test-concurrency-performance: build
	@echo "Running concurrency performance tests..."
	$(MAKE) -C $(BUILD_DIR) test-concurrency-performance

test-all-performance: build
	@echo "Running all performance tests..."
	$(MAKE) -C $(BUILD_DIR) test-all-performance

# Clean test results
test-clean:
	@echo "Cleaning test results..."
	if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) test-clean; fi
	rm -rf test_results

# Show test summary
test-summary: build
	@echo "Test targets summary:"
	$(MAKE) -C $(BUILD_DIR) test-summary

# Run tests in background (for overnight runs)
# Output is saved to test_results/background_test_<timestamp>.log
# PID is saved to test_results/background_test.pid
test-background: build
	@echo "Starting background test run..."
	@mkdir -p test_results
	@TIMESTAMP=$$(date +%Y%m%d_%H%M%S); \
	LOG_FILE="test_results/background_test_$$TIMESTAMP.log"; \
	PID_FILE="test_results/background_test.pid"; \
	echo "Test log: $$LOG_FILE"; \
	echo "PID file: $$PID_FILE"; \
	echo "Starting at: $$(date)" | tee -a $$LOG_FILE; \
	echo "Running: make test-all" | tee -a $$LOG_FILE; \
	echo "" | tee -a $$LOG_FILE; \
	(nohup $(MAKE) -C $(BUILD_DIR) test-all >> $$LOG_FILE 2>&1; \
	 echo "" | tee -a $$LOG_FILE; \
	 echo "Completed at: $$(date)" | tee -a $$LOG_FILE; \
	 echo "Exit code: $$?" | tee -a $$LOG_FILE) & \
	echo $$! > $$PID_FILE; \
	echo "Background test started with PID: $$(cat $$PID_FILE)"; \
	echo "Monitor progress with: tail -f $$LOG_FILE"; \
	echo "Check status with: make test-background-status"; \
	echo "Stop with: make test-background-stop"

# Run tests in background with caffeinate (prevents laptop hibernation)
# This is the recommended way to run overnight tests on macOS
test-background-caffeinate: build
	@echo "Starting background test run with caffeinate (prevents hibernation)..."
	@mkdir -p test_results
	@TIMESTAMP=$$(date +%Y%m%d_%H%M%S); \
	LOG_FILE="test_results/background_test_$$TIMESTAMP.log"; \
	PID_FILE="test_results/background_test.pid"; \
	echo "Test log: $$LOG_FILE"; \
	echo "PID file: $$PID_FILE"; \
	echo "Starting at: $$(date)" | tee -a $$LOG_FILE; \
	echo "Running: make test-all (with caffeinate to prevent hibernation)" | tee -a $$LOG_FILE; \
	echo "" | tee -a $$LOG_FILE; \
	(caffeinate -d -i -m -w bash -c "$(MAKE) -C $(BUILD_DIR) test-all >> $$LOG_FILE 2>&1; echo '' >> $$LOG_FILE; echo 'Completed at: '$$(date) >> $$LOG_FILE; echo 'Exit code: '$$? >> $$LOG_FILE") & \
	echo $$! > $$PID_FILE; \
	echo "Background test started with PID: $$(cat $$PID_FILE)"; \
	echo "Monitor progress with: tail -f $$LOG_FILE"; \
	echo "Check status with: make test-background-status"; \
	echo "Stop with: make test-background-stop"

# Check status of background test
test-background-status:
	@PID_FILE="test_results/background_test.pid"; \
	if [ ! -f $$PID_FILE ]; then \
		echo "No background test running (PID file not found)"; \
		exit 0; \
	fi; \
	PID=$$(cat $$PID_FILE 2>/dev/null); \
	if [ -z "$$PID" ]; then \
		echo "No background test running (PID file empty)"; \
		rm -f $$PID_FILE; \
		exit 0; \
	fi; \
	if ps -p $$PID > /dev/null 2>&1; then \
		echo "Background test is RUNNING (PID: $$PID)"; \
		LOG_FILE=$$(ls -t test_results/background_test_*.log 2>/dev/null | head -1); \
		if [ -n "$$LOG_FILE" ]; then \
			echo "Latest log: $$LOG_FILE"; \
			echo "Last 5 lines:"; \
			tail -5 $$LOG_FILE 2>/dev/null || echo "  (log file not readable)"; \
		fi; \
	else \
		echo "Background test is NOT running (PID $$PID not found)"; \
		rm -f $$PID_FILE; \
		LOG_FILE=$$(ls -t test_results/background_test_*.log 2>/dev/null | head -1); \
		if [ -n "$$LOG_FILE" ]; then \
			echo "Latest log: $$LOG_FILE"; \
			echo "Final status:"; \
			tail -10 $$LOG_FILE 2>/dev/null || echo "  (log file not readable)"; \
		fi; \
	fi

# Stop background test (if running)
test-background-stop:
	@PID_FILE="test_results/background_test.pid"; \
	if [ ! -f $$PID_FILE ]; then \
		echo "No background test running (PID file not found)"; \
		exit 0; \
	fi; \
	PID=$$(cat $$PID_FILE 2>/dev/null); \
	if [ -z "$$PID" ]; then \
		echo "No background test running (PID file empty)"; \
		rm -f $$PID_FILE; \
		exit 0; \
	fi; \
	if ps -p $$PID > /dev/null 2>&1; then \
		echo "Stopping background test (PID: $$PID)..."; \
		kill $$PID 2>/dev/null || true; \
		sleep 2; \
		if ps -p $$PID > /dev/null 2>&1; then \
			echo "Force killing background test..."; \
			kill -9 $$PID 2>/dev/null || true; \
		fi; \
		rm -f $$PID_FILE; \
		echo "Background test stopped"; \
	else \
		echo "Background test is not running (PID $$PID not found)"; \
		rm -f $$PID_FILE; \
	fi

# Install dependencies
deps:
	@echo "Detecting OS and installing dependencies..."
	@if [ "$$(uname)" = "Darwin" ]; then \
		$(MAKE) deps-macos; \
	elif [ "$$(uname)" = "Linux" ]; then \
		$(MAKE) deps-linux; \
	else \
		echo "Unsupported OS. Please install dependencies manually."; \
		exit 1; \
	fi

deps-macos:
	@echo "Installing dependencies on macOS (using Homebrew)..."
	@if ! command -v brew > /dev/null; then \
		echo "Error: Homebrew not found. Install from https://brew.sh"; \
		exit 1; \
	fi
	@echo "Installing required dependencies..."
	brew install cmake
	@echo "Installing optional but recommended dependencies..."
	@brew install tbb spdlog googletest grpc protobuf rapidjson cpp-httplib snappy || echo "Some optional dependencies failed to install (this is OK)"
	@echo "Dependencies installed successfully!"

deps-linux:
	@echo "Installing dependencies on Linux..."
	@if command -v apt-get > /dev/null; then \
		echo "Detected Debian/Ubuntu, using apt-get..."; \
		sudo apt-get update; \
		sudo apt-get install -y build-essential cmake; \
		sudo apt-get install -y libtbb-dev libspdlog-dev libgtest-dev libgrpc++-dev protobuf-compiler libprotobuf-dev rapidjson-dev libhttplib-dev libsnappy-dev || echo "Some optional dependencies failed to install (this is OK)"; \
	elif command -v dnf > /dev/null; then \
		echo "Detected Fedora/RHEL/CentOS, using dnf..."; \
		sudo dnf install -y gcc-c++ cmake make; \
		sudo dnf install -y tbb-devel spdlog-devel gtest-devel grpc-devel protobuf-devel protobuf-compiler snappy-devel || echo "Some optional dependencies failed to install (this is OK)"; \
	else \
		echo "Error: Unsupported Linux distribution. Please install dependencies manually."; \
		exit 1; \
	fi
	@echo "Dependencies installed successfully!"

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
	@echo "Dependency Targets:"
	@echo "  deps             - Install dependencies (auto-detects OS)"
	@echo "  deps-macos       - Install dependencies on macOS (Homebrew)"
	@echo "  deps-linux       - Install dependencies on Linux (apt/dnf)"
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
	@echo "  test-wal-performance      - WAL performance benchmarks (6 tests)"
	@echo "  test-memory-efficiency    - Memory efficiency performance tests (6 tests)"
	@echo "  test-indexing-performance - Indexing performance tests (8 tests)"
	@echo "  test-concurrency-performance - Concurrency performance tests (8 tests)"
	@echo "  test-all-performance      - All performance tests (28 tests)"
	@echo "  test-summary               - Show available test targets"
	@echo ""
	@echo "Background Test Targets:"
	@echo "  test-background            - Run all tests in background (for overnight runs)"
	@echo "  test-background-status     - Check status of background test"
	@echo "  test-background-stop       - Stop running background test"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make test-storage-unit     # Run just storage unit tests"
	@echo "  make clean && make test-all # Clean and run all tests"
	@echo "  make rebuild               # Complete rebuild"
	@echo "  make test-background       # Start tests in background, close laptop"
	@echo "  make test-background-status # Check if tests are still running"
