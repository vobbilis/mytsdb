#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace tsdb {
namespace prometheus {
namespace promql {

struct QueryMetricsSnapshot {
    uint64_t query_count = 0;
    uint64_t query_errors = 0;
    uint64_t total_query_time_ns = 0;
    uint64_t total_parse_time_ns = 0;
    uint64_t total_eval_time_ns = 0;
    uint64_t total_exec_time_ns = 0;
    uint64_t total_storage_read_time_ns = 0;
    uint64_t samples_scanned = 0;
    uint64_t series_scanned = 0;
    uint64_t bytes_scanned = 0;
};

class QueryMetrics {
public:
    static QueryMetrics& GetInstance();

    void RecordQuery(uint64_t duration_ns, bool error);
    void RecordParse(uint64_t duration_ns);
    void RecordEval(uint64_t duration_ns);
    void RecordExec(uint64_t duration_ns);
    void RecordStorageRead(uint64_t duration_ns, uint64_t samples, uint64_t series, uint64_t bytes);

    QueryMetricsSnapshot GetSnapshot() const;
    void Reset();

private:
    QueryMetrics() = default;

    std::atomic<uint64_t> query_count_{0};
    std::atomic<uint64_t> query_errors_{0};
    std::atomic<uint64_t> total_query_time_ns_{0};
    std::atomic<uint64_t> total_parse_time_ns_{0};
    std::atomic<uint64_t> total_eval_time_ns_{0};
    std::atomic<uint64_t> total_exec_time_ns_{0};
    std::atomic<uint64_t> total_storage_read_time_ns_{0};
    std::atomic<uint64_t> samples_scanned_{0};
    std::atomic<uint64_t> series_scanned_{0};
    std::atomic<uint64_t> bytes_scanned_{0};
};

class ScopedQueryTimer {
public:
    enum class Type {
        QUERY,
        PARSE,
        EVAL,
        EXEC,
        STORAGE_READ
    };

    explicit ScopedQueryTimer(Type type);
    ~ScopedQueryTimer();

    void Stop(uint64_t samples = 0, uint64_t series = 0, uint64_t bytes = 0);

private:
    Type type_;
    std::chrono::high_resolution_clock::time_point start_;
    bool stopped_ = false;
};

} // namespace promql
} // namespace prometheus
} // namespace tsdb
