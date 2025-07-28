.PHONY: all clean test bench proto docker-build cpp cpp-test cpp-bench

# Default target
all: cpp

# Protocol buffer generation
proto:
	@echo "Checking protocol buffers..."
	@if [ ! -f "src/tsdb/proto/gen/tsdb.pb.h" ]; then \
		echo "Warning: Protocol buffer files not found. Skipping generation for now."; \
		echo "If you need to regenerate protobuf files, please run: protoc --cpp_out=src/tsdb/proto/gen common/proto/*.proto"; \
	fi

# C++ targets
cpp: proto
	@echo "Building C++ implementation..."
	@mkdir -p build
	@cd build && cmake -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF ..
	@cd build && make -j$(nproc)

# Test target (alias for cpp-test)
test: cpp-test

cpp-test:
	@echo "Testing C++ implementation..."
	@cd build && make test

cpp-bench:
	@echo "Benchmarking C++ implementation..."
	@cd build && make benchmark

# Benchmarks
bench: cpp-bench
	@echo "Running benchmarks..."
	@cd benchmarks && ./run_benchmarks.sh

# Docker builds
docker-build:
	@echo "Building Docker images..."
	@cd deployment/docker && docker-compose build

# Clean all builds
clean:
	@echo "Cleaning build directory..."
	@rm -rf build/
	@echo "Build directory cleaned." 