# TSDB Data Flow Architecture

## 🔄 **Data Flow Overview**

The TSDB data flow architecture describes how data moves through the system for different operations, including write operations, read operations, and background processing tasks.

## 📝 **Write Operation Data Flow**

### **1. Client Write Request**
```
┌─────────────────┐
│   Client        │
│   (HTTP/gRPC)   │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│   API Gateway   │
│   (Validation)  │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  OTEL Bridge    │
│  (Conversion)   │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│   Core Layer    │
│  (Data Model)   │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  AdvPerf Layer  │
│  (Optimization) │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Storage Layer  │
│  (Block Mgmt)   │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│ Persistence     │
│  (File System)  │
└─────────────────┘
```

### **Detailed Write Flow**

#### **Step 1: Client Request Processing**
```
Input: HTTP POST /api/v1/write or gRPC MetricsService.Export
↓
Authentication & Authorization
↓
Request Validation (Content-Type, Size Limits)
↓
Rate Limiting Check
↓
Output: Validated Request Object
```

#### **Step 2: OpenTelemetry Bridge Processing**
```
Input: OTEL Metrics Data
↓
Protocol Buffer Deserialization
↓
Metric Type Detection (Counter, Gauge, Histogram, Summary)
↓
Resource Attributes Processing
↓
Label Mapping & Normalization
↓
Output: Internal Metric Format
```

#### **Step 3: Core Data Model Creation**
```
Input: Internal Metric Format
↓
TimeSeries Object Creation
↓
Labels Object Creation (with pooling)
↓
Sample Object Creation (with pooling)
↓
Metadata Assignment
↓
Output: TimeSeries with Samples
```

#### **Step 4: Advanced Performance Optimization**
```
Input: TimeSeries with Samples
↓
Object Pooling (TimeSeries, Labels, Sample reuse)
↓
Cache Hierarchy Update (L1 cache insertion)
↓
Predictive Cache Pattern Recording
↓
Sharded Write Buffer Distribution
↓
Output: Optimized TimeSeries Data
```

#### **Step 5: Storage Processing**
```
Input: Optimized TimeSeries Data
↓
Block Manager Selection (Hot/Warm/Cold)
↓
Series Manager Lookup/Creation
↓
Block Writing (with compression)
↓
Index Update
↓
Output: Stored Block Reference
```

#### **Step 6: Persistence**
```
Input: Block Data
↓
Write-Ahead Log (WAL) Entry
↓
File System Write
↓
Memory Mapping Update
↓
Durability Confirmation
↓
Output: Persistent Storage Confirmation
```

## 📖 **Read Operation Data Flow**

### **1. Client Read Request**
```
┌─────────────────┐
│   Client        │
│   (PromQL/HTTP) │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Query Engine   │
│   (Parsing)     │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Cache Hierarchy│
│   (L1→L2→L3)    │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Storage Layer  │
│  (Block Read)   │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Decompression  │
│   (Adaptive)    │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Response       │
│  (JSON/PB)      │
└─────────────────┘
```

### **Detailed Read Flow**

#### **Step 1: Query Processing**
```
Input: PromQL Query String
↓
Lexical Analysis (Tokenization)
↓
Syntax Analysis (AST Generation)
↓
Semantic Analysis (Type Checking)
↓
Query Planning (Optimization)
↓
Output: Execution Plan
```

#### **Step 2: Cache Lookup**
```
Input: Execution Plan
↓
L1 Cache Check (WorkingSetCache)
↓
L2 Cache Check (MemoryMappedCache)
↓
L3 Cache Check (Disk Storage)
↓
Predictive Cache Prefetch
↓
Output: Cached Data or Cache Miss
```

#### **Step 3: Storage Retrieval**
```
Input: Cache Miss + Query Plan
↓
Block Manager Lookup
↓
Series Manager Query
↓
Block Reading (with decompression)
↓
Data Assembly
↓
Output: Raw TimeSeries Data
```

#### **Step 4: Data Processing**
```
Input: Raw TimeSeries Data
↓
Delta-of-Delta Decoding
↓
Adaptive Decompression
↓
Data Validation
↓
Result Assembly
↓
Output: Processed TimeSeries Data
```

#### **Step 5: Response Generation**
```
Input: Processed TimeSeries Data
↓
Query Execution (Aggregations, Functions)
↓
Result Formatting (JSON/Protocol Buffers)
↓
Response Serialization
↓
HTTP/gRPC Response
↓
Output: Client Response
```

## 🔄 **Background Processing Data Flow**

### **1. Background Task Pipeline**
```
┌─────────────────┐
│  Background     │
│  Processor      │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Task Queue     │
│  (Priority)     │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Worker Pool    │
│  (Multi-thread) │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Task Execution │
│  (Compression)  │
└─────────┬───────┘
          │
          ▼
┌─────────────────┐
│  Result Update  │
│  (Storage)      │
└─────────────────┘
```

### **Background Task Types**

#### **Compression Tasks**
```
Input: Raw Block Data
↓
Type Detection (Counter, Gauge, Histogram)
↓
Compression Algorithm Selection
↓
Delta-of-Delta Encoding
↓
Adaptive Compression
↓
Output: Compressed Block
```

#### **Index Update Tasks**
```
Input: New TimeSeries Data
↓
Label Index Update
↓
Series Index Update
↓
Block Index Update
↓
Metadata Update
↓
Output: Updated Indexes
```

#### **Cache Maintenance Tasks**
```
Input: Cache Statistics
↓
LRU Eviction (if needed)
↓
Cache Promotion/Demotion
↓
Predictive Cache Update
↓
Memory Cleanup
↓
Output: Optimized Cache State
```

#### **Block Compaction Tasks**
```
Input: Multiple Small Blocks
↓
Block Selection (Hot/Warm/Cold)
↓
Data Merging
↓
Compression Optimization
↓
Old Block Cleanup
↓
Output: Consolidated Block
```

## 📊 **Data Transformation Pipeline**

### **1. Data Format Transformations**

#### **OTEL to Internal Format**
```
OTEL Metric
├── Resource Attributes → Labels
├── Metric Name → Series Name
├── Metric Type → Internal Type
├── Data Points → Samples
└── Timestamps → Internal Timestamps
```

#### **Internal to Storage Format**
```
Internal TimeSeries
├── Labels → Compressed Labels
├── Samples → Compressed Samples
├── Metadata → Block Header
└── Indexes → Block Indexes
```

#### **Storage to Response Format**
```
Stored Block
├── Block Header → Metadata
├── Compressed Data → Decompressed Data
├── Indexes → Query Results
└── Statistics → Response Headers
```

### **2. Compression Pipeline**

#### **Timestamp Compression**
```
Raw Timestamps
↓
Delta Calculation
↓
Delta-of-Delta Calculation
↓
Zigzag Encoding
↓
Variable-Length Encoding
↓
Compressed Timestamps
```

#### **Value Compression**
```
Raw Values
↓
Type Detection
↓
Pattern Analysis
↓
Compression Algorithm Selection
↓
Type-Specific Compression
↓
Compressed Values
```

#### **Label Compression**
```
Raw Labels
↓
Dictionary Building
↓
String Deduplication
↓
Huffman Encoding
↓
Label Compression
↓
Compressed Labels
```

## 🔄 **Concurrent Data Flow**

### **1. Sharded Write Processing**
```
Input Stream
├── Shard 1 → Write Buffer 1 → Background Flush
├── Shard 2 → Write Buffer 2 → Background Flush
├── Shard 3 → Write Buffer 3 → Background Flush
└── Shard N → Write Buffer N → Background Flush
```

### **2. Parallel Query Processing**
```
Query Request
├── Subquery 1 → Worker Thread 1 → Cache/Storage
├── Subquery 2 → Worker Thread 2 → Cache/Storage
├── Subquery 3 → Worker Thread 3 → Cache/Storage
└── Result Aggregation → Response
```

### **3. Background Task Parallelism**
```
Task Queue
├── Compression Task → Worker 1 → Storage Update
├── Index Task → Worker 2 → Index Update
├── Cache Task → Worker 3 → Cache Update
└── Compaction Task → Worker 4 → Block Update
```

## 📈 **Performance Data Flow**

### **1. Metrics Collection**
```
System Components
├── API Gateway → Request Metrics
├── Storage Engine → I/O Metrics
├── Cache Hierarchy → Hit/Miss Metrics
├── Background Processor → Task Metrics
└── Atomic Metrics → Performance Statistics
```

### **2. Performance Monitoring**
```
Performance Data
├── Throughput Metrics → Real-time Monitoring
├── Latency Metrics → Alerting System
├── Resource Metrics → Capacity Planning
└── Error Metrics → Error Tracking
```

## 🔒 **Security Data Flow**

### **1. Authentication Flow**
```
Client Request
↓
API Gateway Authentication
↓
Token Validation
↓
Permission Check
↓
Request Processing
↓
Audit Logging
```

### **2. Data Validation Flow**
```
Input Data
↓
Schema Validation
↓
Content Validation
↓
Size Limits Check
↓
Malicious Content Detection
↓
Sanitized Data
```

## 📊 **Data Flow Metrics**

### **Throughput by Operation**
- **Write Operations**: Optimized for high throughput
- **Read Operations**: Optimized for query latency
- **Background Tasks**: Scheduled task execution
- **Cache Operations**: High-frequency access

### **Latency by Stage**
- **API Gateway**: Minimal overhead
- **Query Processing**: Parse and plan time
- **Cache Lookup**: In-memory access speed
- **Storage I/O**: Block read latency
- **Background Processing**: Asynchronous execution

### **Data Volume by Stage**
- **Input Data**: Raw OTEL metrics
- **Processed Data**: Internal format
- **Compressed Data**: Storage format
- **Response Data**: JSON/Protocol Buffers

---

*This data flow architecture provides a comprehensive view of how data moves through the TSDB system, from initial client requests through processing, storage, and response generation, with detailed information about each transformation stage and performance characteristics.* 