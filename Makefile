.PHONY: all clean test bench proto docker-build

# Default target
all: go cpp

# Protocol buffer generation
proto:
	@echo "Generating protocol buffers..."
	@cd tools/proto && ./generate.sh

# Go targets
go: proto
	@echo "Building Go implementation..."
	@cd go-tsdb && make build

go-test:
	@echo "Testing Go implementation..."
	@cd go-tsdb && make test

go-bench:
	@echo "Benchmarking Go implementation..."
	@cd go-tsdb && make bench

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

# Comparative benchmarks
bench: go-bench cpp-bench
	@echo "Running comparative benchmarks..."
	@cd benchmarks && ./run_benchmarks.sh

# Docker builds
docker-build:
	@echo "Building Docker images..."
	@cd deployment/docker && docker-compose build

# Clean all builds
clean:
	@cd go-tsdb && make clean
	@cd cpp-tsdb && make clean
	@rm -rf build/ 