#include "tsdb/prometheus/promql/query_metrics.h"

namespace tsdb {
namespace prometheus {
namespace promql {

QueryMetrics& QueryMetrics::GetInstance() {
    static QueryMetrics instance;
    return instance;
}

void QueryMetrics::RecordQuery(uint64_t duration_ns, bool error) {
    query_count_.fetch_add(1, std::memory_order_relaxed);
    if (error) {
        query_errors_.fetch_add(1, std::memory_order_relaxed);
    }
    total_query_time_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
}

void QueryMetrics::RecordParse(uint64_t duration_ns) {
    total_parse_time_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
}

void QueryMetrics::RecordEval(uint64_t duration_ns) {
    total_eval_time_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
}

void QueryMetrics::RecordExec(uint64_t duration_ns) {
    total_exec_time_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
}

void QueryMetrics::RecordStorageRead(uint64_t duration_ns, uint64_t samples, uint64_t series, uint64_t bytes) {
    total_storage_read_time_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
    samples_scanned_.fetch_add(samples, std::memory_order_relaxed);
    series_scanned_.fetch_add(series, std::memory_order_relaxed);
    bytes_scanned_.fetch_add(bytes, std::memory_order_relaxed);
}

QueryMetricsSnapshot QueryMetrics::GetSnapshot() const {
    QueryMetricsSnapshot snapshot;
    snapshot.query_count = query_count_.load(std::memory_order_relaxed);
    snapshot.query_errors = query_errors_.load(std::memory_order_relaxed);
    snapshot.total_query_time_ns = total_query_time_ns_.load(std::memory_order_relaxed);
    snapshot.total_parse_time_ns = total_parse_time_ns_.load(std::memory_order_relaxed);
    snapshot.total_eval_time_ns = total_eval_time_ns_.load(std::memory_order_relaxed);
    snapshot.total_exec_time_ns = total_exec_time_ns_.load(std::memory_order_relaxed);
    snapshot.total_storage_read_time_ns = total_storage_read_time_ns_.load(std::memory_order_relaxed);
    snapshot.samples_scanned = samples_scanned_.load(std::memory_order_relaxed);
    snapshot.series_scanned = series_scanned_.load(std::memory_order_relaxed);
    snapshot.bytes_scanned = bytes_scanned_.load(std::memory_order_relaxed);
    return snapshot;
}

void QueryMetrics::Reset() {
    query_count_.store(0, std::memory_order_relaxed);
    query_errors_.store(0, std::memory_order_relaxed);
    total_query_time_ns_.store(0, std::memory_order_relaxed);
    total_parse_time_ns_.store(0, std::memory_order_relaxed);
    total_eval_time_ns_.store(0, std::memory_order_relaxed);
    total_exec_time_ns_.store(0, std::memory_order_relaxed);
    total_storage_read_time_ns_.store(0, std::memory_order_relaxed);
    samples_scanned_.store(0, std::memory_order_relaxed);
    series_scanned_.store(0, std::memory_order_relaxed);
    bytes_scanned_.store(0, std::memory_order_relaxed);
}

ScopedQueryTimer::ScopedQueryTimer(Type type) : type_(type), start_(std::chrono::high_resolution_clock::now()) {}

ScopedQueryTimer::~ScopedQueryTimer() {
    if (!stopped_) {
        Stop();
    }
}

void ScopedQueryTimer::Stop(uint64_t samples, uint64_t series, uint64_t bytes) {
    if (stopped_) return;
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    
    auto& metrics = QueryMetrics::GetInstance();
    switch (type_) {
        case Type::QUERY: metrics.RecordQuery(duration, false); break;
        case Type::PARSE: metrics.RecordParse(duration); break;
        case Type::EVAL: metrics.RecordEval(duration); break;
        case Type::EXEC: metrics.RecordExec(duration); break;
        case Type::STORAGE_READ: metrics.RecordStorageRead(duration, samples, series, bytes); break;
    }
    stopped_ = true;
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
