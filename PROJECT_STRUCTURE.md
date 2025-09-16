# MyTSDB Project Structure

This document defines the proper project structure and best practices for file organization.

## 📁 Project Root Structure

```
mytsdb/
├── README.md                    # Project overview and setup instructions
├── CMakeLists.txt              # Main CMake configuration
├── FEATURES.md                 # Feature documentation
├── NEWRIGORCLIST.md            # Development guidelines
├── docker-compose.yml          # Docker development environment
├── docs/                       # Documentation
│   ├── architecture/           # Architecture documentation
│   │   └── diagrams/           # Architecture diagrams (html, jpeg, md, png)
│
├── src/                        # Source code
│   ├── core/                   # Core functionality
│   ├── histogram/              # Histogram implementations
│   ├── storage/                # Storage implementations
│   └── tsdb/                   # Main TSDB components
│
├── include/                    # Header files
│   └── tsdb/                   # Public API headers
│
├── test/                       # Test code
│   ├── unit/                   # Unit tests
│   ├── integration/            # Integration tests
│   ├── benchmark/              # Performance benchmarks
│   ├── load/                   # Load tests
│   ├── stress/                 # Stress tests
│   ├── scalability/            # Scalability tests
│   └── data/                   # Test data files
│
├── docs/                       # Documentation
│   ├── architecture/           # Architecture documentation
│   │   └── diagrams/           # Architecture diagrams
│   ├── planning/               # Development plans
│   ├── analysis/               # Technical analysis
│   └── testing/                # Testing documentation
│
├── examples/                   # Example code
│   ├── cpp/                    # C++ examples
│   └── go/                     # Go examples
│
├── scripts/                    # Build and utility scripts
│   ├── run_tests.sh            # Test runner script
│   └── update_test_status.py   # Status update script
│
├── config/                     # Configuration files
│   └── config.yaml             # Main configuration
│
├── common/                     # Shared resources
│   ├── proto/                  # Protocol buffer definitions
│   └── schemas/                # JSON schemas
│
├── deployment/                 # Deployment configurations
│   ├── docker/                 # Docker configurations
│   ├── kubernetes/             # Kubernetes manifests
│   └── terraform/              # Infrastructure as code
│
├── benchmarks/                 # Benchmark results and scenarios
│   ├── results/                # Benchmark results
│   └── scenarios/              # Benchmark scenarios
│
├── otel-proto/                 # OpenTelemetry protocol definitions
│
├── cmake/                      # CMake configuration files
│
└── build/                      # Build artifacts (generated)
    ├── src/                    # Compiled source
    ├── test/                   # Compiled tests
    └── [other build artifacts]
```

## 🚫 Files That Should NEVER Be in Project Root

### Test Files
- ❌ `*_test.cpp` - Should be in `test/` directory
- ❌ `*_test.xml` - Test results should be in `test_results/`
- ❌ `debug_*_test` - Debug executables should be in `build/`
- ❌ `*_test_data/` - Test data should be in `test/data/`

### Build Artifacts
- ❌ `*.o` - Object files should be in `build/`
- ❌ `*.so`, `*.dylib` - Libraries should be in `build/`
- ❌ `debug_*` - Debug builds should be in `build/debug/`

### Temporary Files
- ❌ `*.tmp`, `*.temp` - Temporary files should be in `/tmp/`
- ❌ `debug_*.log` - Log files should be in `logs/` or `test_results/logs/`

### Scripts
- ❌ `*.sh` - Shell scripts should be in `scripts/`
- ❌ `debug_*.sh` - Debug scripts should be in `scripts/debug/`

## ✅ Best Practices

### 1. Test Organization
- **Unit tests**: `test/unit/`
- **Integration tests**: `test/integration/`
- **Performance tests**: `test/benchmark/`
- **Test data**: `test/data/`
- **Test results**: `test_results/`

### 2. Build Organization
- **All build artifacts**: `build/`
- **Debug builds**: `build/debug/`
- **Release builds**: `build/release/`
- **Object files**: `build/obj/`

### 3. Documentation Organization
- **Architecture docs**: `docs/architecture/`
- **Development plans**: `docs/planning/`
- **API docs**: `docs/api/`
- **Analysis docs**: `docs/analysis/`

### 4. Script Organization
- **Build scripts**: `scripts/build/`
- **Test scripts**: `scripts/test/`
- **Utility scripts**: `scripts/utils/`
- **Debug scripts**: `scripts/debug/`

## 🔧 Development Guidelines

### Creating New Files
1. **Source files**: Always in `src/` with proper subdirectory
2. **Header files**: Always in `include/` with proper subdirectory
3. **Test files**: Always in `test/` with proper subdirectory
4. **Scripts**: Always in `scripts/` with proper subdirectory
5. **Documentation**: Always in `docs/` with proper subdirectory

### Temporary Development
- Use `build/temp/` for temporary development files
- Use `test_results/temp/` for temporary test files
- Clean up temporary files before committing

### Debugging
- Debug executables go in `build/debug/`
- Debug logs go in `test_results/logs/debug/`
- Debug test data goes in `test/data/debug/`

## 🧹 Cleanup Commands

### Remove Test Artifacts
```bash
# Remove XML test results
rm -f *.xml

# Remove debug executables
rm -f debug_*_test

# Remove debug source files
rm -f debug_*_test.cpp

# Remove test data directories
rm -rf *_test_data/

# Remove object files
rm -f *.o
```

### Remove Build Artifacts
```bash
# Remove all build artifacts
rm -rf build/

# Remove specific build types
rm -rf build/debug/
rm -rf build/release/
```

### Remove Temporary Files
```bash
# Remove temporary files
rm -f *.tmp *.temp

# Remove log files
rm -f *.log
```

## 📋 Maintenance Checklist

- [ ] No test files in project root
- [ ] No build artifacts in project root
- [ ] No temporary files in project root
- [ ] No debug files in project root
- [ ] All scripts in `scripts/` directory
- [ ] All documentation in `docs/` directory
- [ ] All test data in `test/data/` directory
- [ ] All build artifacts in `build/` directory

This structure ensures a clean, maintainable, and professional project organization.
