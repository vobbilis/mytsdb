#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <iostream>

namespace tsdb {
namespace integration {

class DebugCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        core::StorageConfig config;
        config.data_dir = "./test/data/storageimpl_phases/debug";
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
    }
    
    std::unique_ptr<storage::StorageImpl> storage_;
};

TEST_F(DebugCacheTest, DebugWriteAndRead) {
    std::cout << "\n=== DEBUG WRITE AND READ TEST ===" << std::endl;
    
    // Create a simple test series
    core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("test", "debug");
    labels.add("series_id", "0");
    
    core::TimeSeries series(labels);
    for (int i = 0; i < 10; ++i) {
        series.add_sample(1000 + i, 100.0 + i);
    }
    
    std::cout << "Writing series with " << series.samples().size() << " samples" << std::endl;
    std::cout << "Series labels: " << series.labels().to_string() << std::endl;
    
    // Write the series
    auto write_result = storage_->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Print storage stats
    std::cout << "\nStorage stats after write:" << std::endl;
    std::cout << storage_->stats() << std::endl;
    
    // Try to read the series
    std::cout << "\nTrying to read series..." << std::endl;
    auto read_result = storage_->read(labels, 1000, 1010);
    
    if (read_result.ok()) {
        std::cout << "Read successful!" << std::endl;
        std::cout << "Read series has " << read_result.value().samples().size() << " samples" << std::endl;
        std::cout << "Read series labels: " << read_result.value().labels().to_string() << std::endl;
        
        // Print first few samples
        for (int i = 0; i < std::min(5, (int)read_result.value().samples().size()); ++i) {
            const auto& sample = read_result.value().samples()[i];
            std::cout << "Sample " << i << ": timestamp=" << sample.timestamp() 
                      << ", value=" << sample.value() << std::endl;
        }
    } else {
        std::cout << "Read failed: " << read_result.error() << std::endl;
    }
    
    // Print storage stats after read
    std::cout << "\nStorage stats after read:" << std::endl;
    std::cout << storage_->stats() << std::endl;
}

} // namespace integration
} // namespace tsdb 