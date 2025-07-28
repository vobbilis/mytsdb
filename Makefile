.PHONY: all clean test bench proto docker-build

# Default target
all: cpp

# Protocol buffer generation
proto:
	@echo "Generating protocol buffers..."
	@cd tools/proto && ./generate.sh

# C++ targets
cpp: proto
	@echo "Building C++ implementation..."
	@cd cpp-tsdb && make build

cpp-test:
	@echo "Testing C++ implementation..."
	@cd cpp-tsdb && make test

cpp-bench:
	@echo "Benchmarking C++ implementation..."
	@cd cpp-tsdb && make bench

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
	@cd cpp-tsdb && make clean
	@rm -rf build/ 