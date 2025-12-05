#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/common/logger.h"
#include <filesystem>
#include <thread>
#include <atomic>
#include <vector>

using namespace tsdb;
using namespace tsdb::storage;

class StorageStabilityTest : public ::testing::Test {
protected:
    std::string test_dir_;
    std::shared_ptr<StorageImpl> storage_;

    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_stability_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        config.background_config.enable_background_processing = false; // Isolate from background tasks first
        config.enable_compression = false;
        
        storage_ = std::make_shared<StorageImpl>(config);
        storage_->init(config);
    }

    void TearDown() override {
        TSDB_INFO("TearDown: start");
        if (storage_) {
            TSDB_INFO("TearDown: calling close");
            storage_->close();
            TSDB_INFO("TearDown: close done");
            storage_.reset(); // Ensure storage is destroyed and releases all resources (files, mmaps)
            TSDB_INFO("TearDown: reset done");
        }
        TSDB_INFO("TearDown: removing dir");
        std::filesystem::remove_all(test_dir_);
        TSDB_INFO("TearDown: done");
    }
};

#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/prometheus/model/types.h"

// ...

TEST_F(StorageStabilityTest, ConcurrentWriteAndAdapterQuery) {
    std::atomic<bool> running{true};
    std::atomic<int> write_count{0};
    std::atomic<int> query_count{0};
    
    auto adapter = std::make_shared<prometheus::storage::TSDBAdapter>(storage_);
    
    // 1. Writer Thread
    std::thread writer([&]() {
        int i = 0;
        while (running) {
            core::Labels labels;
            labels.add("__name__", "test_metric");
            labels.add("instance", "inst_" + std::to_string(i % 2000)); // 2000 series
            core::TimeSeries series(labels);
            series.add_sample(std::chrono::system_clock::now().time_since_epoch().count(), 1.0);
            
            storage_->write(series);
            write_count++;
            i++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // 2. Reader Thread (Adapter Query)
    std::thread reader([&]() {
        while (running) {
            std::vector<prometheus::model::LabelMatcher> matchers;
            matchers.push_back({prometheus::model::MatcherType::EQUAL, "__name__", "test_metric"});
            
            try {
                auto result = adapter->SelectSeries(matchers, 0, std::numeric_limits<int64_t>::max());
                query_count++;
            } catch (const std::exception& e) {
                TSDB_ERROR("Query failed: {}", e.what());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Run for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));
    running = false;
    
    writer.join();
    reader.join();
    
    TSDB_INFO("Completed {} writes and {} queries", write_count.load(), query_count.load());
}
