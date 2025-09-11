#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/tiered_memory_manager.h"
#include "tsdb/storage/semantic_vector/adaptive_memory_pool.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
namespace core = ::tsdb::core;

TEST(SemVecSmoke, TieredMemoryManagerBasic)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig cfg;
    cfg.ram_tier_capacity_mb = 100;
    cfg.ssd_tier_capacity_mb = 500;
    cfg.hdd_tier_capacity_mb = 1000;
    cfg.enable_tiered_memory = true;
    auto manager = CreateTieredMemoryManager(cfg);

    // Test adding series to different tiers
    ASSERT_TRUE(manager->add_series(12345, core::semantic_vector::MemoryTier::RAM).ok());
    ASSERT_TRUE(manager->add_series(12346, core::semantic_vector::MemoryTier::SSD).ok());
    ASSERT_TRUE(manager->add_series(12347, core::semantic_vector::MemoryTier::HDD).ok());

    // Test getting series tier
    auto tier = manager->get_series_tier(12345);
    ASSERT_TRUE(tier.ok());
    ASSERT_EQ(tier.value(), core::semantic_vector::MemoryTier::RAM);

    // Test getting series memory
    auto memory = manager->get_series_memory(12345);
    ASSERT_TRUE(memory.ok());
    ASSERT_NE(memory.value(), nullptr);

    // Test migration operations
    ASSERT_TRUE(manager->migrate_series(12345, core::semantic_vector::MemoryTier::SSD).ok());
    
    auto new_tier = manager->get_series_tier(12345);
    ASSERT_TRUE(new_tier.ok());
    ASSERT_EQ(new_tier.value(), core::semantic_vector::MemoryTier::SSD);

    // Test promotion and demotion
    ASSERT_TRUE(manager->promote_series(12347).ok());
    ASSERT_TRUE(manager->demote_series(12346).ok());

    // Test optimization
    ASSERT_TRUE(manager->optimize_tier_allocation().ok());

    // Test performance metrics
    auto metrics = manager->get_performance_metrics();
    ASSERT_TRUE(metrics.ok());

    // Test cleanup
    ASSERT_TRUE(manager->remove_series(12345).ok());
    ASSERT_TRUE(manager->remove_series(12346).ok());
    ASSERT_TRUE(manager->remove_series(12347).ok());
}

TEST(SemVecSmoke, AdaptiveMemoryPoolBasic)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig cfg;
    cfg.ram_tier_capacity_mb = 100;
    cfg.enable_adaptive_allocation = true;
    auto pool = CreateAdaptiveMemoryPool(cfg);

    // Test basic allocation/deallocation
    auto ptr1 = pool->allocate(1024);
    ASSERT_TRUE(ptr1.ok());
    ASSERT_NE(ptr1.value(), nullptr);

    auto ptr2 = pool->allocate(2048, 8); // With alignment
    ASSERT_TRUE(ptr2.ok());
    ASSERT_NE(ptr2.value(), nullptr);

    // Test access recording
    ASSERT_TRUE(pool->record_access(ptr1.value()).ok());
    ASSERT_TRUE(pool->record_access(ptr2.value()).ok());

    // Test reallocation
    auto ptr3 = pool->reallocate(ptr1.value(), 4096);
    ASSERT_TRUE(ptr3.ok());
    ASSERT_NE(ptr3.value(), nullptr);

    // Test pool statistics
    auto stats = pool->get_pool_stats();
    ASSERT_TRUE(stats.ok());
    ASSERT_GT(stats.value().allocated_bytes, 0);

    auto efficiency = pool->get_allocation_efficiency();
    ASSERT_TRUE(efficiency.ok());
    ASSERT_GE(efficiency.value(), 0.0);

    auto fragmentation = pool->get_fragmentation_ratio();
    ASSERT_TRUE(fragmentation.ok());
    ASSERT_GE(fragmentation.value(), 0.0);

    // Test optimization operations
    ASSERT_TRUE(pool->defragment().ok());
    ASSERT_TRUE(pool->compact().ok());
    ASSERT_TRUE(pool->optimize_allocation_strategy().ok());

    // Test performance metrics
    auto metrics = pool->get_performance_metrics();
    ASSERT_TRUE(metrics.ok());

    // Test cleanup
    ASSERT_TRUE(pool->deallocate(ptr3.value()).ok());
    ASSERT_TRUE(pool->deallocate(ptr2.value()).ok());
}

TEST(SemVecSmoke, TieredMemoryManagerPressure)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig cfg;
    cfg.ram_tier_capacity_mb = 10;  // Small capacity to trigger pressure
    cfg.ssd_tier_capacity_mb = 50;
    cfg.hdd_tier_capacity_mb = 100;
    cfg.enable_tiered_memory = true;
    auto manager = CreateTieredMemoryManager(cfg);

    // Add multiple series to trigger memory pressure
    for (core::SeriesID i = 1; i <= 5; ++i) {
        ASSERT_TRUE(manager->add_series(i, core::semantic_vector::MemoryTier::RAM).ok());
    }

    // Test memory pressure handling
    ASSERT_TRUE(manager->handle_memory_pressure().ok());

    // Test tier compaction
    ASSERT_TRUE(manager->compact_tier(core::semantic_vector::MemoryTier::RAM).ok());
    ASSERT_TRUE(manager->compact_tier(core::semantic_vector::MemoryTier::SSD).ok());
}

TEST(SemVecSmoke, AdaptiveMemoryPoolStress)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig cfg;
    cfg.ram_tier_capacity_mb = 50;
    cfg.enable_adaptive_allocation = true;
    auto pool = CreateAdaptiveMemoryPool(cfg);

    std::vector<void*> allocations;
    
    // Perform multiple allocations of different sizes
    for (size_t i = 1; i <= 20; ++i) {
        size_t size = 64 * i; // Different sizes
        auto ptr = pool->allocate(size);
        if (ptr.ok()) {
            allocations.push_back(ptr.value());
            // Record some accesses
            ASSERT_TRUE(pool->record_access(ptr.value()).ok());
        }
    }

    // Test defragmentation under load
    ASSERT_TRUE(pool->defragment().ok());

    // Deallocate every other allocation
    for (size_t i = 0; i < allocations.size(); i += 2) {
        ASSERT_TRUE(pool->deallocate(allocations[i]).ok());
    }

    // Test compaction after partial deallocation
    ASSERT_TRUE(pool->compact().ok());

    // Clean up remaining allocations
    for (size_t i = 1; i < allocations.size(); i += 2) {
        ASSERT_TRUE(pool->deallocate(allocations[i]).ok());
    }
}

TEST(SemVecSmoke, MemoryConfigValidation)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig cfg;
    cfg.ram_tier_capacity_mb = 1024;
    cfg.ssd_tier_capacity_mb = 5120;
    cfg.hdd_tier_capacity_mb = 10240;
    cfg.target_memory_reduction = 0.8;
    cfg.max_latency_impact = 0.05;

    // Test tiered memory manager validation
    auto tm_validation = ValidateTieredMemoryManagerConfig(cfg);
    ASSERT_TRUE(tm_validation.ok());
    ASSERT_TRUE(tm_validation.value().is_valid);

    // Test adaptive memory pool validation
    auto amp_validation = ValidateAdaptiveMemoryPoolConfig(cfg);
    ASSERT_TRUE(amp_validation.ok());
    ASSERT_TRUE(amp_validation.value().is_valid);

    // Test invalid configs
    cfg.ram_tier_capacity_mb = 0;
    cfg.target_memory_reduction = 1.5; // Invalid

    auto invalid_tm_validation = ValidateTieredMemoryManagerConfig(cfg);
    ASSERT_TRUE(invalid_tm_validation.ok());
    ASSERT_FALSE(invalid_tm_validation.value().is_valid);

    auto invalid_amp_validation = ValidateAdaptiveMemoryPoolConfig(cfg);
    ASSERT_TRUE(invalid_amp_validation.ok());
    ASSERT_FALSE(invalid_amp_validation.value().is_valid);
}

TEST(SemVecSmoke, MemoryUseCaseFactories)
{
    core::semantic_vector::SemanticVectorConfig::MemoryConfig base_cfg;
    
    // Test tiered memory manager use cases
    auto hp_manager = CreateTieredMemoryManagerForUseCase("high_performance", base_cfg);
    ASSERT_NE(hp_manager, nullptr);
    ASSERT_EQ(hp_manager->get_config().ram_tier_capacity_mb, 2048);
    
    auto me_manager = CreateTieredMemoryManagerForUseCase("memory_efficient", base_cfg);
    ASSERT_NE(me_manager, nullptr);
    ASSERT_EQ(me_manager->get_config().ram_tier_capacity_mb, 512);
    
    auto ha_manager = CreateTieredMemoryManagerForUseCase("high_accuracy", base_cfg);
    ASSERT_NE(ha_manager, nullptr);
    ASSERT_EQ(ha_manager->get_config().ram_tier_capacity_mb, 4096);

    // Test adaptive memory pool use cases
    auto hp_pool = CreateAdaptiveMemoryPoolForUseCase("high_performance", base_cfg);
    ASSERT_NE(hp_pool, nullptr);
    ASSERT_EQ(hp_pool->get_config().ram_tier_capacity_mb, 2048);
    
    auto me_pool = CreateAdaptiveMemoryPoolForUseCase("memory_efficient", base_cfg);
    ASSERT_NE(me_pool, nullptr);
    ASSERT_EQ(me_pool->get_config().ram_tier_capacity_mb, 512);
    
    auto ha_pool = CreateAdaptiveMemoryPoolForUseCase("high_accuracy", base_cfg);
    ASSERT_NE(ha_pool, nullptr);
    ASSERT_EQ(ha_pool->get_config().ram_tier_capacity_mb, 4096);
}
