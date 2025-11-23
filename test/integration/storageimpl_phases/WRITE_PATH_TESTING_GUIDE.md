# Write Path Refactoring - Testing Guide

## Overview

This document describes the integration tests created for the Write Path Refactoring implementation (Phases 1, 2, and 3 from `DESIGN_FIXES.md`).

## Test File Location

```
test/integration/storageimpl_phases/write_path_refactoring_integration_test.cpp
```

## Test Categories

### Phase 1 Tests: Foundational Changes
Tests the removal of in-memory duplication and addition of WAL, Index, and concurrent_hash_map.

- **`Phase1_WALCreatedOnInit`**: Verifies WAL directory is created during initialization
- **`Phase1_WALReceivesWrites`**: Verifies writes are logged to the WAL for durability
- **`Phase1_ConcurrentWritesDontBlock`**: Tests that concurrent writes scale with CPU cores using the concurrent_hash_map

### Phase 2 Tests: Series Write Logic
Tests the implementation of `Series::append()` and `Series::seal_block()`.

- **`Phase2_SeriesAppendCreatesBlock`**: Verifies that writing samples creates in-memory blocks
- **`Phase2_BlockSealingOnFullBlock`**: Tests that blocks are sealed when they exceed the threshold (120 samples)
- **`Phase2_MultipleSeriesIndependentBlocks`**: Verifies each series maintains its own independent blocks

### Phase 3 Tests: BlockManager Integration
Tests the serialization and persistence of blocks to disk.

- **`Phase3_BlockPersistenceToDisk`**: Verifies sealed blocks are written to disk
- **`Phase3_SerializedBlockContainsData`**: Ensures serialized blocks contain actual data (non-zero size)
- **`Phase3_MultipleBlocksCreatedForLargeDataset`**: Tests that large datasets create multiple blocks

### End-to-End Tests
Tests the complete write pipeline from API to disk storage.

- **`EndToEnd_CompleteWritePipeline`**: Tests the full flow: API → WAL → Series → Block → Serialize → Persist
- **`EndToEnd_ConcurrentWritesWithPersistence`**: Tests concurrent writes with block sealing and persistence
- **`EndToEnd_WriteReadConsistency`**: Verifies written data can be read back correctly

## Running the Tests

### Build the Tests

```bash
cd /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/build
cmake ..
make tsdb_integration_test_suite
```

### Run All Integration Tests

```bash
./test/tsdb_integration_test_suite
```

### Run Only Write Path Refactoring Tests

```bash
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.*"
```

### Run Specific Test Categories

```bash
# Run only Phase 1 tests
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.Phase1_*"

# Run only Phase 2 tests
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.Phase2_*"

# Run only Phase 3 tests
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.Phase3_*"

# Run only End-to-End tests
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.EndToEnd_*"
```

### Run with Verbose Output

```bash
./test/tsdb_integration_test_suite --gtest_filter="WritePathRefactoringIntegrationTest.*" --gtest_verbose
```

## Expected Test Results

### Successful Test Output

When all phases are correctly implemented, you should see:

```
[==========] Running 12 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 12 tests from WritePathRefactoringIntegrationTest
[ RUN      ] WritePathRefactoringIntegrationTest.Phase1_WALCreatedOnInit
[       OK ] WritePathRefactoringIntegrationTest.Phase1_WALCreatedOnInit
[ RUN      ] WritePathRefactoringIntegrationTest.Phase1_WALReceivesWrites
[       OK ] WritePathRefactoringIntegrationTest.Phase1_WALReceivesWrites
...
[----------] 12 tests from WritePathRefactoringIntegrationTest (XXXms total)

[==========] 12 tests from 1 test suite ran. (XXXms total)
[  PASSED  ] 12 tests.
```

### Verification Points

The tests verify:

1. **WAL Durability**: Files created in `{test_dir}/wal/`
2. **Concurrent Scalability**: Multiple threads can write simultaneously
3. **Block Creation**: In-memory blocks are created for each series
4. **Block Sealing**: Blocks are sealed when they reach 120 samples
5. **Serialization**: Sealed blocks are converted to byte streams
6. **Persistence**: Block files appear in `{test_dir}/0/` (hot tier) with `.block` extension
7. **Data Integrity**: Serialized blocks have non-zero size

## Troubleshooting

### Test Failures

If tests fail, check:

1. **WAL Not Created**: Verify `Index` and `WriteAheadLog` are initialized in `StorageImpl::init()`
2. **No Block Files**: Check that `seal_and_persist_block()` is uncommented in `StorageImpl::write()`
3. **Empty Block Files**: Verify `BlockImpl::serialize()` is properly collecting compressed data
4. **Concurrent Test Fails**: Ensure `tbb::concurrent_hash_map` is being used instead of global mutex

### Debug Output

Tests include helpful debug output:
- Concurrent write timings
- Block file counts and sizes
- File paths for manual inspection

### Manual Verification

You can manually inspect test artifacts in `/tmp/tsdb_write_path_test_*`:
```bash
# Find test directories
ls -la /tmp/tsdb_write_path_test_*

# Check WAL files
ls -la /tmp/tsdb_write_path_test_*/wal/

# Check block files
ls -la /tmp/tsdb_write_path_test_*/0/  # Hot tier
ls -la /tmp/tsdb_write_path_test_*/1/  # Warm tier
ls -la /tmp/tsdb_write_path_test_*/2/  # Cold tier
```

## Test Coverage

These tests cover:
- ✅ WAL integration
- ✅ Concurrent write operations
- ✅ Series append logic
- ✅ Block sealing logic
- ✅ Block serialization
- ✅ Block persistence to disk
- ✅ Multi-tier storage
- ✅ End-to-end write pipeline

## Next Steps

After these tests pass:
1. Implement the `Index::find_series()` method for query operations
2. Implement the `WAL::replay()` method for crash recovery
3. Add read path integration tests
4. Add performance benchmarks for the write path
5. Test with real-world workloads

## Related Documentation

- `DESIGN_FIXES.md`: Original design document with detailed phase descriptions
- `test/tsdb/storage/storage_test.cpp`: Unit tests for storage interface
- `test/integration/storageimpl_phases/phase2_block_management_integration_test.cpp`: Related block management tests
