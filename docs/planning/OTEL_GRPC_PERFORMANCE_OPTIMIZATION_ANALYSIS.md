# OTEL/gRPC Performance Optimization Analysis

**Date:** 2025-01-23  
**Last Updated:** 2025-11-23  
**Branch:** `performance-tuning`  
**Status:** Analysis Complete, Ready for Implementation

## 🎯 Optimization Priority Analysis

Based on the performance results showing **18x overhead for single writes**, this document analyzes which optimizations will have the biggest impact.

## 📊 Current Performance Bottlenecks

### Single Write Breakdown (84.3 μs total)
1. **gRPC serialization/deserialization** - Estimated: ~30-40% (25-35 μs)
2. **Protocol buffer encoding/decoding** - Estimated: ~20-25% (17-21 μs)
3. **Network stack overhead** (localhost TCP/IP) - Estimated: ~10-15% (8-13 μs)
4. **OTEL bridge conversion** - Estimated: ~15-20% (13-17 μs)
5. **Request/response handling** - Estimated: ~10-15% (8-13 μs)
6. **Storage write** - Estimated: ~5-10% (4-8 μs) - This is the actual work

### Batch Write Efficiency (197 μs for 64 metrics = 3.1 μs/metric)
- Overhead is **amortized** across 64 metrics
- Per-metric overhead drops from 84.3 μs to 3.1 μs
- **27x improvement** from batching alone

## 🚀 Optimization Opportunities - Prioritized by Impact

### Tier 1: Highest Impact (Biggest Bang for Buck) ⭐⭐⭐

**Note:** Unix Domain Sockets optimization removed - not applicable for network-based metrics ingestion use cases.

#### 1. **Async gRPC Calls (Non-blocking)** ⭐⭐⭐
**Impact:** 🔥🔥🔥 **VERY HIGH**  
**Effort:** 🟡 **MEDIUM** (4-6 hours)  
**Risk:** 🟡 **MEDIUM**

**Why it's high impact:**
- Allows concurrent request processing
- Eliminates blocking on serialization/network I/O
- Can process multiple requests in parallel
- **Expected improvement:** 2-5x throughput improvement for concurrent workloads

**Implementation:**
- Use `grpc::CompletionQueue` for async calls
- Implement async `Export()` method
- Requires refactoring MetricsService

**Estimated gain:** 
- Single writes: 20-30% improvement (16-25 μs saved)
- Concurrent writes: 2-5x throughput improvement

---

#### 2. **Optimize OTEL Bridge Conversion** ⭐⭐⭐
**Impact:** 🔥🔥🔥 **VERY HIGH**  
**Effort:** 🟡 **MEDIUM** (3-5 hours)  
**Risk:** 🟢 **LOW**

**Why it's high impact:**
- Conversion happens for every metric (15-20% of time)
- Current implementation may have unnecessary allocations
- Can batch conversions more efficiently
- **Expected improvement:** 30-50% reduction in conversion time

**Implementation:**
- Profile bridge conversion code
- Reduce allocations (object pools, reuse buffers)
- Optimize label/attribute conversion
- Batch process multiple metrics together

**Estimated gain:** 4-8 μs per single write (5-10% improvement), scales with batch size

---

### Tier 2: High Impact (Good ROI) ⭐⭐

#### 4. **Connection Pooling / Keep-Alive** ⭐⭐
**Impact:** 🔥🔥 **HIGH**  
**Effort:** 🟢 **LOW** (1-2 hours)  
**Risk:** 🟢 **LOW**

**Why it's high impact:**
- Eliminates connection setup overhead
- Reduces TLS handshake (if using TLS)
- Improves latency consistency
- **Expected improvement:** 5-10% for first request, minimal for subsequent

**Implementation:**
- Configure gRPC channel with keep-alive
- Reuse channels across requests
- Already partially implemented in benchmark

**Estimated gain:** 2-5 μs per request (3-6% improvement)

---

#### 5. **Optimize Protobuf Serialization** ⭐⭐
**Impact:** 🔥🔥 **HIGH**  
**Effort:** 🟡 **MEDIUM** (4-8 hours)  
**Risk:** 🟡 **MEDIUM**

**Why it's high impact:**
- Protobuf encoding is 20-25% of total time
- Can use faster serialization libraries
- Can optimize message structure
- **Expected improvement:** 20-30% reduction in serialization time

**Implementation:**
- Profile protobuf serialization
- Consider faster alternatives (FlatBuffers, Cap'n Proto)
- Optimize message structure (remove unused fields)
- Use protobuf arena allocation

**Estimated gain:** 3-6 μs per single write (4-7% improvement)

---

### Tier 3: Medium Impact (Incremental Gains) ⭐

#### 6. **Batch Conversions in Bridge** ⭐
**Impact:** 🔥 **MEDIUM**  
**Effort:** 🟢 **LOW** (2-3 hours)  
**Risk:** 🟢 **LOW**

**Why it's medium impact:**
- Already benefits from batching at gRPC level
- Additional batching in bridge may help
- **Expected improvement:** 5-10% for large batches

**Implementation:**
- Process multiple metrics in single conversion pass
- Reduce per-metric overhead

**Estimated gain:** 1-2 μs per metric in batches (3-5% improvement)

---

#### 7. **Cache Common Conversions** ⭐
**Impact:** 🔥 **MEDIUM**  
**Effort:** 🟡 **MEDIUM** (3-4 hours)  
**Risk:** 🟡 **MEDIUM**

**Why it's medium impact:**
- Only helps if same metrics are sent repeatedly
- Cache hit rate may be low for diverse metrics
- **Expected improvement:** 10-20% for cached metrics only

**Implementation:**
- Cache label set conversions
- Cache metric name lookups
- Requires cache invalidation strategy

**Estimated gain:** Variable, depends on cache hit rate

---

#### 8. **Tune gRPC Channel Parameters** ⭐
**Impact:** 🔥 **MEDIUM**  
**Effort:** 🟢 **LOW** (1-2 hours)  
**Risk:** 🟢 **LOW**

**Why it's medium impact:**
- Fine-tuning can improve throughput
- May help with concurrent requests
- **Expected improvement:** 5-15% depending on workload

**Implementation:**
- Tune `GRPC_ARG_MAX_CONCURRENT_STREAMS`
- Adjust `GRPC_ARG_HTTP2_MAX_FRAME_SIZE`
- Configure compression

**Estimated gain:** 2-5% improvement

---

## ✅ Verification Complete

**Date:** 2025-11-23  
**Status:** ✅ All gRPC read/write paths verified and working correctly

Before beginning optimizations, comprehensive verification was completed:
- ✅ Protobuf attribute preservation verified
- ✅ Bridge conversion verified (40 attributes → 41 labels)
- ✅ gRPC Export service verified
- ✅ WAL serialization/deserialization verified
- ✅ Full pipeline end-to-end verified

**See:** `docs/planning/GRPC_READ_WRITE_PATH_VERIFICATION.md` for details.

## 🎯 Recommended Optimization Roadmap

### Phase 1: Quick Wins (1-2 days) ⏳ **READY TO START**
**Target:** 7-15% improvement in single-write performance

1. ⬜ **Connection Pooling** (1-2 hours) - 5-10% gain
   - Configure gRPC channel with keep-alive
   - Reuse channels across requests
   - **Status:** Not started

2. ⬜ **Tune gRPC Parameters** (1 hour) - 2-5% gain
   - Tune `GRPC_ARG_MAX_CONCURRENT_STREAMS`
   - Adjust `GRPC_ARG_HTTP2_MAX_FRAME_SIZE`
   - Configure compression
   - **Status:** Not started

**Total expected improvement:** 7-15% faster single writes

### Phase 2: High-Impact Optimizations (1 week) ⏳ **NEXT**
**Target:** 30-50% improvement in single-write performance

1. ⬜ **Async gRPC Calls** (4-6 hours) - 20-30% gain
   - Use `grpc::CompletionQueue` for async calls
   - Implement async `Export()` method
   - Refactor MetricsService
   - **Status:** Not started
   - **Current:** MetricsService::Export is synchronous (blocking)

2. ⬜ **Optimize OTEL Bridge** (3-5 hours) - 15-25% gain
   - Profile bridge conversion code
   - Reduce allocations (object pools, reuse buffers)
   - Optimize label/attribute conversion
   - Batch process multiple metrics together
   - **Status:** Not started

**Total expected improvement:** 35-55% faster single writes

### Phase 3: Advanced Optimizations (1-2 weeks) ⏳ **FUTURE**
**Target:** Additional 20-30% improvement

1. ⬜ **Optimize Protobuf** (4-8 hours) - 20-30% serialization improvement
   - Profile protobuf serialization
   - Consider faster alternatives (FlatBuffers, Cap'n Proto)
   - Use protobuf arena allocation
   - **Status:** Not started

2. ⬜ **Batch Conversions** (2-3 hours) - 5-10% batch improvement
   - Process multiple metrics in single conversion pass
   - Reduce per-metric overhead
   - **Status:** Not started

3. ⬜ **Cache Common Conversions** (3-4 hours) - Variable gain
   - Cache label set conversions
   - Cache metric name lookups
   - **Status:** Not started

**Total expected improvement:** Additional 25-40% improvement

## 📊 Expected Cumulative Impact

### Single Write Performance (Current: 84.3 μs, 11.8K writes/sec)

| Phase | Optimization | Expected Gain | New Latency | New Throughput |
|-------|-------------|---------------|-------------|----------------|
| Baseline | Current | - | 84.3 μs | 11.8K writes/sec |
| Phase 1 | Quick Wins | 7-15% | 72-78 μs | 13-15K writes/sec |
| Phase 2 | High-Impact | 35-55% | 38-55 μs | 18-26K writes/sec |
| Phase 3 | Advanced | +25-40% | 23-38 μs | 26-43K writes/sec |

### Batch Write Performance (Current: 197 μs for 64 metrics)

| Phase | Optimization | Expected Gain | New Latency | New Throughput |
|-------|-------------|---------------|-------------|----------------|
| Baseline | Current | - | 197 μs | 324K writes/sec |
| Phase 1 | Quick Wins | 5-10% | 177-187 μs | 347-365K writes/sec |
| Phase 2 | High-Impact | 20-30% | 138-158 μs | 405-470K writes/sec |
| Phase 3 | Advanced | +15-25% | 103-138 μs | 463-621K writes/sec |

## 🎯 Top 2 Recommendations (Biggest Bang for Buck)

### 1. **Async gRPC Calls** ⭐⭐⭐
- **Impact:** 20-30% improvement (single), 2-5x (concurrent)
- **Effort:** 4-6 hours
- **Risk:** Medium
- **ROI:** Excellent

### 2. **Optimize OTEL Bridge** ⭐⭐⭐
- **Impact:** 15-25% improvement
- **Effort:** 3-5 hours
- **Risk:** Low
- **ROI:** Excellent

## 💡 Implementation Priority

### ✅ Pre-Implementation Checklist
- [x] Verify gRPC read/write paths work correctly
- [x] Establish baseline performance metrics
- [x] Identify optimization opportunities
- [ ] Clean up debug logging from verification
- [ ] Re-run benchmarks with clean data to confirm baseline

### 🚀 Implementation Order

**Phase 1 (Quick Wins) - Start Here:**
1. **Connection Pooling** - Easy (1-2 hours), good impact (5-10%)
   - Low risk, immediate benefit
   - Can be done in benchmark client first
   
2. **Tune gRPC Parameters** - Easy (1 hour), incremental (2-5%)
   - Low risk, quick to test
   - Server-side configuration

**Phase 2 (High-Impact) - Next:**
1. **Optimize OTEL Bridge** - Medium effort (3-5 hours), high impact (15-25%)
   - Lower risk than async refactoring
   - Can profile first to identify bottlenecks
   
2. **Async gRPC Calls** - Medium effort (4-6 hours), very high impact (20-30%)
   - Requires refactoring MetricsService
   - Biggest benefit for concurrent workloads

**Phase 3 (Advanced) - Future:**
1. **Optimize Protobuf** - Higher effort (4-8 hours), good impact (20-30%)
2. **Batch Conversions** - Low effort (2-3 hours), incremental (5-10%)
3. **Cache Conversions** - Medium effort (3-4 hours), variable impact

## 📋 Current Implementation Status

### MetricsService (src/tsdb/otel/bridge.cpp)
- **Current:** Synchronous (blocking) `Export()` method
- **Location:** Lines 338-365
- **Next Step:** Implement async version using `grpc::CompletionQueue`

### OTEL Bridge (src/tsdb/otel/bridge.cpp)
- **Current:** Direct conversion with allocations
- **Location:** `OTELMetricsBridgeImpl::ConvertMetrics()` and related methods
- **Next Step:** Profile and optimize allocations, batch processing

## 🔍 Profiling Recommendations

Before optimizing, profile to identify actual bottlenecks:

1. **Profile gRPC stack:**
   ```bash
   perf record ./otel_write_perf_bench
   perf report
   ```

2. **Profile OTEL bridge:**
   - Add timing instrumentation to bridge conversion
   - Measure time spent in each conversion step

3. **Profile protobuf:**
   - Measure serialization vs deserialization time
   - Identify hot paths in protobuf code

## 📝 Notes

- All improvements are **additive** - can implement multiple optimizations
- Batch writes already perform well - focus on single-write optimization
- Some optimizations (async, connection pooling) help both single and batch
- Async gRPC provides biggest benefit for concurrent workloads
- Network-based ingestion is the primary use case (Unix domain sockets not applicable)

---

**Conclusion:** Start with **Async gRPC Calls** and **Optimize OTEL Bridge** for maximum impact with reasonable effort.

