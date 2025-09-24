#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/tiered_memory_integration.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>

namespace tsdb {
namespace storage {

class TieredMemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.cache_size_bytes = 1024 * 1024 * 1024; // 1GB
        config_.block_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize tiered memory integration
        integration_ = std::make_unique<TieredMemoryIntegration>(config_);
        
        // Initialize the integration
        auto result = integration_->initialize();
        ASSERT_TRUE(result.ok()) << "Failed to initialize tiered memory integration: " << result.error();
    }
    
    void TearDown() override {
        integration_.reset();
    }
    
    std::unique_ptr<TieredMemoryIntegration> integration_;
    core::StorageConfig config_;
};

TEST_F(TieredMemoryIntegrationTest, BasicSeriesManagement) {
    // Test basic series management
    core::SeriesID series_id = 12345;
    
    // Add series to RAM tier
    auto add_result = integration_->add_series(series_id, MemoryTier::RAM);
    ASSERT_TRUE(add_result.ok()) << "Add series failed: " << add_result.error();
    
    // Get series tier
    auto tier_result = integration_->get_series_tier(series_id);
    ASSERT_TRUE(tier_result.ok()) << "Get series tier failed: " << tier_result.error();
    EXPECT_EQ(tier_result.value(), MemoryTier::RAM);
    
    // Remove series
    auto remove_result = integration_->remove_series(series_id);
    ASSERT_TRUE(remove_result.ok()) << "Remove series failed: " << remove_result.error();
}

TEST_F(TieredMemoryIntegrationTest, SeriesPromotion) {
    // Test series promotion
    core::SeriesID series_id = 12345;
    
    // Add series to SSD tier
    auto add_result = integration_->add_series(series_id, MemoryTier::SSD);
    ASSERT_TRUE(add_result.ok());
    
    // Promote to RAM
    auto promote_result = integration_->promote_series(series_id);
    ASSERT_TRUE(promote_result.ok()) << "Promote series failed: " << promote_result.error();
    
    // Verify tier change
    auto tier_result = integration_->get_series_tier(series_id);
    ASSERT_TRUE(tier_result.ok());
    EXPECT_EQ(tier_result.value(), MemoryTier::RAM);
}

TEST_F(TieredMemoryIntegrationTest, SeriesDemotion) {
    // Test series demotion
    core::SeriesID series_id = 12345;
    
    // Add series to RAM tier
    auto add_result = integration_->add_series(series_id, MemoryTier::RAM);
    ASSERT_TRUE(add_result.ok());
    
    // Demote to SSD
    auto demote_result = integration_->demote_series(series_id);
    ASSERT_TRUE(demote_result.ok()) << "Demote series failed: " << demote_result.error();
    
    // Verify tier change
    auto tier_result = integration_->get_series_tier(series_id);
    ASSERT_TRUE(tier_result.ok());
    EXPECT_EQ(tier_result.value(), MemoryTier::SSD);
}

TEST_F(TieredMemoryIntegrationTest, TieredLayoutOptimization) {
    // Test tiered layout optimization
    std::vector<core::SeriesID> series_ids;
    
    // Add multiple series
    for (int i = 0; i < 10; ++i) {
        core::SeriesID series_id = 1000 + i;
        series_ids.push_back(series_id);
        
        auto add_result = integration_->add_series(series_id, MemoryTier::SSD);
        ASSERT_TRUE(add_result.ok());
    }
    
    // Optimize tiered layout
    auto optimize_result = integration_->optimize_tiered_layout();
    ASSERT_TRUE(optimize_result.ok()) << "Tiered layout optimization failed: " << optimize_result.error();
    
    // Clean up
    for (const auto& series_id : series_ids) {
        auto remove_result = integration_->remove_series(series_id);
        ASSERT_TRUE(remove_result.ok());
    }
}

TEST_F(TieredMemoryIntegrationTest, Statistics) {
    // Test statistics
    auto tiered_stats = integration_->get_tiered_stats();
    ASSERT_FALSE(tiered_stats.empty());
    
    auto series_stats = integration_->get_series_tier_stats();
    ASSERT_FALSE(series_stats.empty());
    
    auto migration_stats = integration_->get_migration_stats();
    ASSERT_FALSE(migration_stats.empty());
}

TEST_F(TieredMemoryIntegrationTest, ConcurrentOperations) {
    // Test concurrent operations
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 25; ++j) {
                core::SeriesID series_id = 10000 + i * 100 + j;
                
                // Add series
                auto add_result = integration_->add_series(series_id, MemoryTier::SSD);
                if (add_result.ok()) {
                    // Promote series
                    auto promote_result = integration_->promote_series(series_id);
                    if (promote_result.ok()) {
                        // Remove series
                        auto remove_result = integration_->remove_series(series_id);
                        if (remove_result.ok()) {
                            success_count.fetch_add(1);
                        }
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all operations succeeded
    EXPECT_EQ(success_count.load(), 100); // 4 threads * 25 operations
}

TEST_F(TieredMemoryIntegrationTest, MultipleTiers) {
    // Test multiple tiers
    std::vector<core::SeriesID> ram_series, ssd_series, hdd_series;
    
    // Add series to different tiers
    for (int i = 0; i < 5; ++i) {
        core::SeriesID ram_id = 10000 + i;
        core::SeriesID ssd_id = 20000 + i;
        core::SeriesID hdd_id = 30000 + i;
        
        ram_series.push_back(ram_id);
        ssd_series.push_back(ssd_id);
        hdd_series.push_back(hdd_id);
        
        auto ram_result = integration_->add_series(ram_id, MemoryTier::RAM);
        auto ssd_result = integration_->add_series(ssd_id, MemoryTier::SSD);
        auto hdd_result = integration_->add_series(hdd_id, MemoryTier::HDD);
        
        ASSERT_TRUE(ram_result.ok());
        ASSERT_TRUE(ssd_result.ok());
        ASSERT_TRUE(hdd_result.ok());
    }
    
    // Verify tiers
    for (const auto& series_id : ram_series) {
        auto tier_result = integration_->get_series_tier(series_id);
        ASSERT_TRUE(tier_result.ok());
        EXPECT_EQ(tier_result.value(), MemoryTier::RAM);
    }
    
    for (const auto& series_id : ssd_series) {
        auto tier_result = integration_->get_series_tier(series_id);
        ASSERT_TRUE(tier_result.ok());
        EXPECT_EQ(tier_result.value(), MemoryTier::SSD);
    }
    
    for (const auto& series_id : hdd_series) {
        auto tier_result = integration_->get_series_tier(series_id);
        ASSERT_TRUE(tier_result.ok());
        EXPECT_EQ(tier_result.value(), MemoryTier::HDD);
    }
    
    // Clean up
    for (const auto& series_id : ram_series) {
        integration_->remove_series(series_id);
    }
    for (const auto& series_id : ssd_series) {
        integration_->remove_series(series_id);
    }
    for (const auto& series_id : hdd_series) {
        integration_->remove_series(series_id);
    }
}

TEST_F(TieredMemoryIntegrationTest, InvalidOperations) {
    // Test invalid operations
    core::SeriesID non_existent_series = 99999;
    
    // Try to get tier of non-existent series
    auto tier_result = integration_->get_series_tier(non_existent_series);
    ASSERT_FALSE(tier_result.ok()) << "Getting tier of non-existent series should fail";
    
    // Try to remove non-existent series
    auto remove_result = integration_->remove_series(non_existent_series);
    ASSERT_FALSE(remove_result.ok()) << "Removing non-existent series should fail";
    
    // Try to promote non-existent series
    auto promote_result = integration_->promote_series(non_existent_series);
    ASSERT_FALSE(promote_result.ok()) << "Promoting non-existent series should fail";
    
    // Try to demote non-existent series
    auto demote_result = integration_->demote_series(non_existent_series);
    ASSERT_FALSE(demote_result.ok()) << "Demoting non-existent series should fail";
}

} // namespace storage
} // namespace tsdb
