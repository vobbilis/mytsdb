# B+ Tree Secondary Index & HNSWLib Integration Plan

## Overview

This document outlines a **two-phase approach** to improving read performance in mytsdb:

1. **Phase A (This Sprint)**: B+ Tree Secondary Index for Parquet — verify read improvements with K8s benchmarks
2. **Phase B (Next Sprint)**: HNSWLib integration for vector similarity search

**Created**: December 7, 2025  
**Status**: Planning  
**Priority**: High  

---

# PHASE A: B+ Tree Secondary Index (Priority)

## Objective

Add a B+ tree secondary index to Parquet cold storage to achieve **O(log n) point lookups** instead of O(n) scans. Verify improvement using existing K8s benchmarks before proceeding to HNSW.

## Success Criteria

1. **Benchmark**: Run `make benchmark-quick` before and after
2. **Target**: Read latency improvement of **10x or more** for point queries
3. **Validation**: All existing tests pass (`make test-unit`, `make test-integration`)

## Implementation Plan

### TASK-BT1: Add Dependencies
**Effort**: 15 minutes  
**Files**: `CMakeLists.txt`

**Steps**:
- [ ] Add CRoaring library for deletion bitmap (or vendor header)
- [ ] Option: Add cpp-btree or tlx for B+ tree (or use custom impl)

```cmake
# In CMakeLists.txt
# Option 1: Use FetchContent for CRoaring
FetchContent_Declare(
  roaring
  GIT_REPOSITORY https://github.com/RoaringBitmap/CRoaring.git
  GIT_TAG v2.0.4
)
FetchContent_MakeAvailable(roaring)

# Option 2: Header-only - just copy roaring.hh to include/third_party/
```

**Completion Criteria**:
- [ ] `#include "roaring/roaring.hh"` compiles
- [ ] B+ tree header available (library or custom)

---

### TASK-BT2: Create Secondary Index Header
**File**: `include/tsdb/storage/parquet/secondary_index.h`  
**Effort**: 1 hour  
**Dependencies**: TASK-BT1

**Minimal Implementation** (focus on point lookups first):

```cpp
#ifndef TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_
#define TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_

#include <map>
#include <shared_mutex>
#include <string>
#include <optional>
#include <vector>

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {
namespace parquet {

/**
 * @brief Location of a row in Parquet file
 */
struct RowLocation {
    int32_t row_group_id;
    int64_t row_offset;
    
    bool operator==(const RowLocation& o) const {
        return row_group_id == o.row_group_id && row_offset == o.row_offset;
    }
};

/**
 * @brief Secondary index for O(log n) Parquet lookups
 * 
 * Phase 1: Use std::map (red-black tree, O(log n))
 * Phase 2: Upgrade to B+ tree if needed for range scans
 */
class ParquetSecondaryIndex {
public:
    ParquetSecondaryIndex() = default;
    ~ParquetSecondaryIndex() = default;
    
    // ========== INDEX BUILDING ==========
    
    /**
     * @brief Build index by scanning Parquet file metadata
     */
    core::Result<void> build_from_parquet(const std::string& parquet_path);
    
    /**
     * @brief Add single entry (used during writes)
     */
    void add_entry(const core::SeriesID& series_id, const RowLocation& location);
    
    /**
     * @brief Rebuild index from scratch
     */
    void clear();
    
    // ========== LOOKUPS ==========
    
    /**
     * @brief O(log n) point lookup
     */
    std::optional<RowLocation> lookup(const core::SeriesID& series_id) const;
    
    /**
     * @brief Check existence without getting location
     */
    bool contains(const core::SeriesID& series_id) const;
    
    /**
     * @brief Batch lookup (more efficient for multiple queries)
     */
    std::vector<std::pair<core::SeriesID, std::optional<RowLocation>>>
    batch_lookup(const std::vector<core::SeriesID>& series_ids) const;
    
    // ========== DELETION TRACKING ==========
    
    /**
     * @brief Mark series as deleted (soft delete)
     */
    void mark_deleted(const core::SeriesID& series_id);
    
    /**
     * @brief Check if series is deleted
     */
    bool is_deleted(const core::SeriesID& series_id) const;
    
    /**
     * @brief Get deletion count for compaction decisions
     */
    size_t deleted_count() const;
    
    // ========== PERSISTENCE ==========
    
    core::Result<void> save(const std::string& path);
    core::Result<void> load(const std::string& path);
    
    // ========== STATS ==========
    
    size_t size() const;
    size_t memory_usage_bytes() const;

private:
    mutable std::shared_mutex mutex_;
    
    // Phase 1: std::map (red-black tree) - O(log n) lookup
    // Easy to implement, good enough for initial validation
    std::map<core::SeriesID, RowLocation> index_;
    
    // Deletion tracking - simple set for Phase 1
    // Can upgrade to roaring bitmap in Phase 2 if memory becomes issue
    std::set<core::SeriesID> deleted_;
};

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb

#endif  // TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_
```

**Completion Criteria**:
- [ ] Header compiles
- [ ] API supports point lookup, add, delete, persistence

---

### TASK-BT3: Implement Secondary Index
**File**: `src/tsdb/storage/parquet/secondary_index.cpp`  
**Effort**: 2 hours  
**Dependencies**: TASK-BT2

```cpp
#include "tsdb/storage/parquet/secondary_index.h"

#include <fstream>
#include <parquet/api/reader.h>

namespace tsdb {
namespace storage {
namespace parquet {

core::Result<void> ParquetSecondaryIndex::build_from_parquet(
    const std::string& parquet_path) {
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_.clear();
    
    try {
        auto reader = ::parquet::ParquetFileReader::OpenFile(parquet_path);
        auto metadata = reader->metadata();
        
        // Find series_id column index
        int series_id_col = -1;
        auto schema = metadata->schema();
        for (int i = 0; i < schema->num_columns(); ++i) {
            if (schema->Column(i)->name() == "series_id") {
                series_id_col = i;
                break;
            }
        }
        
        if (series_id_col < 0) {
            return core::Result<void>::error(core::Error(
                core::ErrorCode::INVALID_ARGUMENT,
                "No series_id column found in Parquet file"));
        }
        
        // Scan each row group
        for (int rg = 0; rg < metadata->num_row_groups(); ++rg) {
            auto row_group = reader->RowGroup(rg);
            auto col_reader = row_group->Column(series_id_col);
            
            // Read series_id values and build index
            // Implementation depends on column type (string vs int)
            int64_t row_offset = 0;
            
            // ... read column values ...
            // For each (series_id, row_offset):
            //   index_[series_id] = {rg, row_offset};
            
            // Simplified: just track first occurrence per series
        }
        
        return core::Result<void>::ok();
        
    } catch (const std::exception& e) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::IO_ERROR,
            std::string("Failed to build index: ") + e.what()));
    }
}

void ParquetSecondaryIndex::add_entry(
    const core::SeriesID& series_id, 
    const RowLocation& location) {
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_[series_id] = location;
}

std::optional<RowLocation> ParquetSecondaryIndex::lookup(
    const core::SeriesID& series_id) const {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Check if deleted first
    if (deleted_.count(series_id)) {
        return std::nullopt;
    }
    
    auto it = index_.find(series_id);
    if (it != index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ParquetSecondaryIndex::contains(const core::SeriesID& series_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_.count(series_id) > 0 && deleted_.count(series_id) == 0;
}

void ParquetSecondaryIndex::mark_deleted(const core::SeriesID& series_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    deleted_.insert(series_id);
}

bool ParquetSecondaryIndex::is_deleted(const core::SeriesID& series_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return deleted_.count(series_id) > 0;
}

core::Result<void> ParquetSecondaryIndex::save(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::IO_ERROR, "Cannot open file for writing"));
    }
    
    // Magic + version
    uint32_t magic = 0x42545258;  // "BTRX"
    uint32_t version = 1;
    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Index entries
    size_t count = index_.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& [series_id, loc] : index_) {
        size_t len = series_id.size();
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(series_id.data(), len);
        ofs.write(reinterpret_cast<const char*>(&loc.row_group_id), sizeof(loc.row_group_id));
        ofs.write(reinterpret_cast<const char*>(&loc.row_offset), sizeof(loc.row_offset));
    }
    
    // Deleted entries
    size_t del_count = deleted_.size();
    ofs.write(reinterpret_cast<const char*>(&del_count), sizeof(del_count));
    for (const auto& series_id : deleted_) {
        size_t len = series_id.size();
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(series_id.data(), len);
    }
    
    return core::Result<void>::ok();
}

core::Result<void> ParquetSecondaryIndex::load(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::IO_ERROR, "Cannot open file for reading"));
    }
    
    // Verify magic + version
    uint32_t magic, version;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    if (magic != 0x42545258 || version != 1) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::INVALID_ARGUMENT, "Invalid index file format"));
    }
    
    // Read index entries
    index_.clear();
    size_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (size_t i = 0; i < count; ++i) {
        size_t len;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string series_id(len, '\0');
        ifs.read(series_id.data(), len);
        
        RowLocation loc;
        ifs.read(reinterpret_cast<char*>(&loc.row_group_id), sizeof(loc.row_group_id));
        ifs.read(reinterpret_cast<char*>(&loc.row_offset), sizeof(loc.row_offset));
        
        index_[series_id] = loc;
    }
    
    // Read deleted entries
    deleted_.clear();
    size_t del_count;
    ifs.read(reinterpret_cast<char*>(&del_count), sizeof(del_count));
    
    for (size_t i = 0; i < del_count; ++i) {
        size_t len;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string series_id(len, '\0');
        ifs.read(series_id.data(), len);
        
        deleted_.insert(series_id);
    }
    
    return core::Result<void>::ok();
}

size_t ParquetSecondaryIndex::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_.size();
}

size_t ParquetSecondaryIndex::memory_usage_bytes() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t bytes = 0;
    for (const auto& [k, v] : index_) {
        bytes += k.size() + sizeof(RowLocation) + 32;  // overhead estimate
    }
    for (const auto& k : deleted_) {
        bytes += k.size() + 32;
    }
    return bytes;
}

void ParquetSecondaryIndex::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_.clear();
    deleted_.clear();
}

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
```

**Completion Criteria**:
- [ ] Index builds from Parquet file
- [ ] Lookup returns correct RowLocation
- [ ] Save/load round-trips correctly
- [ ] Thread-safe operations

---

### TASK-BT4: Unit Tests
**File**: `test/unit/storage/parquet_secondary_index_test.cpp`  
**Effort**: 1.5 hours  
**Dependencies**: TASK-BT3

```cpp
#include <gtest/gtest.h>
#include "tsdb/storage/parquet/secondary_index.h"

using namespace tsdb::storage::parquet;

class ParquetSecondaryIndexTest : public ::testing::Test {
protected:
    ParquetSecondaryIndex index_;
};

TEST_F(ParquetSecondaryIndexTest, AddAndLookup) {
    RowLocation loc{0, 100};
    index_.add_entry("series_1", loc);
    
    auto result = index_.lookup("series_1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->row_group_id, 0);
    EXPECT_EQ(result->row_offset, 100);
}

TEST_F(ParquetSecondaryIndexTest, LookupNotFound) {
    auto result = index_.lookup("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ParquetSecondaryIndexTest, Contains) {
    index_.add_entry("series_1", {0, 0});
    
    EXPECT_TRUE(index_.contains("series_1"));
    EXPECT_FALSE(index_.contains("series_2"));
}

TEST_F(ParquetSecondaryIndexTest, DeletedSeriesNotReturned) {
    index_.add_entry("series_1", {0, 0});
    index_.mark_deleted("series_1");
    
    EXPECT_FALSE(index_.lookup("series_1").has_value());
    EXPECT_FALSE(index_.contains("series_1"));
    EXPECT_TRUE(index_.is_deleted("series_1"));
}

TEST_F(ParquetSecondaryIndexTest, Persistence) {
    // Add entries
    index_.add_entry("series_a", {0, 10});
    index_.add_entry("series_b", {1, 20});
    index_.mark_deleted("series_b");
    
    // Save
    std::string path = "/tmp/test_secondary_index.bin";
    ASSERT_TRUE(index_.save(path).is_ok());
    
    // Load into new index
    ParquetSecondaryIndex loaded;
    ASSERT_TRUE(loaded.load(path).is_ok());
    
    // Verify
    EXPECT_EQ(loaded.size(), 2);
    EXPECT_TRUE(loaded.lookup("series_a").has_value());
    EXPECT_FALSE(loaded.lookup("series_b").has_value());  // deleted
    EXPECT_TRUE(loaded.is_deleted("series_b"));
}

TEST_F(ParquetSecondaryIndexTest, BatchLookup) {
    index_.add_entry("s1", {0, 1});
    index_.add_entry("s2", {0, 2});
    index_.add_entry("s3", {1, 3});
    
    auto results = index_.batch_lookup({"s1", "s2", "s4"});
    
    ASSERT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].second.has_value());   // s1 found
    EXPECT_TRUE(results[1].second.has_value());   // s2 found
    EXPECT_FALSE(results[2].second.has_value());  // s4 not found
}

TEST_F(ParquetSecondaryIndexTest, LookupPerformance) {
    // Add 100K entries
    for (int i = 0; i < 100000; ++i) {
        index_.add_entry("series_" + std::to_string(i), {i / 10000, i % 10000});
    }
    
    // Benchmark lookups
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        index_.lookup("series_" + std::to_string(rand() % 100000));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    double avg_us = us / 10000.0;
    
    std::cout << "Average lookup time: " << avg_us << " µs" << std::endl;
    
    // Should be < 10µs per lookup with std::map (O(log n))
    EXPECT_LT(avg_us, 50.0);  // Conservative for CI
}

TEST_F(ParquetSecondaryIndexTest, ThreadSafety) {
    std::atomic<int> errors{0};
    
    // Writer thread
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            index_.add_entry("series_" + std::to_string(i), {0, i});
        }
    });
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            for (int i = 0; i < 10000; ++i) {
                try {
                    index_.lookup("series_" + std::to_string(i % 1000));
                } catch (...) {
                    errors++;
                }
            }
        });
    }
    
    writer.join();
    for (auto& r : readers) r.join();
    
    EXPECT_EQ(errors, 0);
}
```

**Completion Criteria**:
- [ ] All unit tests pass
- [ ] Lookup performance < 50µs for 100K entries
- [ ] Thread safety verified

---

### TASK-BT5: Integrate with Parquet Reader
**File**: `src/tsdb/storage/parquet/reader.cpp` (or equivalent)  
**Effort**: 2 hours  
**Dependencies**: TASK-BT3

**Key Changes**:

1. Add secondary index member to reader
2. Build index on file open
3. Use index in read path

```cpp
// In ParquetReader class
class ParquetReader {
public:
    ParquetReader(const std::string& path) {
        // ... existing initialization ...
        
        // Build secondary index
        secondary_index_ = std::make_shared<ParquetSecondaryIndex>();
        auto result = secondary_index_->build_from_parquet(path);
        if (!result.is_ok()) {
            // Log warning but continue - can fall back to scan
            LOG_WARN("Failed to build secondary index: " + result.error().message());
        }
        
        // Try to load persisted index
        std::string index_path = path + ".sidx";
        if (std::filesystem::exists(index_path)) {
            secondary_index_->load(index_path);
        }
    }
    
    // Optimized point lookup
    core::Result<TimeSeries> read_series(const core::SeriesID& series_id) {
        // Try secondary index first
        if (secondary_index_) {
            auto loc = secondary_index_->lookup(series_id);
            if (loc) {
                return read_at_location(*loc, series_id);
            }
        }
        
        // Fall back to scan (original behavior)
        return read_series_scan(series_id);
    }
    
private:
    core::Result<TimeSeries> read_at_location(
        const RowLocation& loc, 
        const core::SeriesID& series_id) {
        
        // Direct read from specific row group and offset
        auto row_group = reader_->RowGroup(loc.row_group_id);
        // ... read specific row at loc.row_offset ...
    }
    
    std::shared_ptr<ParquetSecondaryIndex> secondary_index_;
};
```

**Completion Criteria**:
- [ ] Index built automatically on reader creation
- [ ] Point lookups use index when available
- [ ] Falls back to scan when index missing
- [ ] No regression in existing functionality

---

### TASK-BT6: Integration Tests
**File**: `test/integration/parquet_secondary_index_integration_test.cpp`  
**Effort**: 1.5 hours  
**Dependencies**: TASK-BT5

```cpp
TEST(ParquetSecondaryIndexIntegration, EndToEndReadPath) {
    // 1. Write data to create Parquet file
    // 2. Open with ParquetReader (should build index)
    // 3. Read by series_id (should use index)
    // 4. Verify correct data returned
}

TEST(ParquetSecondaryIndexIntegration, PerformanceComparison) {
    // 1. Create Parquet file with 100K series
    // 2. Read 1000 random series WITH index
    // 3. Read 1000 random series WITHOUT index (disable it)
    // 4. Compare times - indexed should be 10x+ faster
}

TEST(ParquetSecondaryIndexIntegration, PersistenceAcrossRestarts) {
    // 1. Create Parquet file and index
    // 2. Save index
    // 3. Create new reader (loads index)
    // 4. Verify lookups work without rebuild
}
```

**Completion Criteria**:
- [ ] End-to-end read path works
- [ ] Measurable performance improvement
- [ ] Index persists across restarts

---

### TASK-BT7: Run Baseline Benchmark
**Effort**: 30 minutes  
**Dependencies**: None (do this BEFORE implementation)

```bash
# Record baseline performance BEFORE changes
cd /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb

# Quick benchmark to establish baseline
make benchmark-quick 2>&1 | tee benchmarks/baseline_before_btree.log

# Note key metrics:
# - Read latency (p50, p99)
# - Queries per second
# - Total read time
```

**Completion Criteria**:
- [ ] Baseline metrics recorded in `benchmarks/baseline_before_btree.log`
- [ ] Key read metrics documented

---

### TASK-BT8: Run Post-Implementation Benchmark
**Effort**: 30 minutes  
**Dependencies**: TASK-BT5, TASK-BT6

```bash
# Run benchmark AFTER implementation
make clean-all
make build
make benchmark-quick 2>&1 | tee benchmarks/after_btree.log

# Compare with baseline
echo "=== COMPARISON ===" 
diff benchmarks/baseline_before_btree.log benchmarks/after_btree.log

# Full benchmark if quick looks good
make benchmark-full 2>&1 | tee benchmarks/after_btree_full.log
```

**Completion Criteria**:
- [ ] Read latency improved by **10x or more**
- [ ] No regression in write performance
- [ ] All tests still pass

---

## Phase A Summary

| Task | Description | Effort | Status |
|------|-------------|--------|--------|
| BT1 | Add dependencies | 15 min | ⬜ |
| BT2 | Secondary index header | 1 hour | ⬜ |
| BT3 | Secondary index implementation | 2 hours | ⬜ |
| BT4 | Unit tests | 1.5 hours | ⬜ |
| BT5 | Integrate with Parquet reader | 2 hours | ⬜ |
| BT6 | Integration tests | 1.5 hours | ⬜ |
| BT7 | Run baseline benchmark | 30 min | ⬜ |
| BT8 | Run post-implementation benchmark | 30 min | ⬜ |

**Total Phase A Effort**: ~9 hours (~1 day)

---

## Phase A Verification Checklist

Before proceeding to Phase B (HNSW), verify:

- [ ] `make test-unit` passes
- [ ] `make test-integration` passes  
- [ ] `make benchmark-quick` shows read improvement
- [ ] No regression in write throughput
- [ ] Index memory usage acceptable (< 100MB for 1M series)

---

# PHASE B: HNSWLib Vector Index (After Phase A Verification)

> **PREREQUISITE**: Complete Phase A and verify read improvements before starting Phase B.

## Overview

This phase adds [hnswlib](https://github.com/nmslib/hnswlib) — a header-only C++ library for fast approximate nearest neighbor (ANN) search — for vector similarity queries.

## Background

### Current State Analysis

The existing `VectorIndexImpl` class (`src/tsdb/storage/semantic_vector/quantized_vector_index.cpp`) uses a **stub implementation** called `SimpleHNSWIndex` that performs **linear O(n) scans** instead of true HNSW graph-based search:

```cpp
// Current stub - NOT real HNSW
class SimpleHNSWIndex {
    std::vector<std::pair<core::SeriesID, core::Vector>> data_;  // Linear storage
    
    void add(...) { data_.emplace_back(sid, vec); }  // O(1) append
    
    std::vector<...> search(...) {
        for (const auto& [sid, v] : data_) { ... }  // O(n) scan!
    }
};
```

### Why HNSWLib

| Criteria | HNSWLib | Status |
|----------|---------|--------|
| Language | Header-only C++11 | ✅ Native fit |
| License | Apache 2.0 | ✅ Compatible |
| Algorithm | HNSW (Hierarchical Navigable Small World) | ✅ State-of-art ANN |
| Complexity | O(log n) search | ✅ Scalable |
| Dependencies | None | ✅ Zero friction |
| Persistence | Built-in save/load | ✅ Production-ready |
| Thread Safety | Read-safe, needs write lock | ✅ Matches our pattern |
| Maturity | 5K+ stars, 8.3K dependents | ✅ Battle-tested |

### Config Alignment

Our existing `VectorConfig` maps **directly** to hnswlib parameters:

| Our Config | HNSWLib Parameter | Purpose |
|------------|-------------------|---------|
| `hnsw_max_connections` (16) | `M` | Max connections per node |
| `hnsw_ef_construction` (200) | `ef_construction` | Build-time accuracy |
| `hnsw_ef_search` (50) | `ef` | Search-time accuracy/speed tradeoff |
| `default_vector_dimension` (768) | `dim` | Vector dimensionality |
| `default_metric` ("cosine") | `space` | Distance metric |

---

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SemanticVectorStorageImpl                            │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                     Secondary Index Layer (In-Memory)                 │  │
│  │  ┌─────────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │  │
│  │  │   B+ Tree Index     │  │  Roaring Bitmap  │  │   HNSW Index     │  │  │
│  │  │   (series_id →      │  │  (deletions)     │  │   (vectors)      │  │  │
│  │  │    RowLocation)     │  │                  │  │                  │  │  │
│  │  │                     │  │  ┌────────────┐  │  │  ┌────────────┐  │  │  │
│  │  │  ┌───────────────┐  │  │  │ 0 0 1 0 1  │  │  │  │hnswlib::   │  │  │  │
│  │  │  │ [cpu_0] ──────┼──┼──┼──┼─► RG:0,R:5 │  │  │  │Hierarchic-│  │  │  │
│  │  │  │ [cpu_1] ──────┼──┼──┼──┼─► RG:0,R:12│  │  │  │alNSW<f>   │  │  │  │
│  │  │  │ [mem_0] ──────┼──┼──┼──┼─► RG:1,R:3 │  │  │  └────────────┘  │  │  │
│  │  │  │    ...        │  │  │  │ deleted?   │  │  │                  │  │  │
│  │  │  └───────────────┘  │  │  └────────────┘  │  │  SeriesID ↔      │  │  │
│  │  │                     │  │                  │  │  Label Mapping   │  │  │
│  │  │  O(log n) lookup    │  │  O(1) check      │  │  O(log n) ANN    │  │  │
│  │  │  O(log n + k) range │  │                  │  │                  │  │  │
│  │  └─────────────────────┘  └──────────────────┘  └──────────────────┘  │  │
│  │           │                        │                     │            │  │
│  │           └────────────────────────┼─────────────────────┘            │  │
│  │                                    │                                  │  │
│  │                                    ▼                                  │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │                    IVectorIndex Interface                       │  │  │
│  │  │  ┌───────────────────────────────────────────────────────────┐  │  │  │
│  │  │  │              HNSWLibVectorIndex (NEW)                     │  │  │  │
│  │  │  │  • add_vector()      • search_similar()                   │  │  │  │
│  │  │  │  • update_vector()   • save_index() / load_index()        │  │  │  │
│  │  │  │  • remove_vector()   • get_performance_metrics()          │  │  │  │
│  │  │  └───────────────────────────────────────────────────────────┘  │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                    │                                        │
│                                    ▼                                        │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    Parquet Cold Storage (Immutable)                   │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │  │
│  │  │ Row Group 0  │  │ Row Group 1  │  │ Row Group N  │                 │  │
│  │  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │                 │  │
│  │  │ │series_id │ │  │ │series_id │ │  │ │series_id │ │                 │  │
│  │  │ │timestamp │ │  │ │timestamp │ │  │ │timestamp │ │                 │  │
│  │  │ │value     │ │  │ │value     │ │  │ │value     │ │                 │  │
│  │  │ │labels    │ │  │ │labels    │ │  │ │labels    │ │                 │  │
│  │  │ │embedding │ │  │ │embedding │ │  │ │embedding │ │  ◄── vectors   │  │
│  │  │ └──────────┘ │  │ └──────────┘ │  │ └──────────┘ │      stored    │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘      here      │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘

                              Data Flow
                              ─────────

  Write Path:
  ┌──────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  Client  │───►│ Add to HNSW │───►│ Add to B+   │───►│  Write to   │
  │  write() │    │   Index     │    │  Tree Index │    │  Parquet    │
  └──────────┘    └─────────────┘    └─────────────┘    └─────────────┘

  Read Path (Point Lookup):
  ┌──────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  Client  │───►│ B+ Tree     │───►│ Check       │───►│ Direct Read │
  │  read()  │    │ Lookup O(1) │    │ Deleted?    │    │ from Parquet│
  └──────────┘    └─────────────┘    └─────────────┘    └─────────────┘

  Read Path (Similarity Search):
  ┌──────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  Client  │───►│ HNSW Search │───►│ Filter      │───►│ Return      │
  │ search() │    │ O(log n)    │    │ Deleted     │    │ SeriesIDs   │
  └──────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

### Key Design Decisions

1. **Wrapper Pattern**: Create `HNSWLibVectorIndex` implementing existing `IVectorIndex` interface
2. **Label Mapping**: hnswlib uses `size_t` labels; we need bidirectional mapping to `SeriesID`
3. **Space Selection**: Support cosine, L2, and inner product via hnswlib's space classes
4. **Thread Safety**: Wrap with existing `std::shared_mutex` pattern for concurrent access
5. **Persistence**: Use hnswlib's native `saveIndex()`/`loadIndex()` for durability

---

## Implementation Tasks

### Phase 1: Setup & Vendor (30 min)

#### TASK-HN1: Vendor HNSWLib Header
**File**: `include/third_party/hnswlib/hnswlib.h`  
**Effort**: 15 minutes  
**Dependencies**: None  

**Steps**:
- [ ] Create `include/third_party/hnswlib/` directory
- [ ] Download hnswlib header from https://github.com/nmslib/hnswlib/blob/master/hnswlib/hnswlib.h
- [ ] Also get: `bruteforce.h`, `hnswalg.h`, `space_ip.h`, `space_l2.h`, `visited_list_pool.h`
- [ ] Add license file attribution

**Completion Criteria**:
- [ ] All hnswlib headers present in `include/third_party/hnswlib/`
- [ ] Compiles without errors when included

---

#### TASK-HN2: Update CMakeLists.txt
**File**: `CMakeLists.txt`, `src/tsdb/storage/semantic_vector/CMakeLists.txt`  
**Effort**: 15 minutes  
**Dependencies**: TASK-HN1  

**Steps**:
- [ ] Add `include/third_party` to include paths
- [ ] Ensure C++11 or higher is set (already should be)
- [ ] No library linking needed (header-only)

**Completion Criteria**:
- [ ] `#include "hnswlib/hnswlib.h"` works in source files

---

### Phase 2: Core Implementation (3-4 hours)

#### TASK-HN3: Create HNSWLibVectorIndex Header
**File**: `include/tsdb/storage/semantic_vector/hnswlib_vector_index.h`  
**Effort**: 1 hour  
**Dependencies**: TASK-HN2  

**Contents**:
```cpp
#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_HNSWLIB_VECTOR_INDEX_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_HNSWLIB_VECTOR_INDEX_H_

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <string>

#include "hnswlib/hnswlib.h"
#include "tsdb/storage/semantic_vector_architecture.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/semantic_vector_types.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Real HNSW implementation using hnswlib
 * 
 * Replaces SimpleHNSWIndex stub with actual O(log n) ANN search.
 * Implements IVectorIndex interface for seamless integration.
 */
class HNSWLibVectorIndex : public IVectorIndex {
public:
    explicit HNSWLibVectorIndex(
        const core::semantic_vector::SemanticVectorConfig::VectorConfig& config);
    
    ~HNSWLibVectorIndex() override;

    // IVectorIndex implementation
    core::Result<void> add_vector(
        const core::SeriesID& series_id, 
        const core::Vector& vector) override;
    
    core::Result<void> update_vector(
        const core::SeriesID& series_id, 
        const core::Vector& vector) override;
    
    core::Result<void> remove_vector(
        const core::SeriesID& series_id) override;
    
    core::Result<core::Vector> get_vector(
        const core::SeriesID& series_id) const override;
    
    core::Result<std::vector<std::pair<core::SeriesID, double>>> search_similar(
        const core::Vector& query_vector,
        size_t k_nearest,
        double similarity_threshold = 0.7) const override;

    // Quantized/Binary operations (delegate to base or stub)
    core::Result<core::QuantizedVector> quantize_vector(
        const core::Vector& vector) const override;
    core::Result<core::Vector> dequantize_vector(
        const core::QuantizedVector& qvector) const override;
    core::Result<std::vector<std::pair<core::SeriesID, double>>> search_quantized(
        const core::QuantizedVector& query_vector,
        size_t k_nearest) const override;
    core::Result<core::BinaryVector> binarize_vector(
        const core::Vector& vector) const override;
    core::Result<std::vector<std::pair<core::SeriesID, uint32_t>>> search_binary(
        const core::BinaryVector& query_vector,
        size_t k_nearest,
        uint32_t max_hamming_distance = 10) const override;

    // Index management
    core::Result<void> build_index() override;
    core::Result<void> optimize_index() override;
    core::Result<core::VectorIndex> get_index_stats() const override;

    // Performance monitoring
    core::Result<core::PerformanceMetrics> get_performance_metrics() const override;
    core::Result<void> reset_performance_metrics() override;

    // Configuration
    void update_config(
        const core::semantic_vector::SemanticVectorConfig::VectorConfig& config) override;
    core::semantic_vector::SemanticVectorConfig::VectorConfig get_config() const override;

    // Persistence (hnswlib native)
    core::Result<void> save_index(const std::string& path);
    core::Result<void> load_index(const std::string& path);

    // Lifecycle
    void close();

private:
    // Configuration
    core::semantic_vector::SemanticVectorConfig::VectorConfig config_;
    
    // HNSWLib index
    std::unique_ptr<hnswlib::SpaceInterface<float>> space_;
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> hnsw_index_;
    
    // SeriesID ↔ hnswlib label mapping
    mutable std::shared_mutex mutex_;
    std::unordered_map<core::SeriesID, size_t> series_to_label_;
    std::unordered_map<size_t, core::SeriesID> label_to_series_;
    std::atomic<size_t> next_label_{0};
    
    // Raw vector storage (for get_vector retrieval)
    std::unordered_map<core::SeriesID, core::Vector> raw_vectors_;
    
    // Performance tracking
    struct Metrics {
        std::atomic<size_t> total_searches{0};
        std::atomic<size_t> total_adds{0};
        std::atomic<double> total_search_time_ms{0.0};
        std::atomic<size_t> vectors_stored{0};
    } metrics_;

    // Helpers
    hnswlib::SpaceInterface<float>* create_space(const std::string& metric, size_t dim);
    size_t get_or_create_label(const core::SeriesID& series_id);
    std::optional<size_t> get_label(const core::SeriesID& series_id) const;
};

}  // namespace semantic_vector
}  // namespace storage
}  // namespace tsdb

#endif  // TSDB_STORAGE_SEMANTIC_VECTOR_HNSWLIB_VECTOR_INDEX_H_
```

**Completion Criteria**:
- [ ] Header compiles without errors
- [ ] All IVectorIndex methods declared
- [ ] Thread-safe data structures in place

---

#### TASK-HN4: Implement HNSWLibVectorIndex Core Methods
**File**: `src/tsdb/storage/semantic_vector/hnswlib_vector_index.cpp`  
**Effort**: 2-3 hours  
**Dependencies**: TASK-HN3  

**Key Implementations**:

```cpp
// Constructor - Initialize space and index
HNSWLibVectorIndex::HNSWLibVectorIndex(const VectorConfig& config) 
    : config_(config) {
    
    size_t dim = config_.default_vector_dimension;
    size_t max_elements = 1000000;  // Initial capacity, can grow
    size_t M = config_.hnsw_max_connections;
    size_t ef_construction = config_.hnsw_ef_construction;
    
    // Create appropriate space based on metric
    space_.reset(create_space(config_.default_metric, dim));
    
    // Initialize HNSW index
    hnsw_index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
        space_.get(), max_elements, M, ef_construction);
    
    // Set search ef parameter
    hnsw_index_->setEf(config_.hnsw_ef_search);
}

// Space factory
hnswlib::SpaceInterface<float>* HNSWLibVectorIndex::create_space(
    const std::string& metric, size_t dim) {
    
    if (metric == "cosine" || metric == "ip") {
        return new hnswlib::InnerProductSpace(dim);
    } else if (metric == "l2" || metric == "euclidean") {
        return new hnswlib::L2Space(dim);
    }
    // Default to L2
    return new hnswlib::L2Space(dim);
}

// Add vector - O(log n) insertion
core::Result<void> HNSWLibVectorIndex::add_vector(
    const core::SeriesID& series_id, 
    const core::Vector& vector) {
    
    if (vector.data.size() != config_.default_vector_dimension) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::VALIDATION_ERROR,
            "Vector dimension mismatch"));
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Get or create label for this series
    size_t label = get_or_create_label(series_id);
    
    // Normalize for cosine similarity if needed
    std::vector<float> data = vector.data;
    if (config_.normalize_vectors && config_.default_metric == "cosine") {
        float norm = 0.0f;
        for (float v : data) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-6f) {
            for (float& v : data) v /= norm;
        }
    }
    
    // Add to hnswlib index
    hnsw_index_->addPoint(data.data(), label);
    
    // Store raw vector for retrieval
    raw_vectors_[series_id] = vector;
    
    metrics_.total_adds++;
    metrics_.vectors_stored++;
    
    return core::Result<void>::ok();
}

// Search - O(log n) approximate nearest neighbors
core::Result<std::vector<std::pair<core::SeriesID, double>>> 
HNSWLibVectorIndex::search_similar(
    const core::Vector& query_vector,
    size_t k_nearest,
    double similarity_threshold) const {
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (hnsw_index_->cur_element_count == 0) {
        return core::Result<std::vector<std::pair<core::SeriesID, double>>>::ok({});
    }
    
    // Normalize query if needed
    std::vector<float> query = query_vector.data;
    if (config_.normalize_vectors && config_.default_metric == "cosine") {
        float norm = 0.0f;
        for (float v : query) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-6f) {
            for (float& v : query) v /= norm;
        }
    }
    
    // Search
    auto results = hnsw_index_->searchKnn(query.data(), k_nearest);
    
    // Convert to our format
    std::vector<std::pair<core::SeriesID, double>> output;
    output.reserve(results.size());
    
    while (!results.empty()) {
        auto [dist, label] = results.top();
        results.pop();
        
        auto it = label_to_series_.find(label);
        if (it != label_to_series_.end()) {
            // Convert distance to similarity
            double similarity = (config_.default_metric == "cosine") 
                ? (1.0 - dist)  // Inner product space returns 1 - cosine
                : (1.0 / (1.0 + dist));  // L2 distance to similarity
            
            if (similarity >= similarity_threshold) {
                output.emplace_back(it->second, similarity);
            }
        }
    }
    
    // Results come in reverse order from priority queue
    std::reverse(output.begin(), output.end());
    
    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    
    const_cast<Metrics&>(metrics_).total_searches++;
    const_cast<Metrics&>(metrics_).total_search_time_ms.fetch_add(latency);
    
    return core::Result<std::vector<std::pair<core::SeriesID, double>>>::ok(output);
}

// Persistence
core::Result<void> HNSWLibVectorIndex::save_index(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    try {
        hnsw_index_->saveIndex(path);
        // TODO: Also persist series_to_label_ and label_to_series_ mappings
        return core::Result<void>::ok();
    } catch (const std::exception& e) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::IO_ERROR,
            std::string("Failed to save index: ") + e.what()));
    }
}

core::Result<void> HNSWLibVectorIndex::load_index(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    try {
        hnsw_index_->loadIndex(path, space_.get());
        // TODO: Also load series_to_label_ and label_to_series_ mappings
        return core::Result<void>::ok();
    } catch (const std::exception& e) {
        return core::Result<void>::error(core::Error(
            core::ErrorCode::IO_ERROR,
            std::string("Failed to load index: ") + e.what()));
    }
}
```

**Completion Criteria**:
- [ ] Constructor initializes hnswlib with correct parameters
- [ ] `add_vector()` inserts vectors with proper label mapping
- [ ] `search_similar()` returns results in O(log n) time
- [ ] `save_index()`/`load_index()` persist and restore index
- [ ] All methods handle errors gracefully

---

#### TASK-HN5: Implement Label Mapping Persistence
**File**: `src/tsdb/storage/semantic_vector/hnswlib_vector_index.cpp`  
**Effort**: 30 minutes  
**Dependencies**: TASK-HN4  

**Purpose**: hnswlib persists vectors by integer labels, but we need to restore `SeriesID ↔ label` mapping.

**Implementation**:
```cpp
// Serialize mapping alongside index
core::Result<void> HNSWLibVectorIndex::save_index(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Save hnswlib index
    hnsw_index_->saveIndex(path);
    
    // Save label mapping as separate file
    std::string mapping_path = path + ".mapping";
    std::ofstream ofs(mapping_path, std::ios::binary);
    
    size_t count = series_to_label_.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& [series_id, label] : series_to_label_) {
        size_t id_len = series_id.size();
        ofs.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        ofs.write(series_id.data(), id_len);
        ofs.write(reinterpret_cast<const char*>(&label), sizeof(label));
    }
    
    return core::Result<void>::ok();
}

core::Result<void> HNSWLibVectorIndex::load_index(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Load hnswlib index
    hnsw_index_->loadIndex(path, space_.get());
    
    // Load label mapping
    std::string mapping_path = path + ".mapping";
    std::ifstream ifs(mapping_path, std::ios::binary);
    
    size_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    series_to_label_.clear();
    label_to_series_.clear();
    
    for (size_t i = 0; i < count; ++i) {
        size_t id_len;
        ifs.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        
        std::string series_id(id_len, '\0');
        ifs.read(series_id.data(), id_len);
        
        size_t label;
        ifs.read(reinterpret_cast<char*>(&label), sizeof(label));
        
        series_to_label_[series_id] = label;
        label_to_series_[label] = series_id;
        next_label_ = std::max(next_label_.load(), label + 1);
    }
    
    return core::Result<void>::ok();
}
```

**Completion Criteria**:
- [ ] Mapping saved to `<path>.mapping` file
- [ ] Mapping restored correctly on load
- [ ] `next_label_` counter restored to avoid collisions

---

### Phase 3: Integration (1-2 hours)

#### TASK-HN6: Wire HNSWLibVectorIndex into VectorIndexImpl
**File**: `src/tsdb/storage/semantic_vector/quantized_vector_index.cpp`  
**Effort**: 30 minutes  
**Dependencies**: TASK-HN4  

**Option A**: Replace `SimpleHNSWIndex` with `HNSWLibVectorIndex`

```cpp
// In VectorIndexImpl constructor
VectorIndexImpl::VectorIndexImpl(const VectorConfig& config) : config_(config) {
    // Replace stub with real hnswlib implementation
    if (config_.default_index_type == VectorIndex::IndexType::HNSW) {
        hnsw_index_ = std::make_unique<HNSWLibVectorIndex>(config_);
    }
    // ... rest unchanged
}
```

**Option B**: Make `HNSWLibVectorIndex` the primary implementation, deprecate `VectorIndexImpl`

**Recommendation**: Option A for minimal disruption, migrate to Option B later.

**Completion Criteria**:
- [ ] `VectorIndexImpl` uses `HNSWLibVectorIndex` when HNSW index type selected
- [ ] Existing tests pass
- [ ] Search performance improved from O(n) to O(log n)

---

#### TASK-HN7: Update InterfaceFactory
**File**: `include/tsdb/storage/semantic_vector_architecture.h` or separate factory  
**Effort**: 30 minutes  
**Dependencies**: TASK-HN6  

**Purpose**: Allow creating `HNSWLibVectorIndex` through factory pattern.

```cpp
class InterfaceFactory {
public:
    static Result<std::shared_ptr<IVectorIndex>> create_vector_index(
        const SemanticVectorConfig& config) {
        
        auto& vec_config = config.vector_config;
        
        if (vec_config.default_index_type == VectorIndex::IndexType::HNSW) {
            return Result<std::shared_ptr<IVectorIndex>>::ok(
                std::make_shared<HNSWLibVectorIndex>(vec_config));
        }
        
        // Fallback to existing impl for other index types
        return Result<std::shared_ptr<IVectorIndex>>::ok(
            std::make_shared<VectorIndexImpl>(vec_config));
    }
};
```

**Completion Criteria**:
- [ ] Factory creates `HNSWLibVectorIndex` for HNSW type
- [ ] Existing code paths unchanged for other index types

---

#### TASK-HN8: Add Persistence Integration
**File**: `src/tsdb/storage/semantic_vector_storage_impl.cpp`  
**Effort**: 30 minutes  
**Dependencies**: TASK-HN5, TASK-HN6  

**Purpose**: Save/load HNSW index during storage lifecycle.

```cpp
// In SemanticVectorStorageImpl
core::Result<void> SemanticVectorStorageImpl::persist_indices() {
    if (auto* hnsw = dynamic_cast<HNSWLibVectorIndex*>(vector_index_.get())) {
        std::string path = storage_config_.data_directory + "/hnsw_index.bin";
        return hnsw->save_index(path);
    }
    return core::Result<void>::ok();
}

core::Result<void> SemanticVectorStorageImpl::load_indices() {
    if (auto* hnsw = dynamic_cast<HNSWLibVectorIndex*>(vector_index_.get())) {
        std::string path = storage_config_.data_directory + "/hnsw_index.bin";
        if (std::filesystem::exists(path)) {
            return hnsw->load_index(path);
        }
    }
    return core::Result<void>::ok();
}
```

**Completion Criteria**:
- [ ] Index persisted on shutdown/checkpoint
- [ ] Index loaded on startup
- [ ] Graceful handling of missing index files

---

### Phase 4: Testing (2-3 hours)

#### TASK-HN9: Unit Tests for HNSWLibVectorIndex
**File**: `test/unit/semantic_vector/hnswlib_vector_index_test.cpp`  
**Effort**: 1.5 hours  
**Dependencies**: TASK-HN4  

**Test Cases**:
```cpp
TEST(HNSWLibVectorIndexTest, AddAndRetrieve) {
    VectorConfig config;
    config.default_vector_dimension = 128;
    config.hnsw_max_connections = 16;
    config.hnsw_ef_construction = 100;
    config.hnsw_ef_search = 50;
    
    HNSWLibVectorIndex index(config);
    
    core::Vector vec(128);
    vec.data.resize(128);
    std::fill(vec.data.begin(), vec.data.end(), 1.0f);
    
    ASSERT_TRUE(index.add_vector("series_1", vec).is_ok());
    
    auto result = index.get_vector("series_1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().data.size(), 128);
}

TEST(HNSWLibVectorIndexTest, SimilaritySearch) {
    VectorConfig config;
    config.default_vector_dimension = 128;
    config.default_metric = "cosine";
    
    HNSWLibVectorIndex index(config);
    
    // Add 1000 random vectors
    for (int i = 0; i < 1000; ++i) {
        core::Vector vec(128);
        vec.data.resize(128);
        for (auto& v : vec.data) v = static_cast<float>(rand()) / RAND_MAX;
        index.add_vector("series_" + std::to_string(i), vec);
    }
    
    // Query
    core::Vector query(128);
    query.data.resize(128);
    std::fill(query.data.begin(), query.data.end(), 0.5f);
    
    auto results = index.search_similar(query, 10, 0.0);
    ASSERT_TRUE(results.is_ok());
    EXPECT_EQ(results.value().size(), 10);
}

TEST(HNSWLibVectorIndexTest, Persistence) {
    VectorConfig config;
    config.default_vector_dimension = 64;
    
    // Create and populate index
    {
        HNSWLibVectorIndex index(config);
        core::Vector vec(64);
        vec.data.resize(64, 1.0f);
        index.add_vector("test_series", vec);
        index.save_index("/tmp/test_hnsw");
    }
    
    // Load and verify
    {
        HNSWLibVectorIndex index(config);
        index.load_index("/tmp/test_hnsw");
        
        auto result = index.search_similar(core::Vector(64), 1, 0.0);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().size(), 1);
    }
}

TEST(HNSWLibVectorIndexTest, PerformanceBenchmark) {
    VectorConfig config;
    config.default_vector_dimension = 768;  // BERT dimension
    config.hnsw_max_connections = 16;
    config.hnsw_ef_construction = 200;
    config.hnsw_ef_search = 50;
    
    HNSWLibVectorIndex index(config);
    
    // Add 10K vectors
    for (int i = 0; i < 10000; ++i) {
        core::Vector vec(768);
        vec.data.resize(768);
        for (auto& v : vec.data) v = static_cast<float>(rand()) / RAND_MAX;
        index.add_vector("series_" + std::to_string(i), vec);
    }
    
    // Benchmark search
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        core::Vector query(768);
        query.data.resize(768);
        for (auto& v : query.data) v = static_cast<float>(rand()) / RAND_MAX;
        index.search_similar(query, 10, 0.0);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Should be < 1ms per query on average
    EXPECT_LT(ms / 1000.0, 5.0);  // Allow 5ms for CI variance
    
    std::cout << "Average search latency: " << (ms / 1000.0) << " ms" << std::endl;
}
```

**Completion Criteria**:
- [ ] All unit tests pass
- [ ] Search latency < 5ms for 10K vectors (768-dim)
- [ ] Persistence round-trip verified

---

#### TASK-HN10: Integration Tests
**File**: `test/integration/semantic_vector_hnswlib_test.cpp`  
**Effort**: 1 hour  
**Dependencies**: TASK-HN6  

**Test Cases**:
- [ ] End-to-end: `write_with_vector()` → `vector_similarity_search()`
- [ ] Concurrent access: multiple readers + single writer
- [ ] Recovery: kill process, restart, verify index intact
- [ ] Scale test: 100K vectors, verify sub-10ms search

**Completion Criteria**:
- [ ] All integration tests pass
- [ ] No data corruption under concurrent access

---

### Phase 5: Documentation & Cleanup (30 min)

#### TASK-HN11: Update Documentation
**Files**: Various docs  
**Effort**: 30 minutes  
**Dependencies**: All above  

**Updates**:
- [ ] Add hnswlib to `README.md` dependencies section
- [ ] Update `SEMANTIC_VECTOR_IMPLEMENTATION_TASKS.md` status
- [ ] Add usage examples to `docs/examples/`
- [ ] Document tuning parameters (M, ef_construction, ef_search)

---

## Summary

| Phase | Tasks | Effort | Cumulative |
|-------|-------|--------|------------|
| 1. Setup | HN1-HN2 | 30 min | 30 min |
| 2. Core Implementation | HN3-HN5 | 3.5 hours | 4 hours |
| 3. Integration | HN6-HN8 | 1.5 hours | 5.5 hours |
| 4. Testing | HN9-HN10 | 2.5 hours | 8 hours |
| 5. Documentation | HN11 | 30 min | **8.5 hours** |

**Total Estimated Effort**: ~1-2 days (including buffer for debugging)

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| hnswlib API changes | Low | Medium | Pin to specific release |
| Memory usage higher than expected | Medium | Medium | Add capacity limits, monitor |
| Thread contention under heavy load | Medium | Low | Benchmark, consider sharding |
| Persistence file corruption | Low | High | Add checksums, backup strategy |

---

## Success Criteria

1. **Performance**: Search latency < 1ms for 100K vectors (768-dim)
2. **Accuracy**: Recall@10 > 95% vs brute-force
3. **Scalability**: Handle 1M+ vectors with < 2GB memory
4. **Reliability**: Zero data loss across restarts
5. **Integration**: All existing tests pass unchanged

---

---

## Phase 6: Parquet Secondary Index (Optional Enhancement)

### Background: Lance-Style Random Access for Parquet

Lance format achieves ~100x faster random access than Parquet by maintaining **in-memory secondary indices**. We can add similar capabilities to our Parquet cold storage without changing the file format.

### Data Structures Used by Lance

| Structure | Purpose | Our Equivalent |
|-----------|---------|----------------|
| B+ Tree Manifest | row_id → (fragment, offset) | `SeriesIndex` |
| Roaring Bitmap | Deletion tracking | `DeletionBitmap` |
| Row Group Cache | Metadata for skip-scan | `RowGroupCache` |
| Vector Index | ANN search | `HNSWLibVectorIndex` (Phase 1-5) |

---

### TASK-HN12: Parquet Secondary Index Header
**File**: `include/tsdb/storage/parquet/secondary_index.h`  
**Effort**: 1.5 hours  
**Dependencies**: None (can be done in parallel with HNSW work)

**Implementation**:
```cpp
#ifndef TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_
#define TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <optional>

// Consider using: https://github.com/tlx/tlx (BSD license) for B+ tree
// Or: https://github.com/google/cpp-btree (Apache 2.0)
// Or: roll our own for learning/control

#include "roaring/roaring.hh"  // CRoaring library
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {
namespace parquet {

/**
 * @brief Physical location of a row in Parquet file(s)
 */
struct RowLocation {
    int32_t file_index;           // Which Parquet file (for partitioned data)
    int32_t row_group_id;         // Row group within file
    int64_t row_offset_in_group;  // Offset within row group
    
    bool operator==(const RowLocation& other) const {
        return file_index == other.file_index &&
               row_group_id == other.row_group_id &&
               row_offset_in_group == other.row_offset_in_group;
    }
};

/**
 * @brief B+ Tree node for range-query-friendly indexing
 * 
 * Properties:
 * - All values in leaves (internal nodes only have keys)
 * - Leaves are linked for efficient range scans
 * - Branching factor B typically 64-256 for cache efficiency
 * - Height = O(log_B(n)), typically 2-4 for millions of entries
 */
template<typename K, typename V, size_t B = 128>
class BPlusTree {
public:
    BPlusTree() : root_(nullptr), size_(0) {}
    ~BPlusTree() { clear(); }
    
    // Core operations
    void insert(const K& key, const V& value);
    bool remove(const K& key);
    std::optional<V> find(const K& key) const;
    bool contains(const K& key) const;
    
    // Range operations (B+ tree strength)
    std::vector<std::pair<K, V>> range(const K& start, const K& end) const;
    std::vector<std::pair<K, V>> prefix_scan(const K& prefix) const;  // For string keys
    
    // Iteration (via leaf links)
    class Iterator;
    Iterator begin() const;
    Iterator end() const;
    Iterator lower_bound(const K& key) const;
    Iterator upper_bound(const K& key) const;
    
    // Bulk operations
    void bulk_load(std::vector<std::pair<K, V>>& sorted_data);  // O(n) construction
    
    // Stats
    size_t size() const { return size_; }
    size_t height() const;
    size_t memory_usage() const;
    
    // Persistence
    void serialize(std::ostream& out) const;
    void deserialize(std::istream& in);
    
    void clear();

private:
    struct Node {
        bool is_leaf;
        std::vector<K> keys;
        
        // For internal nodes: child pointers
        std::vector<Node*> children;
        
        // For leaf nodes: values and sibling link
        std::vector<V> values;
        Node* next_leaf;  // Linked list of leaves for range scans
        
        Node(bool leaf) : is_leaf(leaf), next_leaf(nullptr) {}
    };
    
    Node* root_;
    size_t size_;
    
    // B+ tree operations
    Node* find_leaf(const K& key) const;
    void split_child(Node* parent, size_t child_index);
    void insert_non_full(Node* node, const K& key, const V& value);
    bool remove_from_node(Node* node, const K& key);
    void rebalance_after_remove(Node* node, size_t child_index);
};

/**
 * @brief Row group metadata cache for predicate pushdown
 */
struct RowGroupMetadata {
    int32_t row_group_id;
    int64_t start_row;           // Global row number
    int64_t num_rows;
    int64_t file_offset;         // Byte offset in file
    int64_t compressed_size;
    int64_t uncompressed_size;
    
    // Column statistics for predicate pushdown
    struct ColumnStats {
        std::string column_name;
        bool has_null;
        int64_t null_count;
        std::string min_value;   // Serialized min
        std::string max_value;   // Serialized max
        int64_t distinct_count;  // Optional
    };
    std::map<std::string, ColumnStats> column_stats;
    
    // Check if row group can be skipped based on predicate
    bool can_skip(const std::string& column, 
                  const std::string& op,  // "=", "<", ">", ">=", "<=", "!=" 
                  const std::string& value) const;
};

/**
 * @brief Secondary index for fast Parquet random access
 * 
 * Provides Lance-style O(1) lookups on immutable Parquet files by
 * maintaining in-memory index structures that are rebuilt on load.
 */
class ParquetSecondaryIndex {
public:
    ParquetSecondaryIndex();
    ~ParquetSecondaryIndex();
    
    // ========================================================================
    // INDEX BUILDING
    // ========================================================================
    
    /**
     * @brief Build index from Parquet file(s)
     * 
     * Scans Parquet metadata and optionally data to build:
     * - series_id → RowLocation B+ tree
     * - Row group metadata cache
     * - Timestamp range index (optional)
     * 
     * @param parquet_paths List of Parquet files to index
     * @param index_columns Which columns to build indices for
     */
    core::Result<void> build_from_parquet(
        const std::vector<std::string>& parquet_paths,
        const std::vector<std::string>& index_columns = {"series_id"});
    
    /**
     * @brief Incrementally add new Parquet file to index
     */
    core::Result<void> add_parquet_file(
        const std::string& parquet_path,
        int32_t file_index);
    
    // ========================================================================
    // POINT LOOKUPS - O(log n) with B+ tree, O(1) with hash
    // ========================================================================
    
    /**
     * @brief Find row location for a series_id
     * 
     * @return RowLocation if found, nullopt if not exists or deleted
     */
    std::optional<RowLocation> lookup(const core::SeriesID& series_id) const;
    
    /**
     * @brief Check if series exists (not deleted)
     */
    bool exists(const core::SeriesID& series_id) const;
    
    /**
     * @brief Batch lookup for multiple series
     */
    std::vector<std::pair<core::SeriesID, std::optional<RowLocation>>> 
    batch_lookup(const std::vector<core::SeriesID>& series_ids) const;
    
    // ========================================================================
    // RANGE QUERIES - B+ tree advantage over hash map
    // ========================================================================
    
    /**
     * @brief Find all series with IDs in range [start, end]
     * 
     * Efficient with B+ tree leaf links: O(log n + k) where k = result size
     */
    std::vector<std::pair<core::SeriesID, RowLocation>> 
    range_lookup(const core::SeriesID& start, const core::SeriesID& end) const;
    
    /**
     * @brief Find series with ID prefix (e.g., "cpu_" matches "cpu_0", "cpu_1", ...)
     */
    std::vector<std::pair<core::SeriesID, RowLocation>>
    prefix_lookup(const std::string& prefix) const;
    
    // ========================================================================
    // DELETION TRACKING - Roaring bitmap for memory efficiency
    // ========================================================================
    
    /**
     * @brief Mark row as deleted (soft delete)
     * 
     * Does NOT rewrite Parquet file - just marks in bitmap.
     * Compaction will physically remove deleted rows later.
     */
    void mark_deleted(int64_t global_row_id);
    void mark_deleted(const core::SeriesID& series_id);
    
    /**
     * @brief Check if row is deleted
     */
    bool is_deleted(int64_t global_row_id) const;
    bool is_deleted(const core::SeriesID& series_id) const;
    
    /**
     * @brief Get count of deleted rows (for compaction decisions)
     */
    size_t deleted_count() const;
    
    /**
     * @brief Get deletion ratio (deleted / total)
     */
    double deletion_ratio() const;
    
    // ========================================================================
    // ROW GROUP OPTIMIZATION
    // ========================================================================
    
    /**
     * @brief Get row groups that might contain data for a predicate
     * 
     * Uses column statistics for predicate pushdown.
     */
    std::vector<int32_t> get_relevant_row_groups(
        const std::string& column,
        const std::string& op,
        const std::string& value) const;
    
    /**
     * @brief Get row group metadata
     */
    const RowGroupMetadata* get_row_group_metadata(int32_t row_group_id) const;
    
    // ========================================================================
    // PERSISTENCE
    // ========================================================================
    
    /**
     * @brief Save index to disk
     * 
     * Format: [header][btree_data][bitmap_data][metadata]
     */
    core::Result<void> save(const std::string& path);
    
    /**
     * @brief Load index from disk
     */
    core::Result<void> load(const std::string& path);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    struct Stats {
        size_t total_series;
        size_t total_rows;
        size_t deleted_rows;
        size_t num_row_groups;
        size_t num_files;
        size_t index_memory_bytes;
        size_t btree_height;
        double avg_lookup_ns;  // From sampling
    };
    
    Stats get_stats() const;

private:
    mutable std::shared_mutex mutex_;
    
    // Primary index: series_id → location
    // Using B+ tree for range query support
    BPlusTree<core::SeriesID, RowLocation, 128> series_index_;
    
    // Alternative: hash map for pure point lookups (faster but no range)
    // std::unordered_map<core::SeriesID, RowLocation> series_hash_;
    
    // Deletion tracking with roaring bitmap (memory efficient)
    roaring::Roaring64Map deletion_bitmap_;
    
    // Row group metadata for predicate pushdown
    std::vector<RowGroupMetadata> row_groups_;
    
    // File tracking
    std::vector<std::string> parquet_files_;
    
    // Global row counter
    int64_t total_rows_{0};
};

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb

#endif  // TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H_
```

**Completion Criteria**:
- [ ] Header compiles without errors
- [ ] B+ tree template defined with all operations
- [ ] `ParquetSecondaryIndex` API complete

---

### TASK-HN13: B+ Tree Implementation
**File**: `src/tsdb/storage/parquet/bplus_tree.hpp`  
**Effort**: 3-4 hours (or use existing library)  
**Dependencies**: TASK-HN12

**Option A: Use Existing Library (Recommended)**

```cpp
// Use Google's cpp-btree (Apache 2.0)
// https://github.com/google/cpp-btree
#include "btree/btree_map.h"

// Or TLX B+ tree (BSD)
// https://github.com/tlx/tlx
#include "tlx/container/btree_map.hpp"

using SeriesIndex = btree::btree_map<core::SeriesID, RowLocation>;
```

**Option B: Custom Implementation (for learning/control)**

```cpp
template<typename K, typename V, size_t B>
void BPlusTree<K, V, B>::insert(const K& key, const V& value) {
    if (!root_) {
        root_ = new Node(true);  // Start with leaf
        root_->keys.push_back(key);
        root_->values.push_back(value);
        size_ = 1;
        return;
    }
    
    // If root is full, split it
    if (root_->keys.size() >= 2 * B - 1) {
        Node* new_root = new Node(false);
        new_root->children.push_back(root_);
        split_child(new_root, 0);
        root_ = new_root;
    }
    
    insert_non_full(root_, key, value);
    size_++;
}

template<typename K, typename V, size_t B>
void BPlusTree<K, V, B>::split_child(Node* parent, size_t child_index) {
    Node* child = parent->children[child_index];
    Node* new_node = new Node(child->is_leaf);
    
    size_t mid = B - 1;
    K mid_key = child->keys[mid];
    
    // Move upper half to new node
    new_node->keys.assign(child->keys.begin() + mid + 1, child->keys.end());
    child->keys.resize(mid);
    
    if (child->is_leaf) {
        new_node->values.assign(child->values.begin() + mid + 1, child->values.end());
        child->values.resize(mid + 1);  // Keep mid in left leaf
        
        // Maintain leaf links
        new_node->next_leaf = child->next_leaf;
        child->next_leaf = new_node;
    } else {
        new_node->children.assign(child->children.begin() + mid + 1, child->children.end());
        child->children.resize(mid + 1);
    }
    
    // Insert mid_key and new_node into parent
    parent->keys.insert(parent->keys.begin() + child_index, mid_key);
    parent->children.insert(parent->children.begin() + child_index + 1, new_node);
}

template<typename K, typename V, size_t B>
std::optional<V> BPlusTree<K, V, B>::find(const K& key) const {
    Node* leaf = find_leaf(key);
    if (!leaf) return std::nullopt;
    
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it != leaf->keys.end() && *it == key) {
        size_t index = it - leaf->keys.begin();
        return leaf->values[index];
    }
    return std::nullopt;
}

template<typename K, typename V, size_t B>
typename BPlusTree<K, V, B>::Node* BPlusTree<K, V, B>::find_leaf(const K& key) const {
    if (!root_) return nullptr;
    
    Node* node = root_;
    while (!node->is_leaf) {
        auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
        size_t index = it - node->keys.begin();
        node = node->children[index];
    }
    return node;
}

// Range scan using leaf links - O(log n + k)
template<typename K, typename V, size_t B>
std::vector<std::pair<K, V>> BPlusTree<K, V, B>::range(const K& start, const K& end) const {
    std::vector<std::pair<K, V>> results;
    
    Node* leaf = find_leaf(start);
    if (!leaf) return results;
    
    // Find start position in leaf
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), start);
    size_t index = it - leaf->keys.begin();
    
    // Scan through leaves using next_leaf links
    while (leaf) {
        for (; index < leaf->keys.size(); ++index) {
            if (leaf->keys[index] > end) {
                return results;  // Past end of range
            }
            results.emplace_back(leaf->keys[index], leaf->values[index]);
        }
        leaf = leaf->next_leaf;
        index = 0;
    }
    
    return results;
}

// Bulk load - O(n) construction from sorted data
template<typename K, typename V, size_t B>
void BPlusTree<K, V, B>::bulk_load(std::vector<std::pair<K, V>>& sorted_data) {
    clear();
    if (sorted_data.empty()) return;
    
    // Build leaves first
    std::vector<Node*> leaves;
    Node* current_leaf = new Node(true);
    
    for (const auto& [key, value] : sorted_data) {
        if (current_leaf->keys.size() >= B) {
            leaves.push_back(current_leaf);
            Node* new_leaf = new Node(true);
            current_leaf->next_leaf = new_leaf;
            current_leaf = new_leaf;
        }
        current_leaf->keys.push_back(key);
        current_leaf->values.push_back(value);
    }
    leaves.push_back(current_leaf);
    
    // Build internal nodes bottom-up
    std::vector<Node*> level = leaves;
    while (level.size() > 1) {
        std::vector<Node*> next_level;
        Node* parent = new Node(false);
        
        for (size_t i = 0; i < level.size(); ++i) {
            if (parent->children.size() >= B) {
                next_level.push_back(parent);
                parent = new Node(false);
            }
            parent->children.push_back(level[i]);
            if (parent->children.size() > 1) {
                // Use first key of child as separator
                parent->keys.push_back(level[i]->keys[0]);
            }
        }
        next_level.push_back(parent);
        level = next_level;
    }
    
    root_ = level[0];
    size_ = sorted_data.size();
}
```

**Completion Criteria**:
- [ ] B+ tree insert/find/range operations working
- [ ] Leaf linking for efficient range scans
- [ ] Bulk load for O(n) index construction
- [ ] Memory-efficient with configurable branching factor

---

### TASK-HN14: Secondary Index Implementation
**File**: `src/tsdb/storage/parquet/secondary_index.cpp`  
**Effort**: 2 hours  
**Dependencies**: TASK-HN12, TASK-HN13

```cpp
#include "tsdb/storage/parquet/secondary_index.h"
#include <parquet/api/reader.h>
#include <fstream>

namespace tsdb {
namespace storage {
namespace parquet {

core::Result<void> ParquetSecondaryIndex::build_from_parquet(
    const std::vector<std::string>& parquet_paths,
    const std::vector<std::string>& index_columns) {
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Collect all (series_id, location) pairs for bulk load
    std::vector<std::pair<core::SeriesID, RowLocation>> entries;
    entries.reserve(1000000);  // Pre-allocate for large files
    
    for (int32_t file_idx = 0; file_idx < parquet_paths.size(); ++file_idx) {
        const auto& path = parquet_paths[file_idx];
        parquet_files_.push_back(path);
        
        // Open Parquet file
        auto reader = ::parquet::ParquetFileReader::OpenFile(path);
        auto metadata = reader->metadata();
        
        int64_t global_row_offset = total_rows_;
        
        // Process each row group
        for (int rg = 0; rg < metadata->num_row_groups(); ++rg) {
            auto rg_metadata = metadata->RowGroup(rg);
            
            // Cache row group metadata
            RowGroupMetadata rg_meta;
            rg_meta.row_group_id = row_groups_.size();
            rg_meta.start_row = global_row_offset;
            rg_meta.num_rows = rg_metadata->num_rows();
            rg_meta.compressed_size = rg_metadata->total_compressed_size();
            rg_meta.uncompressed_size = rg_metadata->total_byte_size();
            
            // Extract column statistics
            for (int col = 0; col < rg_metadata->num_columns(); ++col) {
                auto col_chunk = rg_metadata->ColumnChunk(col);
                if (col_chunk->is_stats_set()) {
                    auto stats = col_chunk->statistics();
                    RowGroupMetadata::ColumnStats cs;
                    cs.column_name = metadata->schema()->Column(col)->name();
                    cs.has_null = stats->HasNullCount() && stats->null_count() > 0;
                    cs.null_count = stats->HasNullCount() ? stats->null_count() : 0;
                    // Note: min/max extraction depends on column type
                    rg_meta.column_stats[cs.column_name] = cs;
                }
            }
            
            row_groups_.push_back(rg_meta);
            
            // Read series_id column to build index
            auto row_group_reader = reader->RowGroup(rg);
            // ... read series_id column and build entries ...
            
            // For each row in this row group:
            for (int64_t row = 0; row < rg_metadata->num_rows(); ++row) {
                // Assuming series_id is extracted as string
                core::SeriesID series_id = /* extract from column */;
                
                RowLocation loc;
                loc.file_index = file_idx;
                loc.row_group_id = rg;
                loc.row_offset_in_group = row;
                
                entries.emplace_back(series_id, loc);
            }
            
            global_row_offset += rg_metadata->num_rows();
        }
        
        total_rows_ = global_row_offset;
    }
    
    // Sort for bulk load
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Bulk load into B+ tree - O(n)
    series_index_.bulk_load(entries);
    
    return core::Result<void>::ok();
}

std::optional<RowLocation> ParquetSecondaryIndex::lookup(
    const core::SeriesID& series_id) const {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto result = series_index_.find(series_id);
    if (!result) return std::nullopt;
    
    // Check if deleted
    // Need to compute global row ID from location
    // For simplicity, check using series_id-based deletion
    if (is_deleted(series_id)) return std::nullopt;
    
    return result;
}

std::vector<std::pair<core::SeriesID, RowLocation>> 
ParquetSecondaryIndex::range_lookup(
    const core::SeriesID& start, 
    const core::SeriesID& end) const {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto results = series_index_.range(start, end);
    
    // Filter out deleted entries
    results.erase(
        std::remove_if(results.begin(), results.end(),
            [this](const auto& entry) { return is_deleted(entry.first); }),
        results.end());
    
    return results;
}

void ParquetSecondaryIndex::mark_deleted(const core::SeriesID& series_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto loc = series_index_.find(series_id);
    if (loc) {
        // Compute global row ID
        int64_t global_row_id = row_groups_[loc->row_group_id].start_row 
                              + loc->row_offset_in_group;
        deletion_bitmap_.add(global_row_id);
    }
}

bool ParquetSecondaryIndex::is_deleted(const core::SeriesID& series_id) const {
    auto loc = series_index_.find(series_id);
    if (!loc) return true;  // Not found = effectively deleted
    
    int64_t global_row_id = row_groups_[loc->row_group_id].start_row 
                          + loc->row_offset_in_group;
    return deletion_bitmap_.contains(global_row_id);
}

core::Result<void> ParquetSecondaryIndex::save(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::ofstream ofs(path, std::ios::binary);
    
    // Header
    uint32_t magic = 0x42505458;  // "BPTX"
    uint32_t version = 1;
    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // B+ tree
    series_index_.serialize(ofs);
    
    // Deletion bitmap (roaring has built-in serialization)
    size_t bitmap_size = deletion_bitmap_.getSizeInBytes();
    ofs.write(reinterpret_cast<const char*>(&bitmap_size), sizeof(bitmap_size));
    std::vector<char> bitmap_data(bitmap_size);
    deletion_bitmap_.write(bitmap_data.data());
    ofs.write(bitmap_data.data(), bitmap_size);
    
    // Row group metadata
    size_t num_rg = row_groups_.size();
    ofs.write(reinterpret_cast<const char*>(&num_rg), sizeof(num_rg));
    for (const auto& rg : row_groups_) {
        // Serialize each row group metadata
        ofs.write(reinterpret_cast<const char*>(&rg.row_group_id), sizeof(rg.row_group_id));
        ofs.write(reinterpret_cast<const char*>(&rg.start_row), sizeof(rg.start_row));
        ofs.write(reinterpret_cast<const char*>(&rg.num_rows), sizeof(rg.num_rows));
        // ... etc
    }
    
    return core::Result<void>::ok();
}

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
```

**Completion Criteria**:
- [ ] Index builds from Parquet files
- [ ] Point lookups return correct RowLocation
- [ ] Range queries work efficiently
- [ ] Deletion bitmap tracks soft deletes
- [ ] Persistence works correctly

---

### TASK-HN15: Integrate Secondary Index with Parquet Reader
**File**: `src/tsdb/storage/parquet/reader.cpp`  
**Effort**: 1 hour  
**Dependencies**: TASK-HN14

```cpp
// In ParquetReader class
class ParquetReader {
public:
    // New: Initialize with secondary index
    void set_secondary_index(std::shared_ptr<ParquetSecondaryIndex> index) {
        secondary_index_ = index;
    }
    
    // Optimized read using secondary index
    core::Result<TimeSeries> read_series_fast(const core::SeriesID& series_id) {
        if (!secondary_index_) {
            return read_series_slow(series_id);  // Fallback to scan
        }
        
        auto loc = secondary_index_->lookup(series_id);
        if (!loc) {
            return core::Result<TimeSeries>::error(
                core::Error(core::ErrorCode::NOT_FOUND, "Series not found"));
        }
        
        // Direct read from specific location
        auto row_group = parquet_reader_->RowGroup(loc->row_group_id);
        // Read only the specific row at loc->row_offset_in_group
        // ...
    }
    
private:
    std::shared_ptr<ParquetSecondaryIndex> secondary_index_;
};
```

**Completion Criteria**:
- [ ] Reader uses secondary index when available
- [ ] Falls back to scan when index not present
- [ ] Point lookup is O(1) instead of O(n)

---

## Master Summary

### Phased Implementation Order

```
┌─────────────────────────────────────────────────────────────────────┐
│                    IMPLEMENTATION ROADMAP                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  PHASE A: B+ Tree Secondary Index (DO FIRST)                       │
│  ─────────────────────────────────────────────                     │
│  Goal: Verify read performance improvement before adding HNSW      │
│                                                                     │
│  BT1 → BT2 → BT3 → BT4 → BT5 → BT6                                │
│   │      │      │      │      │      │                             │
│   ▼      ▼      ▼      ▼      ▼      ▼                             │
│  Deps  Header  Impl  Tests  Integrate  Int.Tests                   │
│                                                                     │
│  Then: BT7 (baseline) → BT8 (verify improvement)                   │
│                                                                     │
│  ✓ GATE: Read latency 10x+ improvement in benchmark-quick          │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  PHASE B: HNSWLib Vector Index (AFTER Phase A verification)        │
│  ──────────────────────────────────────────────────────────        │
│  Goal: Add vector similarity search capability                     │
│                                                                     │
│  HN1 → HN2 → HN3 → HN4 → HN5 → HN6 → HN7 → HN8 → HN9 → HN10 → HN11│
│                                                                     │
│  ✓ GATE: Search latency <1ms for 100K vectors                      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Effort Summary

| Phase | Description | Tasks | Effort | Status |
|-------|-------------|-------|--------|--------|
| **A** | **B+ Tree Secondary Index** | BT1-BT8 | **~9 hours (1 day)** | 🎯 **START HERE** |
| B | HNSWLib Vector Index | HN1-HN11 | ~8.5 hours | ⏸️ After Phase A |

**Total Estimated Effort**: ~2 days

### Quick Commands

```bash
# Phase A workflow
cd /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb

# Step 1: Record baseline BEFORE any changes
make benchmark-quick 2>&1 | tee benchmarks/baseline_before_btree.log

# Step 2: Implement BT1-BT6 (see tasks above)

# Step 3: Build and test
make clean-all && make build
make test-unit
make test-integration

# Step 4: Run benchmark AFTER changes
make benchmark-quick 2>&1 | tee benchmarks/after_btree.log

# Step 5: Compare results
echo "=== READ PERFORMANCE COMPARISON ==="
grep -E "(Read|Query|Latency)" benchmarks/baseline_before_btree.log
echo "---"
grep -E "(Read|Query|Latency)" benchmarks/after_btree.log

# If good, run full benchmark
make benchmark-full 2>&1 | tee benchmarks/after_btree_full.log
```

### Success Criteria

**Phase A (B+ Tree) - Must achieve before Phase B**:
- [ ] `make test-unit` passes
- [ ] `make test-integration` passes
- [ ] Read latency improved **10x or more** in `make benchmark-quick`
- [ ] No regression in write throughput
- [ ] Index memory < 100MB for 1M series

**Phase B (HNSW) - After Phase A verified**:
- [ ] Vector search latency < 1ms for 100K vectors
- [ ] Recall@10 > 95%
- [ ] All existing tests pass

---

## Next Steps After Both Phases

1. **Quantization**: Add PQ support for HNSW memory reduction
2. **Range Queries**: Upgrade std::map to B+ tree for range scan optimization
3. **Multi-Index**: Support multiple HNSW indices for different vector spaces
4. **Distributed**: Shard index across nodes for horizontal scaling
5. **Compaction**: Background process to physically remove deleted rows
