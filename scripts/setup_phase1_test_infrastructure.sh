#!/bin/bash

# Phase 1: Memory Access Pattern Optimization - Test Infrastructure Setup
# This script sets up the test infrastructure for Phase 1 memory optimization

echo "🔧 Setting up Phase 1 test infrastructure..."

# Create test result directories
mkdir -p test-results/phase1
mkdir -p test-results/phase1/unit
mkdir -p test-results/phase1/integration
mkdir -p test-results/phase1/benchmark

# Setup memory profiling tools
echo "📊 Setting up memory profiling tools..."

# Check if Valgrind is available
if command -v valgrind &> /dev/null; then
    echo "✅ Valgrind found - memory leak detection enabled"
else
    echo "⚠️  Valgrind not found - install with: brew install valgrind"
fi

# Check if AddressSanitizer is available
if [[ "$CMAKE_BUILD_TYPE" == "Debug" ]]; then
    echo "✅ AddressSanitizer enabled in Debug build"
else
    echo "⚠️  AddressSanitizer only available in Debug builds"
fi

# Setup performance monitoring
echo "📈 Setting up performance monitoring..."

# Check if perf is available
if command -v perf &> /dev/null; then
    echo "✅ perf found - CPU performance monitoring enabled"
else
    echo "⚠️  perf not found - install with: brew install perf"
fi

# Setup cache analysis tools
echo "🔍 Setting up cache analysis tools..."

# Check if cachegrind is available
if command -v cachegrind &> /dev/null; then
    echo "✅ Cachegrind found - cache analysis enabled"
else
    echo "⚠️  Cachegrind not found - install with: brew install valgrind"
fi

# Setup benchmark framework
echo "⚡ Setting up benchmark framework..."

# Create benchmark configuration
cat > test-results/phase1/benchmark_config.json << EOF
{
    "phase1_benchmarks": {
        "memory_access_pattern": {
            "iterations": 1000,
            "warmup_iterations": 100,
            "timeout_seconds": 300
        },
        "cache_alignment": {
            "iterations": 500,
            "warmup_iterations": 50,
            "timeout_seconds": 180
        },
        "allocation_speed": {
            "iterations": 10000,
            "warmup_iterations": 1000,
            "timeout_seconds": 600
        }
    }
}
EOF

echo "✅ Phase 1 test infrastructure setup complete!"
echo "📁 Test results will be stored in: test-results/phase1/"
echo "🔧 Memory profiling tools configured"
echo "📊 Performance monitoring tools ready"
echo "⚡ Benchmark framework configured"
