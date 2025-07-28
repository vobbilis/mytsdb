#include <gtest/gtest.h>
#include "tsdb/storage/performance_config.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <future>

using namespace tsdb::storage::internal;

class PerformanceConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<PerformanceConfig>("test_config");
    }
    
    void TearDown() override {
        config_.reset();
    }
    
    std::unique_ptr<PerformanceConfig> config_;
};

TEST_F(PerformanceConfigTest, DefaultConfiguration) {
    // Test default configuration values
    auto flags = config_->getFlags();
    auto thresholds = config_->getThresholds();
    auto runtime = config_->getRuntimeConfig();
    
    // Check default flags
    EXPECT_TRUE(flags.enable_object_pooling);
    EXPECT_TRUE(flags.enable_working_set_cache);
    EXPECT_TRUE(flags.enable_type_aware_compression);
    EXPECT_TRUE(flags.enable_delta_of_delta_encoding);
    EXPECT_TRUE(flags.enable_atomic_metrics);
    EXPECT_FALSE(flags.enable_sharded_writes);
    EXPECT_FALSE(flags.enable_background_processing);
    
    // Check default thresholds
    EXPECT_EQ(thresholds.max_memory_usage_mb, 8192);
    EXPECT_EQ(thresholds.cache_size_mb, 1024);
    EXPECT_EQ(thresholds.max_write_latency_ms, 10.0);
    EXPECT_EQ(thresholds.max_read_latency_ms, 5.0);
    EXPECT_EQ(thresholds.min_compression_ratio, 0.1);
    EXPECT_EQ(thresholds.max_compression_ratio, 0.8);
    
    // Check default runtime config
    EXPECT_EQ(runtime.metrics_sampling_interval, 1000);
    EXPECT_EQ(runtime.performance_check_interval_ms, 5000);
    EXPECT_TRUE(runtime.enable_adaptive_tuning);
    EXPECT_TRUE(runtime.enable_automatic_rollback);
}

TEST_F(PerformanceConfigTest, FeatureFlagManagement) {
    // Test feature flag management
    EXPECT_TRUE(config_->isFeatureEnabled("object_pooling"));
    EXPECT_TRUE(config_->isFeatureEnabled("working_set_cache"));
    EXPECT_FALSE(config_->isFeatureEnabled("sharded_writes"));
    EXPECT_FALSE(config_->isFeatureEnabled("unknown_feature"));
    
    // Test enabling/disabling features
    auto result = config_->setFeatureEnabled("sharded_writes", true);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(config_->isFeatureEnabled("sharded_writes"));
    
    result = config_->setFeatureEnabled("object_pooling", false);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(config_->isFeatureEnabled("object_pooling"));
    
    // Test unknown feature
    result = config_->setFeatureEnabled("unknown_feature", true);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
}

TEST_F(PerformanceConfigTest, ConfigurationValidation) {
    // Test valid configuration
    auto validation = config_->validate();
    EXPECT_TRUE(validation.is_valid);
    
    // Test invalid thresholds
    PerformanceThresholds invalid_thresholds;
    invalid_thresholds.max_memory_usage_mb = 0;  // Invalid
    invalid_thresholds.cache_size_mb = 10000;    // Greater than max memory
    
    auto result = config_->updateThresholds(invalid_thresholds);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Test valid thresholds update
    PerformanceThresholds valid_thresholds;
    valid_thresholds.max_memory_usage_mb = 16384;  // 16GB
    valid_thresholds.cache_size_mb = 2048;         // 2GB
    valid_thresholds.max_write_latency_ms = 20.0;
    valid_thresholds.max_read_latency_ms = 10.0;
    
    result = config_->updateThresholds(valid_thresholds);
    EXPECT_TRUE(result.is_valid);
    
    auto updated_thresholds = config_->getThresholds();
    EXPECT_EQ(updated_thresholds.max_memory_usage_mb, 16384);
    EXPECT_EQ(updated_thresholds.cache_size_mb, 2048);
    EXPECT_EQ(updated_thresholds.max_write_latency_ms, 20.0);
    EXPECT_EQ(updated_thresholds.max_read_latency_ms, 10.0);
}

TEST_F(PerformanceConfigTest, ABTesting) {
    // Test A/B test configuration
    ABTestConfig test_config;
    test_config.test_name = "compression_optimization_test";
    test_config.variant_a_name = "control";
    test_config.variant_b_name = "optimized";
    test_config.variant_a_percentage = 50.0;
    test_config.variant_b_percentage = 50.0;
    test_config.test_duration = std::chrono::seconds(3600);  // 1 hour
    test_config.enable_gradual_rollout = true;
    test_config.rollout_percentage = 10.0;
    
    auto result = config_->startABTest(test_config);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(config_->isABTestActive());
    
    // Test variant assignment
    std::string variant1 = config_->getABTestVariant("user1");
    std::string variant2 = config_->getABTestVariant("user2");
    std::string variant3 = config_->getABTestVariant("user3");
    
    // Same user should get same variant
    EXPECT_EQ(config_->getABTestVariant("user1"), variant1);
    EXPECT_EQ(config_->getABTestVariant("user1"), variant1);
    
    // Test A/B test results
    auto results = config_->getABTestResults();
    EXPECT_EQ(results.test_name, "compression_optimization_test");
    
    // Stop A/B test
    config_->stopABTest();
    EXPECT_FALSE(config_->isABTestActive());
    EXPECT_EQ(config_->getABTestVariant("user1"), "control");
}

TEST_F(PerformanceConfigTest, ABTestValidation) {
    // Test invalid A/B test configurations
    ABTestConfig invalid_config;
    
    // Empty test name
    auto result = config_->startABTest(invalid_config);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Invalid percentages
    invalid_config.test_name = "test";
    invalid_config.variant_a_percentage = 60.0;
    invalid_config.variant_b_percentage = 50.0;  // Doesn't sum to 100
    
    result = config_->startABTest(invalid_config);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Invalid rollout percentage
    invalid_config.variant_a_percentage = 50.0;
    invalid_config.variant_b_percentage = 50.0;
    invalid_config.rollout_percentage = 150.0;  // > 100%
    
    result = config_->startABTest(invalid_config);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Valid configuration
    invalid_config.rollout_percentage = 10.0;
    result = config_->startABTest(invalid_config);
    EXPECT_TRUE(result.is_valid);
}

TEST_F(PerformanceConfigTest, ConfigurationPersistence) {
    // Test JSON serialization
    std::string json = config_->toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("config_name"), std::string::npos);
    EXPECT_NE(json.find("flags"), std::string::npos);
    EXPECT_NE(json.find("thresholds"), std::string::npos);
    
    // Test JSON deserialization
    auto result = config_->fromJson(json);
    EXPECT_TRUE(result.is_valid);
    
    // Test invalid JSON
    result = config_->fromJson("invalid json");
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
}

TEST_F(PerformanceConfigTest, ConfigurationSummary) {
    // Test configuration summary
    std::string summary = config_->getSummary();
    EXPECT_FALSE(summary.empty());
    EXPECT_NE(summary.find("Performance Configuration"), std::string::npos);
    EXPECT_NE(summary.find("Feature Flags"), std::string::npos);
    EXPECT_NE(summary.find("Performance Thresholds"), std::string::npos);
    EXPECT_NE(summary.find("Runtime Configuration"), std::string::npos);
    
    // Test summary with A/B test
    ABTestConfig test_config;
    test_config.test_name = "test";
    test_config.variant_a_percentage = 50.0;
    test_config.variant_b_percentage = 50.0;
    config_->startABTest(test_config);
    
    summary = config_->getSummary();
    EXPECT_NE(summary.find("A/B Test Active"), std::string::npos);
    EXPECT_NE(summary.find("test"), std::string::npos);
}

TEST_F(PerformanceConfigTest, ChangeCallbacks) {
    // Test configuration change callbacks
    bool callback_called = false;
    std::string callback_config_name;
    
    auto callback = [&](const PerformanceConfig& config) {
        callback_called = true;
        callback_config_name = config.getConfigName();
    };
    
    config_->registerChangeCallback(callback);
    
    // Trigger a change
    PerformanceFlags new_flags = config_->getFlags();
    new_flags.enable_sharded_writes = true;
    config_->updateFlags(new_flags);
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_config_name, "test_config");
}

TEST_F(PerformanceConfigTest, ThreadSafety) {
    // Test thread safety with concurrent access
    const int num_threads = 4;
    const int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, operations_per_thread, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Read operations
                auto flags = config_->getFlags();
                auto thresholds = config_->getThresholds();
                config_->isFeatureEnabled("object_pooling");
                
                // Write operations
                if (j % 10 == 0) {
                    config_->setFeatureEnabled("sharded_writes", (j % 20 == 0));
                }
                
                // A/B test operations
                if (j % 100 == 0) {
                    config_->getABTestVariant("user" + std::to_string(i) + "_" + std::to_string(j));
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Configuration should still be valid after concurrent access
    auto validation = config_->validate();
    EXPECT_TRUE(validation.is_valid);
}

TEST_F(PerformanceConfigTest, GlobalConfiguration) {
    // Test global configuration instance
    GlobalPerformanceConfig::initialize("global_test");
    
    auto& global_config = GlobalPerformanceConfig::getInstance();
    EXPECT_EQ(global_config.getConfigName(), "global_test");
    
    // Test global configuration operations
    EXPECT_TRUE(global_config.isFeatureEnabled("object_pooling"));
    
    auto result = global_config.setFeatureEnabled("sharded_writes", true);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(global_config.isFeatureEnabled("sharded_writes"));
    
    // Test global configuration persistence
    std::string json = global_config.toJson();
    EXPECT_FALSE(json.empty());
    
    // Reset global configuration
    GlobalPerformanceConfig::resetToDefaults();
    EXPECT_FALSE(global_config.isFeatureEnabled("sharded_writes"));
}

TEST_F(PerformanceConfigTest, RuntimeConfiguration) {
    // Test runtime configuration updates
    RuntimeConfig new_runtime;
    new_runtime.metrics_sampling_interval = 500;
    new_runtime.performance_check_interval_ms = 2000;
    new_runtime.enable_adaptive_tuning = false;
    new_runtime.enable_automatic_rollback = false;
    new_runtime.enable_debug_logging = true;
    new_runtime.log_level = 3;
    
    auto result = config_->updateRuntimeConfig(new_runtime);
    EXPECT_TRUE(result.is_valid);
    
    auto updated_runtime = config_->getRuntimeConfig();
    EXPECT_EQ(updated_runtime.metrics_sampling_interval, 500);
    EXPECT_EQ(updated_runtime.performance_check_interval_ms, 2000);
    EXPECT_FALSE(updated_runtime.enable_adaptive_tuning);
    EXPECT_FALSE(updated_runtime.enable_automatic_rollback);
    EXPECT_TRUE(updated_runtime.enable_debug_logging);
    EXPECT_EQ(updated_runtime.log_level, 3);
}

TEST_F(PerformanceConfigTest, FlagConflicts) {
    // Test flag conflict validation
    PerformanceFlags conflicting_flags = config_->getFlags();
    conflicting_flags.enable_machine_learning_optimization = true;
    conflicting_flags.enable_atomic_metrics = false;  // Conflict
    
    auto result = config_->updateFlags(conflicting_flags);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.errors.empty());
    
    // Test warning for suboptimal configuration
    PerformanceFlags warning_flags = config_->getFlags();
    warning_flags.enable_sharded_writes = true;
    warning_flags.enable_background_processing = false;  // Warning
    
    result = config_->updateFlags(warning_flags);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(result.warnings.empty());
}

TEST_F(PerformanceConfigTest, ABTestGradualRollout) {
    // Test gradual rollout functionality
    ABTestConfig test_config;
    test_config.test_name = "gradual_rollout_test";
    test_config.variant_a_percentage = 50.0;
    test_config.variant_b_percentage = 50.0;
    test_config.enable_gradual_rollout = true;
    test_config.rollout_percentage = 10.0;
    test_config.rollout_interval = std::chrono::minutes(1);
    test_config.test_duration = std::chrono::seconds(3600);
    
    auto result = config_->startABTest(test_config);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(config_->isABTestActive());
    
    // Initially, most users should get control
    int control_count = 0;
    int treatment_count = 0;
    
    for (int i = 0; i < 1000; ++i) {
        std::string variant = config_->getABTestVariant("user" + std::to_string(i));
        if (variant == "control") {
            control_count++;
        } else {
            treatment_count++;
        }
    }
    
    // With 10% rollout, most should be control
    EXPECT_GT(control_count, treatment_count);
    EXPECT_GT(control_count, 800);  // At least 80% should be control
}

TEST_F(PerformanceConfigTest, ABTestExpiration) {
    // Test A/B test expiration
    ABTestConfig test_config;
    test_config.test_name = "expiration_test";
    test_config.variant_a_percentage = 50.0;
    test_config.variant_b_percentage = 50.0;
    test_config.test_duration = std::chrono::seconds(1);  // Very short duration
    
    auto result = config_->startABTest(test_config);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(config_->isABTestActive());
    
    // Wait for test to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Should return control after expiration
    EXPECT_EQ(config_->getABTestVariant("user1"), "control");
}

TEST_F(PerformanceConfigTest, ConfigurationReset) {
    // Test configuration reset
    PerformanceFlags custom_flags = config_->getFlags();
    custom_flags.enable_sharded_writes = true;
    custom_flags.enable_background_processing = true;
    config_->updateFlags(custom_flags);
    
    PerformanceThresholds custom_thresholds = config_->getThresholds();
    custom_thresholds.max_memory_usage_mb = 16384;
    custom_thresholds.cache_size_mb = 2048;
    config_->updateThresholds(custom_thresholds);
    
    // Verify custom values
    EXPECT_TRUE(config_->isFeatureEnabled("sharded_writes"));
    EXPECT_TRUE(config_->isFeatureEnabled("background_processing"));
    EXPECT_EQ(config_->getThresholds().max_memory_usage_mb, 16384);
    
    // Reset to defaults
    config_->resetToDefaults();
    
    // Verify default values
    EXPECT_FALSE(config_->isFeatureEnabled("sharded_writes"));
    EXPECT_FALSE(config_->isFeatureEnabled("background_processing"));
    EXPECT_EQ(config_->getThresholds().max_memory_usage_mb, 8192);
}

TEST_F(PerformanceConfigTest, ConcurrentABTestAccess) {
    // Test concurrent A/B test access
    ABTestConfig test_config;
    test_config.test_name = "concurrent_test";
    test_config.variant_a_percentage = 50.0;
    test_config.variant_b_percentage = 50.0;
    test_config.enable_gradual_rollout = false;  // 100% rollout for testing
    
    config_->startABTest(test_config);
    
    const int num_threads = 4;
    const int requests_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> control_count{0};
    std::atomic<int> treatment_count{0};
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, requests_per_thread, i, &control_count, &treatment_count]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                std::string variant = config_->getABTestVariant("user" + std::to_string(i) + "_" + std::to_string(j));
                if (variant == "control") {
                    control_count++;
                } else {
                    treatment_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should have roughly equal distribution
    int total_requests = num_threads * requests_per_thread;
    EXPECT_GT(control_count, total_requests * 0.4);  // At least 40% control
    EXPECT_GT(treatment_count, total_requests * 0.4); // At least 40% treatment
}

TEST_F(PerformanceConfigTest, PerformanceBenchmark) {
    // Test performance of configuration operations
    const int num_operations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        config_->isFeatureEnabled("object_pooling");
        config_->isFeatureEnabled("working_set_cache");
        config_->isFeatureEnabled("sharded_writes");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable (less than 1μs per operation)
    double avg_time_per_op = static_cast<double>(duration.count()) / (num_operations * 3);
    EXPECT_LT(avg_time_per_op, 1.0);  // Less than 1μs per operation
}

TEST_F(PerformanceConfigTest, ConfigurationName) {
    // Test configuration name
    EXPECT_EQ(config_->getConfigName(), "test_config");
    
    // Test with different name
    PerformanceConfig custom_config("custom_name");
    EXPECT_EQ(custom_config.getConfigName(), "custom_name");
} 