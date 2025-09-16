# StorageImpl Phases Test Data Directory

This directory contains the test data directories for the StorageImpl integration test phases. These directories are created dynamically by the StorageImpl system when tests run.

## Directory Structure

```
test/data/storageimpl_phases/
├── phase1/                    # Phase 1: Object Pool Integration Tests
├── phase2/                    # Phase 2: Cache Hierarchy Integration Tests
├── phase2_custom/             # Phase 2: Custom Configuration Tests
├── phase3/                    # Phase 3: Compression Integration Tests
├── phase3_algo/               # Phase 3: Algorithm Selection Tests
├── phase3_adaptive/           # Phase 3: Adaptive Compression Tests
├── phase3_single/             # Phase 3: Single Sample Tests
├── phase3_no_compression/     # Phase 3: No Compression Tests
├── debug/                     # Debug Tests (Cache & Stats)
├── size_analysis/             # Object Pool Size Analysis Tests
└── education/                 # Object Pool Education Tests
```

## How It Works

1. **Dynamic Creation**: These directories are created automatically by the StorageImpl system when tests run
2. **Automatic Cleanup**: Directories are created fresh for each test run
3. **Isolated Testing**: Each test phase has its own isolated data directory
4. **No Manual Setup**: No need to manually create these directories

## StorageImpl Components That Create Directories

- **BlockManager**: Creates block storage directories using `std::filesystem::create_directories()`
- **CacheHierarchy**: Creates cache directories (l2, l3) using `std::filesystem::create_directories()`
- **MemoryMappedCache**: Creates memory-mapped cache directories

## Test Configuration

Each test script configures its data directory using:
```cpp
config.data_dir = "./test/data/storageimpl_phases/[phase_name]";
```

This ensures:
- **Organized Structure**: All test data is under a single, organized directory
- **Easy Cleanup**: Can easily clean all test data by removing the parent directory
- **Clear Separation**: Each test phase has its own isolated data space
- **Maintainable**: Easy to understand and manage test data structure
