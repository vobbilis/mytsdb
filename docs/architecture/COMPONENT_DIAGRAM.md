# TSDB Component Architecture Diagram

## 🏗️ **System Component Overview**

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              TSDB SYSTEM ARCHITECTURE                           │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CLIENT LAYER                                      │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │   HTTP      │  │   gRPC      │  │  Prometheus │  │ OpenTelemetry│           │
│  │   Client    │  │   Client    │  │   Client    │  │   Client    │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              API GATEWAY LAYER                                 │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │   HTTP      │  │   gRPC      │  │  Prometheus │  │ OpenTelemetry│           │
│  │   Server    │  │   Service   │  │   API       │  │   Bridge    │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              QUERY LAYER                                       │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  PromQL     │  │   Query     │  │   Query     │  │   Query     │           │
│  │  Engine     │  │  Planner    │  │  Optimizer  │  │  Executor   │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CORE LAYER                                        │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  TimeSeries │  │   Labels    │  │   Sample    │  │   Metric    │           │
│  │  Manager    │  │   Manager   │  │   Manager   │  │   Manager   │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
│                                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Database   │  │  Metric     │  │  Result<T>  │  │  Config     │           │
│  │  Interface  │  │  Family     │  │  Template   │  │  Manager    │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           ADVANCED PERFORMANCE LAYER (AdvPerf)                 │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Cache      │  │  Object     │  │  Predictive │  │  Background │           │
│  │ Hierarchy   │  │  Pooling    │  │   Cache     │  │  Processor  │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
│                                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Sharded    │  │  Sync       │  │  Atomic     │  │ Performance │           │
│  │  WAL        │  │  Maps       │  │  Metrics    │  │   Config    │           │
│  │             │  │             │  │             │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              STORAGE LAYER                                     │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Block      │  │  Series     │  │  File       │  │  Index      │           │
│  │  Manager    │  │  Manager    │  │  Manager    │  │  Manager    │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
│                                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  Adaptive   │  │  Delta-of-  │  │  Memory     │  │  Working    │           │
│  │ Compressor  │  │  Delta      │  │  Mapped     │  │  Set Cache  │           │
│  │             │  │  Encoder    │  │  Cache      │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              PERSISTENCE LAYER                                 │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  File       │  │  Block      │  │  WAL        │  │  Snapshot   │           │
│  │  System     │  │  Storage    │  │  (Write-    │  │  Manager    │           │
│  │             │  │             │  │  Ahead Log) │  │             │           │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## 🔄 **Component Interaction Flow**

### **Write Operation Flow**
```
1. Client Request (HTTP/gRPC)
   ↓
2. API Gateway (Authentication, Validation)
   ↓
3. OpenTelemetry Bridge (Format Conversion)
   ↓
4. Core Layer (Data Model Creation)
   ↓
5. AdvPerf Layer (Object Pooling, Caching)
   ↓
6. Storage Layer (Block Management)
   ↓
7. Persistence Layer (File System)
   ↓
8. Background Processing (Compression, Indexing)
```

### **Read Operation Flow**
```
1. Query Request (PromQL/HTTP)
   ↓
2. Query Engine (Parsing, Planning)
   ↓
3. Cache Hierarchy (L1 → L2 → L3)
   ↓
4. Storage Layer (Block Retrieval)
   ↓
5. Decompression (Adaptive, Delta-of-Delta)
   ↓
6. Core Layer (Data Assembly)
   ↓
7. Response (JSON/Protocol Buffers)
```

## 📊 **Detailed Component Descriptions**

### **API Gateway Layer**

#### **HTTP Server**
- **Purpose**: RESTful API endpoints for Prometheus compatibility
- **Components**: 
  - Query API (`/api/v1/query`)
  - Write API (`/api/v1/write`)
  - Labels API (`/api/v1/labels`)
  - Series API (`/api/v1/series`)
- **Features**: JSON responses, error handling, rate limiting

#### **gRPC Service**
- **Purpose**: High-performance RPC for OpenTelemetry integration
- **Components**:
  - Metrics Service (OTEL v1)
  - Trace Service (OTEL v1)
  - Logs Service (OTEL v1)
- **Features**: Protocol Buffers, streaming, bidirectional communication

#### **OpenTelemetry Bridge**
- **Purpose**: Convert OTEL metrics to internal format
- **Components**:
  - Metric Converter
  - Resource Attribute Processor
  - Label Mapper
- **Features**: Batch processing, real-time conversion, error handling

### **Query Layer**

#### **PromQL Engine**
- **Purpose**: Parse and execute PromQL queries
- **Components**:
  - Lexer (Tokenization)
  - Parser (AST Generation)
  - Evaluator (Query Execution)
- **Features**: Vector selectors, aggregations, functions, subqueries

#### **Query Planner**
- **Purpose**: Optimize query execution plans
- **Components**:
  - Cost-based optimizer
  - Index selection
  - Parallel execution planning
- **Features**: Query rewriting, index hints, execution statistics

### **Core Layer**

#### **TimeSeries Manager**
- **Purpose**: Manage time series data structures
- **Components**:
  - Series creation and deletion
  - Sample addition and retrieval
  - Metadata management
- **Features**: Thread-safe operations, memory optimization

#### **Labels Manager**
- **Purpose**: Efficient label management
- **Components**:
  - Label set operations
  - Label value indexing
  - Label cardinality tracking
- **Features**: Unicode support, case sensitivity, memory pooling

#### **Configuration Manager**
- **Purpose**: Runtime configuration management
- **Components**:
  - Configuration validation
  - Hot reloading
  - Feature flags
- **Features**: JSON configuration, environment variables, validation

### **Advanced Performance Layer (AdvPerf)**

#### **Cache Hierarchy**
- **Purpose**: Multi-level caching for optimal performance
- **Components**:
  - L1 Cache (WorkingSetCache)
  - L2 Cache (MemoryMappedCache)
  - L3 Cache (Disk Storage)
  - Predictive Cache
- **Features**: LRU eviction, hit ratio tracking, automatic promotion/demotion

#### **Object Pooling**
- **Purpose**: Reduce memory allocation overhead
- **Components**:
  - TimeSeriesPool
  - LabelsPool
  - SamplePool
- **Features**: 99% memory reduction, thread-safe operations, configurable sizes

#### **Sharded WAL**
- **Purpose**: Low-latency persistence
- **Components**:
  - Multiple WAL shards
  - Async persistence queues
  - Background flusher
- **Features**: Reduced contention, decoupled I/O latency

#### **Background Processor**
- **Purpose**: Asynchronous task processing
- **Components**:
  - Worker thread pool
  - Priority queue
  - Task scheduler
- **Features**: Non-blocking operations, graceful shutdown, task prioritization

### **Storage Layer**

#### **Block Manager**
- **Purpose**: Manage data blocks and lifecycle
- **Components**:
  - Block creation and writing
  - Block reading and decompression
  - Block compaction and deletion
- **Features**: Multi-tier storage, automatic compaction, block rotation

#### **Series Manager**
- **Purpose**: Efficient series operations
- **Components**:
  - Series creation and lookup
  - Series indexing
  - Series metadata management
- **Features**: Fast lookups, memory efficiency, concurrent access

#### **Compression Engine**
- **Purpose**: Data compression for storage efficiency
- **Components**:
  - Adaptive Compressor
  - Delta-of-Delta Encoder
  - Type-aware compression
- **Features**: 20-60% compression ratio, multiple algorithms, automatic selection

### **Persistence Layer**

#### **File System Manager**
- **Purpose**: Low-level file operations
- **Components**:
  - File creation and deletion
  - Memory mapping
  - I/O optimization
- **Features**: Async I/O, memory mapping, error handling

#### **Write-Ahead Log (WAL)**
- **Purpose**: Durability and recovery
- **Components**:
  - Log writing
  - Log replay
  - Log truncation
- **Features**: Crash recovery, durability guarantees, performance optimization

## 🔗 **Component Dependencies**

### **Direct Dependencies**
```
API Gateway → Query Layer → Core Layer → AdvPerf Layer → Storage Layer → Persistence Layer
```

### **Cross-Layer Dependencies**
```
Cache Hierarchy ↔ Storage Layer (Data access)
Background Processor ↔ Storage Layer (Compaction tasks)
Object Pooling ↔ Core Layer (Memory management)
Performance Config ↔ All Layers (Feature flags)
```

### **Interface Dependencies**
```
ITimeSeries ← TimeSeries Manager
IStorage ← Storage Engine
ICache ← Cache Hierarchy
ICompressor ← Compression Engine
```

## 📈 **Performance Characteristics**

### **Throughput by Component**
- **API Gateway**: High-throughput non-blocking I/O
- **Query Engine**: Vectorized execution
- **Storage Engine**: Concurrent write path
- **Cache Hierarchy**: High hit ratio design
- **Background Processor**: Asynchronous maintenance

### **Memory Usage by Component**
- **Object Pooling**: Significant allocation reduction
- **Cache Hierarchy**: Efficient working set management
- **Compression**: Tiered compression strategy
- **Working Set**: LRU-based memory limits

---

*This component architecture provides a comprehensive view of the TSDB system, showing how all components interact to deliver high-performance time series database functionality with advanced performance optimizations.* 