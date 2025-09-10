# LSM Tree Implementation Plan for TSDB Storage Layer

**Generated:** January 2025  
**Implementation Scope:** Hybrid LSM + Block-based Storage Architecture  
**Target Timeline:** 6 months phased implementation  

## 📋 **Implementation Overview**

This document provides a detailed implementation plan for integrating Log Structured Merge (LSM) trees into the TSDB storage layer. The plan follows a hybrid approach that combines LSM write performance with block-based read optimization.

### **Implementation Goals:**
- **67-150% write performance improvement**
- **Maintain read performance** of current architecture
- **Seamless integration** with existing components
- **Backward compatibility** during migration
- **Gradual rollout** to minimize risk

## 🏗️ **Architecture Design**

### **High-Level Architecture**

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              HYBRID LSM + BLOCK ARCHITECTURE                   │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CLIENT INTERFACE                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Storage    │  │  Write      │  │  Read       │  │  Query      │           │
│  │  Interface  │  │  Operations │  │  Operations │  │  Operations │           │
│  │             │  │             │  │             │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              STORAGE IMPL                                      │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  LSM        │  │  Block      │  │  Cache      │  │  Compression│           │
│  │  Manager    │  │  Manager    │  │  Hierarchy  │  │  Engine     │           │
│  │             │  │             │  │             │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              LSM COMPONENTS                                    │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  MemTable   │  │  SSTable    │  │  Compaction │  │  Write-Ahead│           │
│  │  Manager    │  │  Manager    │  │  Engine     │  │  Log        │           │
│  │             │  │             │  │             │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### **Component Responsibilities**

#### **1. LSM Manager**
- **Coordinator**: Manages LSM tree lifecycle
- **Write Path**: Routes writes to appropriate components
- **Read Path**: Coordinates reads across LSM and block storage
- **Compaction**: Schedules and manages compaction operations

#### **2. MemTable Manager**
- **Active MemTable**: Handles current write operations
- **Immutable MemTables**: Manages tables ready for flushing
- **Bloom Filters**: Provides fast point query filtering
- **Memory Management**: Controls MemTable size and eviction

#### **3. SSTable Manager**
- **Level Management**: Organizes SSTables by levels
- **File Operations**: Handles SSTable creation and deletion
- **Index Management**: Maintains SSTable indexes
- **Metadata**: Tracks SSTable metadata and statistics

#### **4. Compaction Engine**
- **Compaction Scheduling**: Determines when to compact
- **Level Compaction**: Merges SSTables within levels
- **Cross-Level Compaction**: Moves data between levels
- **Background Processing**: Runs compaction in background

## 📁 **File Structure and Organization**

### **New Header Files**

```
include/tsdb/storage/lsm/
├── lsm_manager.h              # Main LSM coordinator
├── memtable_manager.h         # MemTable lifecycle management
├── sstable_manager.h          # SSTable operations and management
├── compaction_engine.h        # Compaction logic and scheduling
├── write_ahead_log.h          # WAL implementation
├── bloom_filter.h             # Bloom filter for point queries
├── sstable.h                  # SSTable structure and operations
├── memtable.h                 # MemTable structure and operations
└── lsm_types.h                # LSM-specific types and constants
```

### **New Source Files**

```
src/tsdb/storage/lsm/
├── lsm_manager.cpp            # LSM manager implementation
├── memtable_manager.cpp       # MemTable manager implementation
├── sstable_manager.cpp        # SSTable manager implementation
├── compaction_engine.cpp      # Compaction engine implementation
├── write_ahead_log.cpp        # WAL implementation
├── bloom_filter.cpp           # Bloom filter implementation
├── sstable.cpp                # SSTable implementation
├── memtable.cpp               # MemTable implementation
└── lsm_utils.cpp              # LSM utility functions
```

### **Integration Points**

```
src/tsdb/storage/
├── storage_impl.cpp           # Modified to integrate LSM
├── storage_factory.cpp        # Updated factory for LSM
└── block_manager.cpp          # Enhanced for LSM integration
```

## 🛠️ **Implementation Phases**

### **Phase 1: Core LSM Components (Months 1-2)**

#### **Week 1-2: Foundation**
```cpp
// 1. Define LSM types and constants
// include/tsdb/storage/lsm/lsm_types.h
namespace tsdb {
namespace storage {
namespace lsm {

enum class CompactionStrategy {
    SIZE_TIERED,
    TIME_TIERED,
    LEVELED
};

struct LSMConfig {
    size_t memtable_size = 64 * 1024 * 1024;  // 64MB
    size_t sstable_levels = 7;
    CompactionStrategy compaction_strategy = CompactionStrategy::SIZE_TIERED;
    size_t bloom_filter_bits = 10;
    size_t write_buffer_size = 32 * 1024 * 1024;  // 32MB
};

struct SSTableMetadata {
    uint64_t id;
    size_t level;
    int64_t min_timestamp;
    int64_t max_timestamp;
    size_t size_bytes;
    std::vector<SeriesID> series_ids;
};

} // namespace lsm
} // namespace storage
} // namespace tsdb
```

#### **Week 3-4: MemTable Implementation**
```cpp
// include/tsdb/storage/lsm/memtable.h
class MemTable {
private:
    std::map<SeriesID, core::TimeSeries> data_;
    std::unique_ptr<BloomFilter> bloom_filter_;
    std::unique_ptr<WriteAheadLog> wal_;
    size_t max_size_;
    std::atomic<bool> immutable_;
    std::atomic<size_t> current_size_;
    
public:
    explicit MemTable(const lsm::LSMConfig& config);
    ~MemTable() = default;
    
    core::Result<void> write(const core::TimeSeries& series);
    core::Result<core::TimeSeries> read(const core::Labels& labels);
    core::Result<void> flush_to_sstable();
    bool is_full() const;
    bool is_immutable() const;
    void make_immutable();
    size_t size() const;
    size_t series_count() const;
};
```

#### **Week 5-6: SSTable Implementation**
```cpp
// include/tsdb/storage/lsm/sstable.h
class SSTable {
private:
    lsm::SSTableMetadata metadata_;
    std::string file_path_;
    std::unique_ptr<BloomFilter> bloom_filter_;
    std::map<SeriesID, size_t> index_;
    
public:
    explicit SSTable(const std::string& file_path);
    ~SSTable() = default;
    
    core::Result<void> write(const std::vector<core::TimeSeries>& series);
    core::Result<core::TimeSeries> read(const core::Labels& labels);
    core::Result<std::vector<core::TimeSeries>> read_range(
        const core::Labels& labels, int64_t start_time, int64_t end_time);
    bool contains_series(const SeriesID& series_id) const;
    const lsm::SSTableMetadata& metadata() const;
    size_t size() const;
};
```

#### **Week 7-8: Bloom Filter and WAL**
```cpp
// include/tsdb/storage/lsm/bloom_filter.h
class BloomFilter {
private:
    std::vector<bool> bits_;
    size_t num_bits_;
    size_t num_hashes_;
    std::vector<std::hash<std::string>> hash_functions_;
    
public:
    explicit BloomFilter(size_t num_bits, size_t num_hashes);
    void add(const std::string& key);
    bool contains(const std::string& key) const;
    void clear();
    size_t size() const;
};

// include/tsdb/storage/lsm/write_ahead_log.h
class WriteAheadLog {
private:
    std::string file_path_;
    std::ofstream file_;
    std::mutex mutex_;
    
public:
    explicit WriteAheadLog(const std::string& file_path);
    ~WriteAheadLog();
    
    core::Result<void> append(const core::TimeSeries& series);
    core::Result<std::vector<core::TimeSeries>> replay();
    core::Result<void> truncate();
    core::Result<void> close();
};
```

### **Phase 2: Management Components (Months 3-4)**

#### **Week 9-10: MemTable Manager**
```cpp
// include/tsdb/storage/lsm/memtable_manager.h
class MemTableManager {
private:
    lsm::LSMConfig config_;
    std::unique_ptr<MemTable> active_memtable_;
    std::vector<std::unique_ptr<MemTable>> immutable_memtables_;
    std::mutex mutex_;
    
public:
    explicit MemTableManager(const lsm::LSMConfig& config);
    ~MemTableManager() = default;
    
    core::Result<void> write(const core::TimeSeries& series);
    core::Result<core::TimeSeries> read(const core::Labels& labels);
    core::Result<std::vector<core::TimeSeries>> read_range(
        const core::Labels& labels, int64_t start_time, int64_t end_time);
    core::Result<std::unique_ptr<MemTable>> get_immutable_memtable();
    bool has_immutable_memtables() const;
    size_t immutable_count() const;
};
```

#### **Week 11-12: SSTable Manager**
```cpp
// include/tsdb/storage/lsm/sstable_manager.h
class SSTableManager {
private:
    lsm::LSMConfig config_;
    std::vector<std::vector<std::unique_ptr<SSTable>>> levels_;
    std::string data_dir_;
    std::mutex mutex_;
    
public:
    explicit SSTableManager(const lsm::LSMConfig& config, const std::string& data_dir);
    ~SSTableManager() = default;
    
    core::Result<void> add_sstable(std::unique_ptr<SSTable> sstable, size_t level);
    core::Result<core::TimeSeries> read(const core::Labels& labels);
    core::Result<std::vector<core::TimeSeries>> read_range(
        const core::Labels& labels, int64_t start_time, int64_t end_time);
    core::Result<std::vector<std::unique_ptr<SSTable>>> get_level_sstables(size_t level);
    core::Result<void> remove_sstable(const SSTable* sstable, size_t level);
    size_t level_count(size_t level) const;
    size_t total_sstables() const;
};
```

#### **Week 13-14: Compaction Engine**
```cpp
// include/tsdb/storage/lsm/compaction_engine.h
class CompactionEngine {
private:
    lsm::LSMConfig config_;
    std::unique_ptr<BackgroundProcessor> processor_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    
public:
    explicit CompactionEngine(const lsm::LSMConfig& config);
    ~CompactionEngine();
    
    core::Result<void> start();
    core::Result<void> stop();
    core::Result<void> schedule_compaction();
    core::Result<void> compact_level(size_t level);
    core::Result<void> merge_sstables(const std::vector<SSTable*>& inputs, size_t target_level);
    bool is_running() const;
};
```

#### **Week 15-16: LSM Manager**
```cpp
// include/tsdb/storage/lsm/lsm_manager.h
class LSMManager {
private:
    lsm::LSMConfig config_;
    std::unique_ptr<MemTableManager> memtable_manager_;
    std::unique_ptr<SSTableManager> sstable_manager_;
    std::unique_ptr<CompactionEngine> compaction_engine_;
    std::atomic<bool> initialized_;
    
public:
    explicit LSMManager(const lsm::LSMConfig& config, const std::string& data_dir);
    ~LSMManager();
    
    core::Result<void> init();
    core::Result<void> write(const core::TimeSeries& series);
    core::Result<core::TimeSeries> read(const core::Labels& labels);
    core::Result<std::vector<core::TimeSeries>> read_range(
        const core::Labels& labels, int64_t start_time, int64_t end_time);
    core::Result<void> compact();
    core::Result<void> flush();
    core::Result<void> close();
    std::string stats() const;
};
```

### **Phase 3: Integration and Optimization (Months 5-6)**

#### **Week 17-18: StorageImpl Integration**
```cpp
// Modified src/tsdb/storage/storage_impl.h
class StorageImpl : public Storage {
private:
    // Existing components
    mutable std::shared_mutex mutex_;
    std::shared_ptr<BlockManager> block_manager_;
    bool initialized_;
    core::StorageConfig config_;
    
    // New LSM components
    std::unique_ptr<lsm::LSMManager> lsm_manager_;
    bool lsm_enabled_;
    
    // Integration helpers
    core::Result<void> write_to_lsm(const core::TimeSeries& series);
    core::Result<void> write_to_blocks(const core::TimeSeries& series);
    core::Result<core::TimeSeries> read_from_lsm(const core::Labels& labels, 
                                                 int64_t start_time, 
                                                 int64_t end_time);
    core::Result<core::TimeSeries> read_from_blocks(const core::Labels& labels, 
                                                    int64_t start_time, 
                                                    int64_t end_time);
    
public:
    // Existing methods remain the same
    core::Result<void> write(const core::TimeSeries& series) override;
    core::Result<core::TimeSeries> read(const core::Labels& labels,
                                       int64_t start_time,
                                       int64_t end_time) override;
    // ... other methods
};
```

#### **Week 19-20: Configuration and Factory Updates**
```cpp
// Modified include/tsdb/storage/storage.h
struct StorageOptions {
    // Existing options
    std::string data_dir;
    size_t max_block_size;
    size_t max_block_records;
    std::chrono::seconds block_duration;
    size_t max_concurrent_compactions;
    
    // New LSM options
    bool lsm_enabled = false;
    lsm::LSMConfig lsm_config;
    
    static StorageOptions Default() {
        return {
            "data",                // data_dir
            64 * 1024 * 1024,     // max_block_size
            1000000,              // max_block_records
            std::chrono::hours(2), // block_duration
            2,                     // max_concurrent_compactions
            false,                 // lsm_enabled
            lsm::LSMConfig{}       // lsm_config
        };
    }
};
```

#### **Week 21-22: Testing and Validation**
```cpp
// New test files
test/unit/storage/
├── lsm_manager_test.cpp
├── memtable_manager_test.cpp
├── sstable_manager_test.cpp
├── compaction_engine_test.cpp
├── bloom_filter_test.cpp
└── write_ahead_log_test.cpp

test/integration/
├── lsm_integration_test.cpp
├── lsm_block_integration_test.cpp
└── lsm_performance_test.cpp
```

#### **Week 23-24: Performance Optimization**
- **Memory Management**: Optimize MemTable memory usage
- **I/O Optimization**: Batch operations and async I/O
- **Compaction Tuning**: Optimize compaction scheduling
- **Cache Integration**: Integrate with existing cache hierarchy

## 🔧 **Configuration and Tuning**

### **LSM Configuration Options**

```yaml
storage:
  lsm:
    enabled: true
    memtable_size: 64MB
    sstable_levels: 7
    compaction_strategy: size_tiered
    bloom_filter_bits: 10
    write_buffer_size: 32MB
    
  compaction:
    max_concurrent: 4
    target_file_size: 64MB
    max_file_size: 256MB
    compaction_threshold: 0.8
    
  integration:
    hybrid_mode: true
    sstable_to_block_conversion: true
    adaptive_compaction: true
    write_path: lsm
    read_path: hybrid
```

### **Performance Tuning Parameters**

#### **Write Performance**
```cpp
struct WriteTuning {
    size_t memtable_flush_threshold = 0.8;  // 80% full
    size_t write_batch_size = 1000;         // Batch writes
    bool async_wal = true;                  // Async WAL writes
    size_t max_immutable_memtables = 3;     // Max immutable tables
};
```

#### **Read Performance**
```cpp
struct ReadTuning {
    size_t bloom_filter_bits = 10;          // Bloom filter accuracy
    bool parallel_sstable_reads = true;     // Parallel SSTable reads
    size_t read_buffer_size = 1MB;          // Read buffer size
    bool cache_sstable_metadata = true;     // Cache metadata
};
```

#### **Compaction Performance**
```cpp
struct CompactionTuning {
    size_t max_concurrent_compactions = 4;  // Concurrent compactions
    double compaction_threshold = 0.8;      // Compaction trigger
    size_t target_sstable_size = 64MB;      // Target SSTable size
    bool adaptive_compaction = true;        // Adaptive scheduling
};
```

## 📊 **Testing Strategy**

### **Unit Tests**

#### **Component Testing**
```cpp
// Example test structure
TEST_F(LSMManagerTest, BasicWriteAndRead) {
    lsm::LSMConfig config;
    config.memtable_size = 1024 * 1024;  // 1MB
    
    auto manager = std::make_unique<lsm::LSMManager>(config, "./test_data");
    ASSERT_TRUE(manager->init().ok());
    
    // Write test data
    core::Labels labels;
    labels.add("test", "basic");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = manager->write(series);
    ASSERT_TRUE(write_result.ok());
    
    // Read test data
    auto read_result = manager->read(labels);
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(read_result.value().samples().size(), 1);
    EXPECT_EQ(read_result.value().samples()[0].value(), 42.0);
}
```

#### **Performance Testing**
```cpp
TEST_F(LSMPerformanceTest, WriteThroughput) {
    // Test write throughput with various configurations
    std::vector<size_t> memtable_sizes = {32, 64, 128, 256};  // MB
    
    for (auto size : memtable_sizes) {
        lsm::LSMConfig config;
        config.memtable_size = size * 1024 * 1024;
        
        auto manager = std::make_unique<lsm::LSMManager>(config, "./test_data");
        ASSERT_TRUE(manager->init().ok());
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Write 1M samples
        for (int i = 0; i < 1000000; ++i) {
            core::Labels labels;
            labels.add("test", "throughput");
            core::TimeSeries series(labels);
            series.add_sample(1000000000 + i, static_cast<double>(i));
            
            auto result = manager->write(series);
            ASSERT_TRUE(result.ok());
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        double throughput = 1000000.0 / (duration.count() / 1000.0);
        std::cout << "MemTable size " << size << "MB: " << throughput << " writes/sec" << std::endl;
    }
}
```

### **Integration Tests**

#### **LSM + Block Integration**
```cpp
TEST_F(LSMBlockIntegrationTest, HybridReadWrite) {
    // Test hybrid mode with both LSM and block storage
    core::StorageConfig config;
    config.lsm_enabled = true;
    config.lsm_config.memtable_size = 64 * 1024 * 1024;
    
    auto storage = std::make_unique<StorageImpl>(config);
    ASSERT_TRUE(storage->init(config).ok());
    
    // Write data (should go to LSM)
    core::Labels labels;
    labels.add("test", "hybrid");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok());
    
    // Read data (should come from LSM)
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(read_result.value().samples().size(), 1);
    
    // Trigger compaction (should convert to blocks)
    auto compact_result = storage->compact();
    ASSERT_TRUE(compact_result.ok());
    
    // Read data (should now come from blocks)
    auto read_result2 = storage->read(labels, 1000000000, 1000000000);
    ASSERT_TRUE(read_result2.ok());
    EXPECT_EQ(read_result2.value().samples().size(), 1);
}
```

### **Stress Tests**

#### **Concurrent Access**
```cpp
TEST_F(LSMStressTest, ConcurrentWrites) {
    lsm::LSMConfig config;
    config.memtable_size = 64 * 1024 * 1024;
    
    auto manager = std::make_unique<lsm::LSMManager>(config, "./test_data");
    ASSERT_TRUE(manager->init().ok());
    
    const size_t num_threads = 8;
    const size_t writes_per_thread = 10000;
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_writes{0};
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&manager, &successful_writes, writes_per_thread, i]() {
            for (size_t j = 0; j < writes_per_thread; ++j) {
                core::Labels labels;
                labels.add("thread", std::to_string(i));
                labels.add("sample", std::to_string(j));
                
                core::TimeSeries series(labels);
                series.add_sample(1000000000 + j, static_cast<double>(j));
                
                auto result = manager->write(series);
                if (result.ok()) {
                    successful_writes++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(successful_writes.load(), num_threads * writes_per_thread);
}
```

## 📈 **Performance Monitoring**

### **LSM-Specific Metrics**

#### **Write Metrics**
```cpp
struct LSMWriteMetrics {
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> memtable_writes{0};
    std::atomic<uint64_t> sstable_writes{0};
    std::atomic<uint64_t> write_latency_ns{0};
    std::atomic<uint64_t> memtable_flushes{0};
    std::atomic<uint64_t> write_errors{0};
};
```

#### **Read Metrics**
```cpp
struct LSMReadMetrics {
    std::atomic<uint64_t> total_reads{0};
    std::atomic<uint64_t> memtable_reads{0};
    std::atomic<uint64_t> sstable_reads{0};
    std::atomic<uint64_t> bloom_filter_hits{0};
    std::atomic<uint64_t> bloom_filter_misses{0};
    std::atomic<uint64_t> read_latency_ns{0};
    std::atomic<uint64_t> read_errors{0};
};
```

#### **Compaction Metrics**
```cpp
struct LSMCompactionMetrics {
    std::atomic<uint64_t> total_compactions{0};
    std::atomic<uint64_t> level_compactions[10]{0};  // Up to 10 levels
    std::atomic<uint64_t> compaction_duration_ns{0};
    std::atomic<uint64_t> sstables_merged{0};
    std::atomic<uint64_t> compaction_errors{0};
};
```

### **Monitoring Integration**

```cpp
class LSMMonitor {
private:
    LSMWriteMetrics write_metrics_;
    LSMReadMetrics read_metrics_;
    LSMCompactionMetrics compaction_metrics_;
    
public:
    void record_write(uint64_t latency_ns, bool from_memtable);
    void record_read(uint64_t latency_ns, bool from_memtable, bool bloom_hit);
    void record_compaction(size_t level, uint64_t duration_ns, size_t sstables_merged);
    
    std::string get_metrics_summary() const;
    void reset_metrics();
};
```

## 🚀 **Deployment Strategy**

### **Gradual Rollout Plan**

#### **Phase 1: Development and Testing (Months 1-2)**
- Implement core LSM components
- Unit and integration testing
- Performance benchmarking
- Documentation updates

#### **Phase 2: Staging Deployment (Month 3)**
- Deploy to staging environment
- Load testing with production-like data
- Performance validation
- Bug fixes and optimizations

#### **Phase 3: Production Pilot (Month 4)**
- Deploy to small production cluster
- Monitor performance and stability
- Gather feedback and metrics
- Refine configuration

#### **Phase 4: Full Production Rollout (Months 5-6)**
- Gradual rollout to all production clusters
- Monitor performance and stability
- Optimize based on real-world usage
- Complete migration from block-only storage

### **Rollback Plan**

#### **Immediate Rollback Triggers**
- **Data Loss**: Any indication of data corruption
- **Performance Degradation**: >20% performance regression
- **Stability Issues**: Frequent crashes or errors
- **Resource Exhaustion**: Memory or disk space issues

#### **Rollback Procedure**
```cpp
class LSMRollbackManager {
public:
    core::Result<void> disable_lsm_writes();
    core::Result<void> enable_block_only_mode();
    core::Result<void> validate_data_consistency();
    core::Result<void> restore_from_backup();
    core::Result<void> notify_operations_team();
};
```

## 📋 **Success Criteria**

### **Performance Targets**
- **Write Throughput**: ≥67% improvement (4.8M → 8M+ metrics/sec)
- **Write Latency**: ≥80% reduction (1-5ms → 0.1-1ms)
- **Read Performance**: ≤10% degradation for range queries
- **Storage Efficiency**: ≥10% improvement in space utilization

### **Stability Targets**
- **Uptime**: ≥99.9% availability
- **Data Consistency**: 100% data integrity
- **Error Rate**: ≤0.1% error rate
- **Recovery Time**: ≤5 minutes for full recovery

### **Operational Targets**
- **Monitoring**: Complete observability of LSM operations
- **Documentation**: Comprehensive operational guides
- **Training**: Operations team fully trained on LSM
- **Support**: 24/7 support for LSM-related issues

---

**This implementation plan provides a comprehensive roadmap for integrating LSM trees into the TSDB storage layer while maintaining the strengths of the existing block-based architecture. The phased approach ensures minimal risk and maximum benefit from the hybrid architecture.** 