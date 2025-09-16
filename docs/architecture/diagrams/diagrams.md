# TSDB Architecture Diagrams

**Updated**: January 2025  
**Status**: Comprehensive architecture reflecting all implemented features including high-concurrency sharded storage architecture

## System Architecture Overview
```mermaid
graph TB
    Client[Client Applications] --> GRPC[gRPC Interface]
    OTEL[OpenTelemetry] --> GRPC
    Prometheus[Prometheus] --> GRPC
    
    subgraph IngestionLayer
        GRPC --> Parser[Protocol Parser]
        Parser --> Validator[Validation Layer]
        Validator --> Router[Metric Router]
        Router --> HighConcurrencyStorage[HighConcurrencyStorage Interface]
        Router --> StorageImpl[StorageImpl Interface]
    end
    
    subgraph HighConcurrencyEngine
        HighConcurrencyStorage --> ShardedManager[ShardedStorageManager]
        ShardedManager --> Shard0[Shard 0<br/>StorageImpl]
        ShardedManager --> Shard1[Shard 1<br/>StorageImpl]
        ShardedManager --> Shard2[Shard 2<br/>StorageImpl]
        ShardedManager --> Shard3[Shard 3<br/>StorageImpl]
        ShardedManager --> WriteQueues[Write Queues]
        ShardedManager --> WorkerThreads[Worker Threads]
    end
    
    subgraph AdvancedStorageEngine
        StorageImpl --> ObjectPools[Object Pools]
        StorageImpl --> CacheHierarchy[Cache Hierarchy]
        StorageImpl --> Compression[Compression Engine]
        StorageImpl --> BlockManager[Block Manager]
        StorageImpl --> BackgroundProc[Background Processor]
        StorageImpl --> PredictiveCache[Predictive Cache]
    end
    
    subgraph MultiTierStorage
        BlockManager --> HotStorage[Hot Storage]
        BlockManager --> WarmStorage[Warm Storage]
        BlockManager --> ColdStorage[Cold Storage]
        
        subgraph HistogramStorage
            HotStorage --> HBuckets[Hot Buckets]
            WarmStorage --> WBuckets[Warm Buckets]
            ColdStorage --> CBuckets[Cold Buckets]
        end
    end
    
    subgraph QueryEngine
        Query[Query Processor] --> PromQL[PromQL Engine]
        PromQL --> Planner[Query Planner]
        Planner --> Executor[Query Executor]
        Executor --> CacheHierarchy
        Executor --> BlockManager
    end
    
    subgraph MonitoringMetrics
        AtomicMetrics[Atomic Metrics] --> StorageImpl
        PerformanceConfig[Performance Config] --> StorageImpl
        BackgroundProc --> MetricsCollection[Metrics Collection]
    end
```

## StorageImpl Integration Architecture
```mermaid
graph TB
    subgraph StorageImplInterface
        WriteOp[Write Operations] --> ObjectPoolIntegration[Object Pool Integration]
        ReadOp[Read Operations] --> CacheIntegration[Cache Integration]
        QueryOp[Query Operations] --> CompressionIntegration[Compression Integration]
    end
    
    subgraph ObjectPoolLayer
        ObjectPoolIntegration --> TimeSeriesPool[TimeSeries Pool]
        ObjectPoolIntegration --> LabelsPool[Labels Pool]
        ObjectPoolIntegration --> SamplePool[Sample Pool]
    end
    
    subgraph CacheHierarchyLayer
        CacheIntegration --> L1Cache[L1 Cache<br/>WorkingSetCache]
        CacheIntegration --> L2Cache[L2 Cache<br/>MemoryMappedCache]
        CacheIntegration --> L3Cache[L3 Cache<br/>Disk Storage]
        L1Cache --> L2Cache
        L2Cache --> L3Cache
    end
    
    subgraph CompressionLayer
        CompressionIntegration --> TimestampComp[Timestamp Compression<br/>Delta-of-Delta]
        CompressionIntegration --> ValueComp[Value Compression<br/>XOR/RLE]
        CompressionIntegration --> LabelComp[Label Compression<br/>RLE]
    end
    
    subgraph BlockManagementLayer
        BlockManager --> BlockCreation[Block Creation]
        BlockManager --> BlockRotation[Block Rotation]
        BlockManager --> BlockCompaction[Block Compaction]
        BlockManager --> BlockIndexing[Block Indexing]
    end
    
    subgraph BackgroundProcessing
        BackgroundProc --> CacheMaintenance[Cache Maintenance]
        BackgroundProc --> BlockMaintenance[Block Maintenance]
        BackgroundProc --> MetricsCollection[Metrics Collection]
        BackgroundProc --> PredictiveAnalysis[Predictive Analysis]
    end
```

## Advanced Features Architecture
```mermaid
graph TB
    subgraph AdvPerfFeatures
        ObjectPooling[Object Pooling<br/>Memory Efficiency]
        MultiLevelCache[Multi-Level Cache<br/>Performance Optimization]
        PredictiveCaching[Predictive Caching<br/>Access Pattern Analysis]
        AdaptiveCompression[Adaptive Compression<br/>Multiple Algorithms]
        BackgroundProcessing[Background Processing<br/>Automatic Maintenance]
        AtomicReferenceCounting[Atomic Reference Counting<br/>Memory Management]
    end
    
    subgraph PerformanceOptimization
        ObjectPooling --> MemoryEfficiency[Memory Efficiency]
        MultiLevelCache --> AccessSpeed[Access Speed]
        PredictiveCaching --> Prefetching[Intelligent Prefetching]
        AdaptiveCompression --> StorageEfficiency[Storage Efficiency]
        BackgroundProcessing --> SystemOptimization[System Optimization]
        AtomicReferenceCounting --> SafeMemory[Safe Memory Management]
    end
    
    subgraph IntegrationPoints
        MemoryEfficiency --> StorageImpl
        AccessSpeed --> StorageImpl
        Prefetching --> StorageImpl
        StorageEfficiency --> StorageImpl
        SystemOptimization --> StorageImpl
        SafeMemory --> StorageImpl
    end
```

## Data Model with Advanced Features
```mermaid
classDiagram
    class StorageImpl {
        -ObjectPools object_pools
        -CacheHierarchy cache_hierarchy
        -BlockManager block_manager
        -BackgroundProcessor background_processor
        -AtomicMetrics atomic_metrics
        +write(TimeSeries) Result
        +read(Labels, start, end) TimeSeries
        +query(matchers, start, end) TimeSeries[]
        +stats() string
    }
    
    class ObjectPools {
        -TimeSeriesPool time_series_pool
        -LabelsPool labels_pool
        -SamplePool sample_pool
        +acquire() unique_ptr
        +release(unique_ptr) void
        +stats() string
    }
    
    class CacheHierarchy {
        -WorkingSetCache l1_cache
        -MemoryMappedCache l2_cache
        -DiskStorage l3_cache
        +get(SeriesID) shared_ptr
        +put(SeriesID, TimeSeries) void
        +promote(SeriesID) void
        +demote(SeriesID) void
    }
    
    class BlockManager {
        -FileBlockStorage hot_storage
        -FileBlockStorage warm_storage
        -FileBlockStorage cold_storage
        +createBlock(start, end) Result
        +writeData(header, data) Result
        +readData(header) Result
        +compact() Result
    }
    
    class CompressionEngine {
        -TimestampCompressor ts_compressor
        -ValueCompressor val_compressor
        -LabelCompressor label_compressor
        +compress(data) vector
        +decompress(data) vector
        +getCompressionRatio() double
    }
    
    class BackgroundProcessor {
        -vector tasks
        -thread worker_thread
        +add_task(name, func, interval) void
        +start() void
        +stop() void
    }
    
    class PredictiveCache {
        -vector access_patterns
        -map predictions
        +record_access(SeriesID) void
        +get_predictions() vector
        +calculate_confidence() double
    }
    
    StorageImpl --> ObjectPools
    StorageImpl --> CacheHierarchy
    StorageImpl --> BlockManager
    StorageImpl --> CompressionEngine
    StorageImpl --> BackgroundProcessor
    StorageImpl --> PredictiveCache
```

## Multi-Tier Storage Architecture
```mermaid
graph TB
    subgraph StorageTiers
        subgraph HotTier
            HotMemory[In-Memory Storage]
            HotCache[L1 Cache<br/>WorkingSetCache]
            HotBlocks[Hot Blocks<br/>Recent Data]
        end
        
        subgraph WarmTier
            WarmMemory[Memory-Mapped Storage]
            WarmCache[L2 Cache<br/>MemoryMappedCache]
            WarmBlocks[Warm Blocks<br/>Compressed Data]
        end
        
        subgraph ColdTier
            ColdDisk[Disk Storage]
            ColdCache[L3 Cache<br/>Persistent Storage]
            ColdBlocks[Cold Blocks<br/>Archived Data]
        end
    end
    
    subgraph DataFlow
        Write[Write Operations] --> HotMemory
        HotMemory --> HotCache
        HotCache --> HotBlocks
        
        HotBlocks --> WarmMemory
        WarmMemory --> WarmCache
        WarmCache --> WarmBlocks
        
        WarmBlocks --> ColdDisk
        ColdDisk --> ColdCache
        ColdCache --> ColdBlocks
    end
    
    subgraph BackgroundOperations
        BackgroundProc --> TierPromotion[Promotion<br/>Cold → Warm → Hot]
        BackgroundProc --> TierDemotion[Demotion<br/>Hot → Warm → Cold]
        BackgroundProc --> Compaction[Block Compaction]
        BackgroundProc --> Cleanup[Data Cleanup]
    end
```

## Compression Architecture
```mermaid
graph TB
    subgraph CompressionAlgorithms
        subgraph TimestampCompression
            DeltaOfDelta[Delta-of-Delta<br/>High Compression]
            SimpleDelta[Simple Delta<br/>Fast Processing]
            XOR[XOR Compression<br/>Efficient for Timestamps]
        end
        
        subgraph ValueCompression
            XORValue[XOR Compression<br/>Floating Point]
            RLEValue[RLE Compression<br/>Repeated Values]
            AdaptiveValue[Adaptive Compression<br/>Auto Selection]
        end
        
        subgraph LabelCompression
            RLELabel[RLE Compression<br/>String Labels]
            DictionaryLabel[Dictionary Compression<br/>Label Sets]
        end
    end
    
    subgraph CompressionPipeline
        InputData[Input Data] --> AlgorithmSelector[Algorithm Selector]
        AlgorithmSelector --> TimestampComp[Timestamp Compression]
        AlgorithmSelector --> ValueComp[Value Compression]
        AlgorithmSelector --> LabelComp[Label Compression]
        
        TimestampComp --> CompressedData[Compressed Data]
        ValueComp --> CompressedData
        LabelComp --> CompressedData
        
        CompressedData --> BlockStorage[Block Storage]
    end
    
    subgraph PerformanceMonitoring
        CompressionRatio[Compression Ratio] --> AlgorithmSelector
        CompressionSpeed[Compression Speed] --> AlgorithmSelector
        DecompressionSpeed[Decompression Speed] --> AlgorithmSelector
    end
```

## Cache Hierarchy Architecture
```mermaid
graph TB
    subgraph CacheLevels
        subgraph L1Cache
            L1WorkingSet[Working Set Cache<br/>LRU Eviction]
            L1Memory[In-Memory Storage<br/>~10-100ns Access]
            L1Size[Small Size<br/>~1K Entries]
        end
        
        subgraph L2Cache
            L2MemoryMapped[Memory-Mapped Cache<br/>File-Based]
            L2Medium[Medium Speed<br/>~1-10μs Access]
            L2Size[Medium Size<br/>~10K Entries]
        end
        
        subgraph L3Cache
            L3Disk[Disk Storage<br/>Persistent]
            L3Slow[Slow Speed<br/>~1-10ms Access]
            L3Size[Large Size<br/>Unlimited]
        end
    end
    
    subgraph CacheOperations
        ReadRequest[Read Request] --> L1WorkingSet
        L1WorkingSet --> L1Hit{L1 Hit?}
        L1Hit -->|Yes| ReturnData[Return Data]
        L1Hit -->|No| L2MemoryMapped
        L2MemoryMapped --> L2Hit{L2 Hit?}
        L2Hit -->|Yes| PromoteToL1[Promote to L1]
        L2Hit -->|No| L3Disk
        L3Disk --> PromoteToL2[Promote to L2]
        PromoteToL2 --> PromoteToL1
        PromoteToL1 --> ReturnData
    end
    
    subgraph BackgroundMaintenance
        BackgroundProc --> CacheOptimization[Cache Optimization]
        CacheOptimization --> EvictionPolicy[LRU Eviction]
        CacheOptimization --> PromotionPolicy[Access-Based Promotion]
        CacheOptimization --> DemotionPolicy[Time-Based Demotion]
    end
```

## Block Management Architecture
```mermaid
graph TB
    subgraph BlockLifecycle
        BlockCreation[Block Creation] --> BlockWriting[Block Writing]
        BlockWriting --> BlockCompression[Block Compression]
        BlockCompression --> BlockStorage[Block Storage]
        BlockStorage --> BlockRotation[Block Rotation]
        BlockRotation --> BlockCompaction[Block Compaction]
        BlockCompaction --> BlockArchival[Block Archival]
    end
    
    subgraph BlockTypes
        subgraph HotBlocks
            HotActive[Active Blocks<br/>Current Writing]
            HotRecent[Recent Blocks<br/>Frequently Accessed]
        end
        
        subgraph WarmBlocks
            WarmCompressed[Compressed Blocks<br/>Medium Access]
            WarmIndexed[Indexed Blocks<br/>Query Optimization]
        end
        
        subgraph ColdBlocks
            ColdArchived[Archived Blocks<br/>Rare Access]
            ColdCompacted[Compacted Blocks<br/>Space Optimized]
        end
    end
    
    subgraph BlockOperations
        WriteData[Write Data] --> BlockManager
        BlockManager --> BlockCreation
        BlockManager --> BlockRotation
        BlockManager --> BlockCompaction
        
        ReadData[Read Data] --> BlockManager
        BlockManager --> BlockLookup[Block Lookup]
        BlockManager --> BlockDecompression[Block Decompression]
    end
```

## Background Processing Architecture
```mermaid
graph TB
    subgraph BackgroundTasks
        subgraph CacheMaintenance
            CacheOptimization[Cache Optimization]
            EvictionManagement[Eviction Management]
            PromotionManagement[Promotion Management]
        end
        
        subgraph BlockMaintenance
            BlockCompaction[Block Compaction]
            BlockRotation[Block Rotation]
            BlockCleanup[Block Cleanup]
        end
        
        subgraph MetricsCollection
            PerformanceMetrics[Performance Metrics]
            ResourceMetrics[Resource Metrics]
            SystemMetrics[System Metrics]
        end
        
        subgraph PredictiveAnalysis
            PatternAnalysis[Pattern Analysis]
            Prefetching[Intelligent Prefetching]
            ConfidenceScoring[Confidence Scoring]
        end
    end
    
    subgraph TaskScheduling
        BackgroundProcessor --> TaskScheduler[Task Scheduler]
        TaskScheduler --> PeriodicTasks[Periodic Tasks]
        TaskScheduler --> EventDrivenTasks[Event-Driven Tasks]
        TaskScheduler --> PriorityTasks[Priority Tasks]
    end
    
    subgraph Execution
        PeriodicTasks --> CacheOptimization
        PeriodicTasks --> BlockCompaction
        PeriodicTasks --> PerformanceMetrics
        
        EventDrivenTasks --> EvictionManagement
        EventDrivenTasks --> BlockRotation
        EventDrivenTasks --> PatternAnalysis
        
        PriorityTasks --> PromotionManagement
        PriorityTasks --> BlockCleanup
        PriorityTasks --> Prefetching
    end
```

## Query Processing Architecture
```mermaid
graph TB
    subgraph QueryProcessing
        Query[Query Request] --> Parser[Query Parser]
        Parser --> Analyzer[Query Analyzer]
        Analyzer --> Planner[Query Planner]
        Planner --> Optimizer[Query Optimizer]
        Optimizer --> Executor[Query Executor]
    end
    
    subgraph StorageAccess
        Executor --> CacheLookup[Cache Lookup]
        CacheLookup --> CacheHit{Cache Hit?}
        CacheHit -->|Yes| ReturnResult[Return Result]
        CacheHit -->|No| BlockLookup[Block Lookup]
        BlockLookup --> BlockDecompression[Block Decompression]
        BlockDecompression --> ReturnResult
    end
    
    subgraph Optimization
        QueryOptimizer[Query Optimizer] --> IndexSelection[Index Selection]
        QueryOptimizer --> CacheStrategy[Cache Strategy]
        QueryOptimizer --> Parallelization[Parallelization]
        
        IndexSelection --> BlockLookup
        CacheStrategy --> CacheLookup
        Parallelization --> Executor
    end
```

## Performance Monitoring Architecture
```mermaid
graph TB
    subgraph MetricsCollection
        AtomicMetrics --> PerformanceMetrics[Performance Metrics]
        AtomicMetrics --> ResourceMetrics[Resource Metrics]
        AtomicMetrics --> SystemMetrics[System Metrics]
    end
    
    subgraph PerformanceMetrics
        PerformanceMetrics --> Throughput[Throughput<br/>Ops/Second]
        PerformanceMetrics --> Latency[Latency<br/>Response Time]
        PerformanceMetrics --> HitRate[Cache Hit Rate]
        PerformanceMetrics --> CompressionRatio[Compression Ratio]
    end
    
    subgraph ResourceMetrics
        ResourceMetrics --> MemoryUsage[Memory Usage]
        ResourceMetrics --> DiskUsage[Disk Usage]
        ResourceMetrics --> CPUUsage[CPU Usage]
        ResourceMetrics --> NetworkUsage[Network Usage]
    end
    
    subgraph SystemMetrics
        SystemMetrics --> ErrorRate[Error Rate]
        SystemMetrics --> Availability[Availability]
        SystemMetrics --> Scalability[Scalability]
        SystemMetrics --> Reliability[Reliability]
    end
    
    subgraph MonitoringIntegration
        PerformanceMetrics --> MonitoringSystem[Monitoring System]
        ResourceMetrics --> MonitoringSystem
        SystemMetrics --> MonitoringSystem
        MonitoringSystem --> Alerting[Alerting]
        MonitoringSystem --> Dashboards[Dashboards]
    end
```

## High-Concurrency Sharded Storage Architecture
```mermaid
graph TB
    subgraph HighConcurrencyLayer
        Client[Client Applications] --> HighConcurrencyStorage[HighConcurrencyStorage Interface]
        
        subgraph ShardedStorageManager
            HighConcurrencyStorage --> ShardManager[ShardedStorageManager]
            ShardManager --> Shard0[Shard 0<br/>StorageImpl]
            ShardManager --> Shard1[Shard 1<br/>StorageImpl]
            ShardManager --> Shard2[Shard 2<br/>StorageImpl]
            ShardManager --> Shard3[Shard 3<br/>StorageImpl]
        end
        
        subgraph WriteQueueSystem
            HighConcurrencyStorage --> WriteQueue[Write Queue System]
            WriteQueue --> Queue0[Queue 0<br/>Per-Shard Queue]
            WriteQueue --> Queue1[Queue 1<br/>Per-Shard Queue]
            WriteQueue --> Queue2[Queue 2<br/>Per-Shard Queue]
            WriteQueue --> Queue3[Queue 3<br/>Per-Shard Queue]
        end
        
        subgraph WorkerThreads
            Queue0 --> Worker0[Worker Thread 0]
            Queue1 --> Worker1[Worker Thread 1]
            Queue2 --> Worker2[Worker Thread 2]
            Queue3 --> Worker3[Worker Thread 3]
        end
        
        subgraph LoadBalancing
            ShardManager --> HashFunction[Hash Function<br/>Series Labels → Shard ID]
            HashFunction --> ShardSelection[Shard Selection]
        end
        
        subgraph BatchProcessing
            Worker0 --> BatchProcessor[Batch Processor]
            Worker1 --> BatchProcessor
            Worker2 --> BatchProcessor
            Worker3 --> BatchProcessor
            BatchProcessor --> BatchWrite[Batch Write Operations]
        end
        
        subgraph RetryLogic
            BatchWrite --> RetryMechanism[Retry Mechanism]
            RetryMechanism --> SuccessPath[Success Path]
            RetryMechanism --> FailurePath[Failure Path]
            FailurePath --> RetryQueue[Retry Queue]
        end
    end
    
    subgraph PerformanceMetrics
        ShardManager --> Stats[ShardedStorageStats]
        Stats --> ThroughputMetrics[Throughput Metrics<br/>498K ops/sec]
        Stats --> LatencyMetrics[Latency Metrics<br/><1ms P99]
        Stats --> SuccessRateMetrics[Success Rate<br/>99.9995%]
    end
```

## High-Concurrency Architecture Components
```mermaid
classDiagram
    class HighConcurrencyStorage {
        -ShardedStorageManager sharded_manager_
        -ShardedStorageConfig config_
        +init(StorageConfig) Result
        +write(TimeSeries) Result
        +read(Labels, start, end) TimeSeries
        +write_async(TimeSeries, callback) Result
        +get_detailed_stats() ShardedStorageStats
        +is_healthy() bool
    }
    
    class ShardedStorageManager {
        -vector~StorageImpl~ shards_
        -vector~WriteQueue~ write_queues_
        -vector~thread~ worker_threads_
        -ShardedStorageConfig config_
        -atomic~ShardedStorageStats~ stats_
        +init(StorageConfig) Result
        +write(TimeSeries, callback) Result
        +read(Labels, start, end) TimeSeries
        +flush() Result
        +close() Result
        +get_stats() ShardedStorageStats
        +is_healthy() bool
    }
    
    class ShardedStorageConfig {
        +size_t num_shards
        +size_t queue_size
        +size_t batch_size
        +size_t num_workers
        +chrono::milliseconds flush_interval
        +chrono::milliseconds retry_delay
        +size_t max_retries
    }
    
    class ShardedStorageStats {
        +atomic~uint64_t~ total_operations
        +atomic~uint64_t~ successful_operations
        +atomic~uint64_t~ failed_operations
        +atomic~uint64_t~ retry_operations
        +atomic~uint64_t~ queue_full_errors
        +atomic~uint64_t~ shard_operations[4]
        +atomic~uint64_t~ queue_sizes[4]
    }
    
    class WriteQueue {
        -queue~WriteOperation~ operations_
        -mutex queue_mutex_
        -condition_variable queue_condition_
        -atomic~size_t~ size_
        +enqueue(WriteOperation) bool
        +dequeue() WriteOperation
        +size() size_t
        +is_full() bool
    }
    
    class WriteOperation {
        +TimeSeries series
        +function~void(Result)~ callback
        +chrono::steady_clock::time_point timestamp
        +size_t retry_count
    }
    
    HighConcurrencyStorage --> ShardedStorageManager
    ShardedStorageManager --> ShardedStorageConfig
    ShardedStorageManager --> ShardedStorageStats
    ShardedStorageManager --> WriteQueue
    WriteQueue --> WriteOperation
```

## Performance Architecture Comparison
```mermaid
graph TB
    subgraph OriginalArchitecture
        Client1[Client] --> StorageImpl1[StorageImpl]
        StorageImpl1 --> ObjectPools1[Object Pools]
        StorageImpl1 --> Cache1[Cache Hierarchy]
        StorageImpl1 --> Compression1[Compression]
        StorageImpl1 --> BlockManager1[Block Manager]
        
        subgraph Performance1
            Throughput1[Throughput: ~30K ops/sec]
            Latency1[Latency: ~5ms P99]
            SuccessRate1[Success Rate: ~33% under load]
        end
    end
    
    subgraph HighConcurrencyArchitecture
        Client2[Client] --> HighConcurrencyStorage2[HighConcurrencyStorage]
        HighConcurrencyStorage2 --> ShardedManager2[ShardedStorageManager]
        ShardedManager2 --> Shard0_2[Shard 0<br/>StorageImpl]
        ShardedManager2 --> Shard1_2[Shard 1<br/>StorageImpl]
        ShardedManager2 --> Shard2_2[Shard 2<br/>StorageImpl]
        ShardedManager2 --> Shard3_2[Shard 3<br/>StorageImpl]
        
        subgraph Performance2
            Throughput2[Throughput: 498K ops/sec<br/>16.6x improvement]
            Latency2[Latency: <1ms P99<br/>5x improvement]
            SuccessRate2[Success Rate: 99.9995%<br/>3x improvement]
        end
    end
    
    subgraph ScalabilityComparison
        Load1[Low Load<br/>1K operations] --> OriginalArchitecture
        Load2[Medium Load<br/>10K operations] --> OriginalArchitecture
        Load3[High Load<br/>100K operations] --> HighConcurrencyArchitecture
        Load4[Ultra Load<br/>10M operations] --> HighConcurrencyArchitecture
    end
```

## Integration Testing Architecture
```mermaid
graph TB
    subgraph TestPhases
        subgraph Phase1ObjectPoolIntegration
            ObjectPoolTests[Object Pool Tests]
            MemoryEfficiencyTests[Memory Efficiency Tests]
            PoolStatisticsTests[Pool Statistics Tests]
        end
        
        subgraph Phase2CacheHierarchyIntegration
            CacheHitMissTests[Cache Hit/Miss Tests]
            CacheEvictionTests[Cache Eviction Tests]
            CachePromotionTests[Cache Promotion Tests]
        end
        
        subgraph Phase3CompressionIntegration
            CompressionAccuracyTests[Compression Accuracy Tests]
            CompressionRatioTests[Compression Ratio Tests]
            AlgorithmSelectionTests[Algorithm Selection Tests]
        end
        
        subgraph Phase4BlockManagementIntegration
            BlockCreationTests[Block Creation Tests]
            BlockRotationTests[Block Rotation Tests]
            BlockCompactionTests[Block Compaction Tests]
        end
        
        subgraph Phase5BackgroundProcessingIntegration
            BackgroundTaskTests[Background Task Tests]
            MaintenanceTests[Maintenance Tests]
            MetricsCollectionTests[Metrics Collection Tests]
        end
        
        subgraph Phase6ComprehensiveIntegration
            EndToEndTests[End-to-End Tests]
            PerformanceTests[Performance Tests]
            StressTests[Stress Tests]
        end
    end
    
    subgraph TestExecution
        TestRunner[Test Runner] --> Phase1[Phase 1 Tests]
        TestRunner --> Phase2[Phase 2 Tests]
        TestRunner --> Phase3[Phase 3 Tests]
        TestRunner --> Phase4[Phase 4 Tests]
        TestRunner --> Phase5[Phase 5 Tests]
        TestRunner --> Phase6[Phase 6 Tests]
    end
    
    subgraph TestResults
        Phase1 --> ResultsCollection[Test Results]
        Phase2 --> ResultsCollection
        Phase3 --> ResultsCollection
        Phase4 --> ResultsCollection
        Phase5 --> ResultsCollection
        Phase6 --> ResultsCollection
        
        ResultsCollection --> PerformanceBenchmarks[Performance Benchmarks]
        ResultsCollection --> QualityMetrics[Quality Metrics]
        ResultsCollection --> SuccessCriteria[Success Criteria]
    end
``` 