/**
 * @file high_concurrency_storage.cpp
 * @brief Implementation of high-concurrency storage
 */

#include "tsdb/storage/high_concurrency_storage.h"
#include <sstream>
#include <future>

namespace tsdb {
namespace storage {

HighConcurrencyStorage::HighConcurrencyStorage(const ShardedStorageConfig& config)
    : config_(config)
    , sharded_manager_(std::make_unique<ShardedStorageManager>(config)) {
}

core::Result<void> HighConcurrencyStorage::init(const core::StorageConfig& config) {
    return sharded_manager_->init(config);
}

core::Result<void> HighConcurrencyStorage::write(const core::TimeSeries& series) {
    // For now, use synchronous write without callback
    // TODO: Implement proper synchronous write with callback
    return sharded_manager_->write(series, nullptr);
}

core::Result<core::TimeSeries> HighConcurrencyStorage::read(const core::Labels& labels, 
                                                          int64_t start_time, int64_t end_time) {
    return sharded_manager_->read(labels, start_time, end_time);
}

core::Result<std::vector<core::TimeSeries>> HighConcurrencyStorage::query(
    const std::vector<std::pair<std::string, std::string>>& matchers,
    int64_t start_time, int64_t end_time) {
    // For now, return empty result - this would need to be implemented in ShardedStorageManager
    return core::Result<std::vector<core::TimeSeries>>(std::vector<core::TimeSeries>());
}

core::Result<std::vector<std::string>> HighConcurrencyStorage::label_names() {
    // For now, return empty result - this would need to be implemented in ShardedStorageManager
    return core::Result<std::vector<std::string>>(std::vector<std::string>());
}

core::Result<std::vector<std::string>> HighConcurrencyStorage::label_values(const std::string& label_name) {
    // For now, return empty result - this would need to be implemented in ShardedStorageManager
    return core::Result<std::vector<std::string>>(std::vector<std::string>());
}

core::Result<void> HighConcurrencyStorage::delete_series(const std::vector<std::pair<std::string, std::string>>& matchers) {
    // For now, return success - this would need to be implemented in ShardedStorageManager
    return core::Result<void>();
}

core::Result<void> HighConcurrencyStorage::compact() {
    // For now, return success - this would need to be implemented in ShardedStorageManager
    return core::Result<void>();
}

core::Result<void> HighConcurrencyStorage::flush() {
    return sharded_manager_->flush();
}

core::Result<void> HighConcurrencyStorage::close() {
    return sharded_manager_->close();
}

std::string HighConcurrencyStorage::stats() const {
    return sharded_manager_->get_stats_string();
}

core::Result<void> HighConcurrencyStorage::write_async(const core::TimeSeries& series, 
                                                     std::function<void(const core::Result<void>&)> callback) {
    return sharded_manager_->write(series, callback);
}

ShardedStorageStats HighConcurrencyStorage::get_detailed_stats() const {
    return sharded_manager_->get_stats();
}

bool HighConcurrencyStorage::is_healthy() const {
    return sharded_manager_->is_healthy();
}

} // namespace storage
} // namespace tsdb
