#include <gtest/gtest.h>
#include "tsdb/storage/cache_hierarchy.h"
#include "tsdb/core/types.h"
#include <filesystem>
#include <memory>
#include <vector>

namespace tsdb {
namespace storage {

class CacheHierarchyReproTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/tsdb_cache_repro_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(CacheHierarchyReproTest, TriggerSegfault) {
    CacheHierarchyConfig config;
    config.l1_max_size = 10; // Small L1 to force eviction to L2
    config.l2_max_size = 100;
    config.l2_storage_path = test_dir_ + "/l2";
    config.l3_storage_path = test_dir_ + "/l3";
    config.enable_background_processing = false; // Disable background to isolate the issue

    CacheHierarchy cache(config);

    // Create more series than L1 can hold to force eviction to L2
    for (int i = 0; i < 20; ++i) {
        core::Labels labels;
        labels.add("id", std::to_string(i));
        auto series = std::make_shared<core::TimeSeries>(labels);
        // Add some dummy data
        series->add_sample(1000 + i, 1.0 + i);
        
        // This should trigger eviction from L1 to L2 when i >= 10
        cache.put(i, series);
    }
    
    // Verify we can retrieve them (some from L1, some from L2)
    for (int i = 0; i < 20; ++i) {
        auto series = cache.get(i);
        ASSERT_NE(series, nullptr) << "Failed to retrieve series " << i;
        // We can't check ID directly on series as it's not stored there, but we can check labels if we want
        // ASSERT_EQ(series->labels().get("id"), std::to_string(i));
    }
}

} // namespace storage
} // namespace tsdb
