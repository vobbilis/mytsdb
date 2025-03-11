#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"

namespace tsdb {
namespace storage {

std::shared_ptr<Storage> CreateStorage(const StorageOptions& options) {
    auto storage = std::make_shared<StorageImpl>();
    
    // Convert StorageOptions to core::StorageConfig
    core::StorageConfig config;
    config.data_dir = options.data_dir;
    config.block_size = options.max_block_size;
    config.block_duration = options.block_duration.count() * 1000;  // Convert seconds to milliseconds
    config.retention_period = options.block_duration.count() * 1000;  // Convert seconds to milliseconds
    config.max_blocks_per_series = 1024;  // Default value
    config.cache_size_bytes = 1024 * 1024 * 1024;  // 1GB default
    config.enable_compression = true;  // Enable by default
    
    storage->init(config);
    return storage;
}

}  // namespace storage
}  // namespace tsdb 