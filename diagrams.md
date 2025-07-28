# TSDB Architecture Diagrams

**Updated**: July 2025  
**Status**: Comprehensive architecture reflecting all implemented features and integration plans

## System Architecture Overview
```mermaid
graph TB
    Client[Client Applications] --> GRPC[gRPC Interface]
    OTEL[OpenTelemetry] --> GRPC
    Prometheus[Prometheus] --> GRPC
    
    subgraph Ingestion Layer
        GRPC --> Parser[Protocol Parser]
        Parser --> Validator[Validation Layer]
        Validator --> Router[Metric Router]
        Router --> StorageImpl[StorageImpl Interface]
    end
    
    subgraph Advanced Storage Engine
        StorageImpl --> ObjectPools[Object Pools]
        StorageImpl --> CacheHierarchy[Cache Hierarchy]
        StorageImpl --> Compression[Compression Engine]
        StorageImpl --> BlockManager[Block Manager]
        StorageImpl --> BackgroundProc[Background Processor]
        StorageImpl --> PredictiveCache[Predictive Cache]
    end
    
    subgraph Multi-Tier Storage
        BlockManager --> HotStorage[Hot Storage]
        BlockManager --> WarmStorage[Warm Storage]
        BlockManager --> ColdStorage[Cold Storage]
        
        subgraph HistogramStorage
            HotStorage --> HBuckets[Hot Buckets]
            WarmStorage --> WBuckets[Warm Buckets]
            ColdStorage --> CBuckets[Cold Buckets]
        end
    end
    
    subgraph Query Engine
        Query[Query Processor] --> PromQL[PromQL Engine]
        PromQL --> Planner[Query Planner]
        Planner --> Executor[Query Executor]
        Executor --> CacheHierarchy
        Executor --> BlockManager
    end
    
    subgraph Monitoring & Metrics
        AtomicMetrics[Atomic Metrics] --> StorageImpl
        PerformanceConfig[Performance Config] --> StorageImpl
        BackgroundProc --> MetricsCollection[Metrics Collection]
    end
```

## StorageImpl Integration Architecture
```mermaid
graph TB
    subgraph StorageImpl Interface
        WriteOp[Write Operations] --> ObjectPoolIntegration[Object Pool Integration]
        ReadOp[Read Operations] --> CacheIntegration[Cache Integration]
        QueryOp[Query Operations] --> CompressionIntegration[Compression Integration]
    end
    
    subgraph Object Pool Layer
        ObjectPoolIntegration --> TimeSeriesPool[TimeSeries Pool]
        ObjectPoolIntegration --> LabelsPool[Labels Pool]
        ObjectPoolIntegration --> SamplePool[Sample Pool]
    end
    
    subgraph Cache Hierarchy Layer
        CacheIntegration --> L1Cache[L1 Cache<br/>WorkingSetCache]
        CacheIntegration --> L2Cache[L2 Cache<br/>MemoryMappedCache]
        CacheIntegration --> L3Cache[L3 Cache<br/>Disk Storage]
        L1Cache --> L2Cache
        L2Cache --> L3Cache
    end
    
    subgraph Compression Layer
        CompressionIntegration --> TimestampComp[Timestamp Compression<br/>Delta-of-Delta]
        CompressionIntegration --> ValueComp[Value Compression<br/>XOR/RLE]
        CompressionIntegration --> LabelComp[Label Compression<br/>RLE]
    end
    
    subgraph Block Management Layer
        BlockManager --> BlockCreation[Block Creation]
        BlockManager --> BlockRotation[Block Rotation]
        BlockManager --> BlockCompaction[Block Compaction]
        BlockManager --> BlockIndexing[Block Indexing]
    end
    
    subgraph Background Processing
        BackgroundProc --> CacheMaintenance[Cache Maintenance]
        BackgroundProc --> BlockMaintenance[Block Maintenance]
        BackgroundProc --> MetricsCollection[Metrics Collection]
        BackgroundProc --> PredictiveAnalysis[Predictive Analysis]
    end
```

## Advanced Features Architecture
```mermaid
graph TB
    subgraph AdvPerf Features
        ObjectPooling[Object Pooling<br/>Memory Efficiency]
        MultiLevelCache[Multi-Level Cache<br/>Performance Optimization]
        PredictiveCaching[Predictive Caching<br/>Access Pattern Analysis]
        AdaptiveCompression[Adaptive Compression<br/>Multiple Algorithms]
        BackgroundProcessing[Background Processing<br/>Automatic Maintenance]
        AtomicReferenceCounting[Atomic Reference Counting<br/>Memory Management]
    end
    
    subgraph Performance Optimization
        ObjectPooling --> MemoryEfficiency[Memory Efficiency]
        MultiLevelCache --> AccessSpeed[Access Speed]
        PredictiveCaching --> Prefetching[Intelligent Prefetching]
        AdaptiveCompression --> StorageEfficiency[Storage Efficiency]
        BackgroundProcessing --> SystemOptimization[System Optimization]
        AtomicReferenceCounting --> SafeMemory[Safe Memory Management]
    end
    
    subgraph Integration Points
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
    subgraph Storage Tiers
        subgraph Hot Tier
            HotMemory[In-Memory Storage]
            HotCache[L1 Cache<br/>WorkingSetCache]
            HotBlocks[Hot Blocks<br/>Recent Data]
        end
        
        subgraph Warm Tier
            WarmMemory[Memory-Mapped Storage]
            WarmCache[L2 Cache<br/>MemoryMappedCache]
            WarmBlocks[Warm Blocks<br/>Compressed Data]
        end
        
        subgraph Cold Tier
            ColdDisk[Disk Storage]
            ColdCache[L3 Cache<br/>Persistent Storage]
            ColdBlocks[Cold Blocks<br/>Archived Data]
        end
    end
    
    subgraph Data Flow
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
    
    subgraph Background Operations
        BackgroundProc --> TierPromotion[Promotion<br/>Cold → Warm → Hot]
        BackgroundProc --> TierDemotion[Demotion<br/>Hot → Warm → Cold]
        BackgroundProc --> Compaction[Block Compaction]
        BackgroundProc --> Cleanup[Data Cleanup]
    end
```

## Compression Architecture
```mermaid
graph TB
    subgraph Compression Algorithms
        subgraph Timestamp Compression
            DeltaOfDelta[Delta-of-Delta<br/>High Compression]
            SimpleDelta[Simple Delta<br/>Fast Processing]
            XOR[XOR Compression<br/>Efficient for Timestamps]
        end
        
        subgraph Value Compression
            XORValue[XOR Compression<br/>Floating Point]
            RLEValue[RLE Compression<br/>Repeated Values]
            AdaptiveValue[Adaptive Compression<br/>Auto Selection]
        end
        
        subgraph Label Compression
            RLELabel[RLE Compression<br/>String Labels]
            DictionaryLabel[Dictionary Compression<br/>Label Sets]
        end
    end
    
    subgraph Compression Pipeline
        InputData[Input Data] --> AlgorithmSelector[Algorithm Selector]
        AlgorithmSelector --> TimestampComp[Timestamp Compression]
        AlgorithmSelector --> ValueComp[Value Compression]
        AlgorithmSelector --> LabelComp[Label Compression]
        
        TimestampComp --> CompressedData[Compressed Data]
        ValueComp --> CompressedData
        LabelComp --> CompressedData
        
        CompressedData --> BlockStorage[Block Storage]
    end
    
    subgraph Performance Monitoring
        CompressionRatio[Compression Ratio] --> AlgorithmSelector
        CompressionSpeed[Compression Speed] --> AlgorithmSelector
        DecompressionSpeed[Decompression Speed] --> AlgorithmSelector
    end
```

## Cache Hierarchy Architecture
```mermaid
graph TB
    subgraph Cache Levels
        subgraph L1 Cache (Fastest)
            L1WorkingSet[Working Set Cache<br/>LRU Eviction]
            L1Memory[In-Memory Storage<br/>~10-100ns Access]
            L1Size[Small Size<br/>~1K Entries]
        end
        
        subgraph L2 Cache (Medium)
            L2MemoryMapped[Memory-Mapped Cache<br/>File-Based]
            L2Medium[Medium Speed<br/>~1-10μs Access]
            L2Size[Medium Size<br/>~10K Entries]
        end
        
        subgraph L3 Cache (Slowest)
            L3Disk[Disk Storage<br/>Persistent]
            L3Slow[Slow Speed<br/>~1-10ms Access]
            L3Size[Large Size<br/>Unlimited]
        end
    end
    
    subgraph Cache Operations
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
    
    subgraph Background Maintenance
        BackgroundProc --> CacheOptimization[Cache Optimization]
        CacheOptimization --> EvictionPolicy[LRU Eviction]
        CacheOptimization --> PromotionPolicy[Access-Based Promotion]
        CacheOptimization --> DemotionPolicy[Time-Based Demotion]
    end
```

## Block Management Architecture
```mermaid
graph TB
    subgraph Block Lifecycle
        BlockCreation[Block Creation] --> BlockWriting[Block Writing]
        BlockWriting --> BlockCompression[Block Compression]
        BlockCompression --> BlockStorage[Block Storage]
        BlockStorage --> BlockRotation[Block Rotation]
        BlockRotation --> BlockCompaction[Block Compaction]
        BlockCompaction --> BlockArchival[Block Archival]
    end
    
    subgraph Block Types
        subgraph Hot Blocks
            HotActive[Active Blocks<br/>Current Writing]
            HotRecent[Recent Blocks<br/>Frequently Accessed]
        end
        
        subgraph Warm Blocks
            WarmCompressed[Compressed Blocks<br/>Medium Access]
            WarmIndexed[Indexed Blocks<br/>Query Optimization]
        end
        
        subgraph Cold Blocks
            ColdArchived[Archived Blocks<br/>Rare Access]
            ColdCompacted[Compacted Blocks<br/>Space Optimized]
        end
    end
    
    subgraph Block Operations
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
    subgraph Background Tasks
        subgraph Cache Maintenance
            CacheOptimization[Cache Optimization]
            EvictionManagement[Eviction Management]
            PromotionManagement[Promotion Management]
        end
        
        subgraph Block Maintenance
            BlockCompaction[Block Compaction]
            BlockRotation[Block Rotation]
            BlockCleanup[Block Cleanup]
        end
        
        subgraph Metrics Collection
            PerformanceMetrics[Performance Metrics]
            ResourceMetrics[Resource Metrics]
            SystemMetrics[System Metrics]
        end
        
        subgraph Predictive Analysis
            PatternAnalysis[Pattern Analysis]
            Prefetching[Intelligent Prefetching]
            ConfidenceScoring[Confidence Scoring]
        end
    end
    
    subgraph Task Scheduling
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
    subgraph Query Processing
        Query[Query Request] --> Parser[Query Parser]
        Parser --> Analyzer[Query Analyzer]
        Analyzer --> Planner[Query Planner]
        Planner --> Optimizer[Query Optimizer]
        Optimizer --> Executor[Query Executor]
    end
    
    subgraph Storage Access
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
    subgraph Metrics Collection
        AtomicMetrics --> PerformanceMetrics[Performance Metrics]
        AtomicMetrics --> ResourceMetrics[Resource Metrics]
        AtomicMetrics --> SystemMetrics[System Metrics]
    end
    
    subgraph Performance Metrics
        PerformanceMetrics --> Throughput[Throughput<br/>Ops/Second]
        PerformanceMetrics --> Latency[Latency<br/>Response Time]
        PerformanceMetrics --> HitRate[Cache Hit Rate]
        PerformanceMetrics --> CompressionRatio[Compression Ratio]
    end
    
    subgraph Resource Metrics
        ResourceMetrics --> MemoryUsage[Memory Usage]
        ResourceMetrics --> DiskUsage[Disk Usage]
        ResourceMetrics --> CPUUsage[CPU Usage]
        ResourceMetrics --> NetworkUsage[Network Usage]
    end
    
    subgraph System Metrics
        SystemMetrics --> ErrorRate[Error Rate]
        SystemMetrics --> Availability[Availability]
        SystemMetrics --> Scalability[Scalability]
        SystemMetrics --> Reliability[Reliability]
    end
    
    subgraph Monitoring Integration
        PerformanceMetrics --> MonitoringSystem[Monitoring System]
        ResourceMetrics --> MonitoringSystem
        SystemMetrics --> MonitoringSystem
        MonitoringSystem --> Alerting[Alerting]
        MonitoringSystem --> Dashboards[Dashboards]
    end
```

## Integration Testing Architecture
```mermaid
graph TB
    subgraph Test Phases
        subgraph Phase 1: Object Pool Integration
            ObjectPoolTests[Object Pool Tests]
            MemoryEfficiencyTests[Memory Efficiency Tests]
            PoolStatisticsTests[Pool Statistics Tests]
        end
        
        subgraph Phase 2: Cache Hierarchy Integration
            CacheHitMissTests[Cache Hit/Miss Tests]
            CacheEvictionTests[Cache Eviction Tests]
            CachePromotionTests[Cache Promotion Tests]
        end
        
        subgraph Phase 3: Compression Integration
            CompressionAccuracyTests[Compression Accuracy Tests]
            CompressionRatioTests[Compression Ratio Tests]
            AlgorithmSelectionTests[Algorithm Selection Tests]
        end
        
        subgraph Phase 4: Block Management Integration
            BlockCreationTests[Block Creation Tests]
            BlockRotationTests[Block Rotation Tests]
            BlockCompactionTests[Block Compaction Tests]
        end
        
        subgraph Phase 5: Background Processing Integration
            BackgroundTaskTests[Background Task Tests]
            MaintenanceTests[Maintenance Tests]
            MetricsCollectionTests[Metrics Collection Tests]
        end
        
        subgraph Phase 6: Comprehensive Integration
            EndToEndTests[End-to-End Tests]
            PerformanceTests[Performance Tests]
            StressTests[Stress Tests]
        end
    end
    
    subgraph Test Execution
        TestRunner[Test Runner] --> Phase1[Phase 1 Tests]
        TestRunner --> Phase2[Phase 2 Tests]
        TestRunner --> Phase3[Phase 3 Tests]
        TestRunner --> Phase4[Phase 4 Tests]
        TestRunner --> Phase5[Phase 5 Tests]
        TestRunner --> Phase6[Phase 6 Tests]
    end
    
    subgraph Test Results
        Phase1 --> TestResults[Test Results]
        Phase2 --> TestResults
        Phase3 --> TestResults
        Phase4 --> TestResults
        Phase5 --> TestResults
        Phase6 --> TestResults
        
        TestResults --> PerformanceBenchmarks[Performance Benchmarks]
        TestResults --> QualityMetrics[Quality Metrics]
        TestResults --> SuccessCriteria[Success Criteria]
    end
``` 