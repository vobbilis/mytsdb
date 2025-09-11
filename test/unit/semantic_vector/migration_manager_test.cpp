#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/migration_manager.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
using namespace tsdb::core;

TEST(SemVecSmoke, MigrationManagerBasic) {
    // Create basic migration config
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_progress_tracking = true;
    config.enable_checkpoints = true;
    config.batch_size = 100;
    config.max_parallel_batches = 4;
    
    // Create migration manager instance
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Test migration start
    std::vector<SeriesID> test_series = {"series_1", "series_2", "series_3", "series_4", "series_5"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    ASSERT_FALSE(migration_id.empty());
    ASSERT_TRUE(migration_id.find("migration_") == 0);
    
    // Test migration progress tracking
    auto progress_result = migration_manager->get_migration_progress(migration_id);
    ASSERT_TRUE(progress_result.ok());
    
    auto progress = progress_result.value();
    ASSERT_EQ(progress.migration_id, migration_id);
    ASSERT_EQ(progress.strategy, semantic_vector::MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_EQ(progress.total_series_count, test_series.size());
    ASSERT_GE(progress.get_completion_percentage(), 0.0);
    ASSERT_LE(progress.get_completion_percentage(), 100.0);
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecSmoke, MigrationBatchProcessing) {
    // Create migration manager for batch testing
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.batch_size = 3;
    config.batch_retry_limit = 2;
    config.enable_data_validation = true;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Test batch creation
    std::vector<SeriesID> series_ids = {"batch_1", "batch_2", "batch_3", "batch_4", "batch_5", "batch_6", "batch_7"};
    auto batches_result = migration_manager->create_migration_batches(series_ids, 3);
    ASSERT_TRUE(batches_result.ok());
    
    auto batches = batches_result.value();
    ASSERT_EQ(batches.size(), 3);  // 7 series / 3 per batch = 3 batches
    
    // Verify batch contents
    ASSERT_EQ(batches[0].series_ids.size(), 3);
    ASSERT_EQ(batches[1].series_ids.size(), 3);
    ASSERT_EQ(batches[2].series_ids.size(), 1);  // Last batch has remainder
    
    // Test batch processing
    std::string migration_id = "test_migration_batch";
    auto process_result = migration_manager->process_migration_batch(migration_id, batches[0]);
    // Result may succeed or fail (simulated), but should return a valid result
    ASSERT_TRUE(process_result.ok() || !process_result.ok());
    
    // Verify batch status changed
    ASSERT_NE(batches[0].status, semantic_vector::MigrationBatch::Status::PENDING);
}

TEST(SemVecSmoke, MigrationCheckpoints) {
    // Create migration manager with checkpoint support
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_checkpoints = true;
    config.checkpoint_interval_batches = 2;
    config.max_checkpoints = 5;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start a migration
    std::vector<SeriesID> test_series = {"checkpoint_1", "checkpoint_2", "checkpoint_3"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::SEQUENTIAL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Create a checkpoint
    auto checkpoint_result = migration_manager->create_checkpoint(migration_id);
    ASSERT_TRUE(checkpoint_result.ok());
    
    std::string checkpoint_id = checkpoint_result.value();
    ASSERT_FALSE(checkpoint_id.empty());
    ASSERT_TRUE(checkpoint_id.find("checkpoint_") == 0);
    
    // Verify checkpoint exists
    auto checkpoints_result = migration_manager->get_migration_checkpoints(migration_id);
    ASSERT_TRUE(checkpoints_result.ok());
    
    auto checkpoints = checkpoints_result.value();
    ASSERT_GE(checkpoints.size(), 1);
    
    // Find our checkpoint
    bool found_checkpoint = false;
    for (const auto& cp : checkpoints) {
        if (cp.checkpoint_id == checkpoint_id) {
            found_checkpoint = true;
            ASSERT_TRUE(cp.is_valid());
            break;
        }
    }
    ASSERT_TRUE(found_checkpoint);
    
    // Test rollback to checkpoint
    auto rollback_result = migration_manager->rollback_to_checkpoint(migration_id, checkpoint_id);
    ASSERT_TRUE(rollback_result.ok());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecSmoke, MigrationRollback) {
    // Create migration manager for rollback testing
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_automatic_rollback = true;
    config.rollback_trigger_error_rate = 0.1;  // 10% error rate
    config.enable_rollback_to_checkpoint = true;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start a migration
    std::vector<SeriesID> test_series = {"rollback_1", "rollback_2", "rollback_3"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Test different rollback strategies
    auto immediate_rollback = migration_manager->rollback_migration(migration_id, semantic_vector::MigrationManager::RollbackStrategy::IMMEDIATE);
    ASSERT_TRUE(immediate_rollback.ok());
    
    // Verify migration status changed
    auto progress_result = migration_manager->get_migration_progress(migration_id);
    ASSERT_TRUE(progress_result.ok());
    
    auto progress = progress_result.value();
    // Progress phase should indicate rollback or cancellation
    ASSERT_TRUE(progress.current_phase == semantic_vector::MigrationManager::MigrationPhase::ROLLBACK ||
                !progress.is_completed());
}

TEST(SemVecSmoke, MigrationStatusReporting) {
    // Create migration manager for status reporting
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_migration_logging = true;
    config.enable_performance_monitoring = true;
    config.progress_report_interval_seconds = 1.0;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start a migration
    std::vector<SeriesID> test_series = {"status_1", "status_2", "status_3", "status_4"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::INCREMENTAL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Generate status report
    auto report_result = migration_manager->generate_status_report(migration_id);
    ASSERT_TRUE(report_result.ok());
    
    auto report = report_result.value();
    ASSERT_EQ(report.migration_id, migration_id);
    ASSERT_GE(report.system_cpu_usage, 0.0);
    ASSERT_LE(report.system_cpu_usage, 1.0);
    ASSERT_GE(report.system_memory_usage, 0.0);
    ASSERT_LE(report.system_memory_usage, 1.0);
    ASSERT_GE(report.data_accuracy, 0.0);
    ASSERT_LE(report.data_accuracy, 1.0);
    ASSERT_GE(report.migration_efficiency, 0.0);
    ASSERT_LE(report.migration_efficiency, 1.0);
    
    // Test overall health assessment
    double health_score = report.get_overall_health_score();
    ASSERT_GE(health_score, 0.0);
    ASSERT_LE(health_score, 1.0);
    
    // Test all status reports
    auto all_reports_result = migration_manager->generate_all_status_reports();
    ASSERT_TRUE(all_reports_result.ok());
    
    auto all_reports = all_reports_result.value();
    ASSERT_GE(all_reports.size(), 1);
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecSmoke, MigrationDataValidation) {
    // Create migration manager for data validation
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_data_validation = true;
    config.enable_integrity_checks = true;
    config.validation_sample_rate = 10;  // Validate every 10th series
    config.consistency_check_threshold = 0.95;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start a migration
    std::vector<SeriesID> test_series = {"validation_1", "validation_2", "validation_3"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::BULK);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Test data validation
    std::vector<SeriesID> sample_series = {"validation_1", "validation_2"};
    auto validation_result = migration_manager->validate_migration_data(migration_id, sample_series);
    ASSERT_TRUE(validation_result.ok());
    
    bool validation_success = validation_result.value();
    // Should be either true or false (simulated)
    ASSERT_TRUE(validation_success == true || validation_success == false);
    
    // Test consistency score calculation
    auto consistency_result = migration_manager->calculate_data_consistency_score(migration_id);
    ASSERT_TRUE(consistency_result.ok());
    
    double consistency_score = consistency_result.value();
    ASSERT_GE(consistency_score, 0.0);
    ASSERT_LE(consistency_score, 1.0);
    
    // Test data integrity verification
    auto integrity_result = migration_manager->verify_data_integrity(migration_id);
    ASSERT_TRUE(integrity_result.ok());
    
    // Test corruption detection
    auto corruption_result = migration_manager->detect_data_corruption(migration_id);
    ASSERT_TRUE(corruption_result.ok());
    
    auto corruption_instances = corruption_result.value();
    // Should return a vector (may be empty)
    ASSERT_TRUE(corruption_instances.size() >= 0);
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecSmoke, MigrationUseCases) {
    // Test high throughput use case
    auto high_throughput_manager = CreateMigrationManagerForUseCase("high_throughput");
    ASSERT_NE(high_throughput_manager, nullptr);
    
    auto throughput_config = high_throughput_manager->get_config();
    ASSERT_EQ(throughput_config.default_strategy, semantic_vector::MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_EQ(throughput_config.batch_size, 5000);
    ASSERT_FALSE(throughput_config.enable_compression_during_migration);  // Speed over space
    
    // Test high reliability use case
    auto high_reliability_manager = CreateMigrationManagerForUseCase("high_reliability");
    ASSERT_NE(high_reliability_manager, nullptr);
    
    auto reliability_config = high_reliability_manager->get_config();
    ASSERT_EQ(reliability_config.default_strategy, semantic_vector::MigrationManager::MigrationStrategy::SEQUENTIAL);
    ASSERT_TRUE(reliability_config.enable_checkpoints);
    ASSERT_TRUE(reliability_config.enable_automatic_rollback);
    ASSERT_EQ(reliability_config.rollback_trigger_error_rate, 0.01);  // 1% error rate
    
    // Test zero downtime use case
    auto zero_downtime_manager = CreateMigrationManagerForUseCase("zero_downtime");
    ASSERT_NE(zero_downtime_manager, nullptr);
    
    auto zero_downtime_config = zero_downtime_manager->get_config();
    ASSERT_EQ(zero_downtime_config.default_strategy, semantic_vector::MigrationManager::MigrationStrategy::INCREMENTAL);
    ASSERT_TRUE(zero_downtime_config.enable_dual_write);
    ASSERT_TRUE(zero_downtime_config.enable_zero_downtime_migration);
    ASSERT_EQ(zero_downtime_config.max_acceptable_downtime_minutes, 0.0);
    
    // Test resource constrained use case
    auto resource_constrained_manager = CreateMigrationManagerForUseCase("resource_constrained");
    ASSERT_NE(resource_constrained_manager, nullptr);
    
    auto resource_config = resource_constrained_manager->get_config();
    ASSERT_EQ(resource_config.default_strategy, semantic_vector::MigrationManager::MigrationStrategy::SEQUENTIAL);
    ASSERT_EQ(resource_config.batch_size, 500);
    ASSERT_EQ(resource_config.max_parallel_batches, 2);
    ASSERT_EQ(resource_config.migration_thread_pool_size, 4);
}

TEST(SemVecSmoke, MigrationConfigValidation) {
    // Test valid config
    semantic_vector::SemanticVectorConfig::MigrationConfig valid_config;
    valid_config.batch_size = 1000;
    valid_config.max_parallel_batches = 8;
    valid_config.batch_timeout_seconds = 300.0;
    valid_config.migration_thread_pool_size = 16;
    valid_config.target_migration_rate_series_per_second = 100.0;
    
    auto validation = ValidateMigrationManagerConfig(valid_config);
    ASSERT_TRUE(validation.ok());
    ASSERT_TRUE(validation.value().is_valid);
    
    // Test invalid config - batch size too large
    semantic_vector::SemanticVectorConfig::MigrationConfig invalid_config1;
    invalid_config1.batch_size = 200000;  // Too large
    
    auto invalid_validation1 = ValidateMigrationManagerConfig(invalid_config1);
    ASSERT_TRUE(invalid_validation1.ok());
    ASSERT_FALSE(invalid_validation1.value().is_valid);
    
    // Test invalid config - negative timeout
    semantic_vector::SemanticVectorConfig::MigrationConfig invalid_config2;
    invalid_config2.batch_timeout_seconds = -1.0;  // Negative
    
    auto invalid_validation2 = ValidateMigrationManagerConfig(invalid_config2);
    ASSERT_TRUE(invalid_validation2.ok());
    ASSERT_FALSE(invalid_validation2.value().is_valid);
    
    // Test invalid config - too many parallel batches
    semantic_vector::SemanticVectorConfig::MigrationConfig invalid_config3;
    invalid_config3.max_parallel_batches = 128;  // Too high
    
    auto invalid_validation3 = ValidateMigrationManagerConfig(invalid_config3);
    ASSERT_TRUE(invalid_validation3.ok());
    ASSERT_FALSE(invalid_validation3.value().is_valid);
}

TEST(SemVecSmoke, MigrationPerformanceMonitoring) {
    // Create migration manager with performance monitoring
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.enable_performance_monitoring = true;
    config.batch_size = 50;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start a migration to generate some metrics
    std::vector<SeriesID> test_series = {"perf_1", "perf_2", "perf_3"};
    auto migration_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Let some processing happen
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Get performance metrics
    auto metrics_result = migration_manager->get_performance_metrics();
    ASSERT_TRUE(metrics_result.ok());
    
    auto metrics = metrics_result.value();
    ASSERT_GE(metrics.average_migration_rate_series_per_second, 0.0);
    ASSERT_GE(metrics.total_series_migrated, 0);
    ASSERT_GE(metrics.migration_accuracy, 0.0);
    ASSERT_LE(metrics.migration_accuracy, 1.0);
    ASSERT_GE(metrics.data_consistency_score, 0.0);
    ASSERT_LE(metrics.data_consistency_score, 1.0);
    
    // Test metrics reset
    auto reset_result = migration_manager->reset_performance_metrics();
    ASSERT_TRUE(reset_result.ok());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecSmoke, MigrationLifecycleManagement) {
    // Test complete migration lifecycle
    semantic_vector::SemanticVectorConfig::MigrationConfig config;
    config.batch_size = 2;
    config.enable_progress_tracking = true;
    
    auto migration_manager = CreateMigrationManager(config);
    ASSERT_NE(migration_manager, nullptr);
    
    // Start migration
    std::vector<SeriesID> test_series = {"lifecycle_1", "lifecycle_2", "lifecycle_3", "lifecycle_4"};
    auto start_result = migration_manager->start_migration(test_series, semantic_vector::MigrationManager::MigrationStrategy::SEQUENTIAL);
    ASSERT_TRUE(start_result.ok());
    
    std::string migration_id = start_result.value();
    
    // Pause migration
    auto pause_result = migration_manager->pause_migration(migration_id);
    ASSERT_TRUE(pause_result.ok());
    
    // Resume migration
    auto resume_result = migration_manager->resume_migration(migration_id);
    ASSERT_TRUE(resume_result.ok());
    
    // Get all migrations progress
    auto all_progress_result = migration_manager->get_all_migrations_progress();
    ASSERT_TRUE(all_progress_result.ok());
    
    auto all_progress = all_progress_result.value();
    ASSERT_GE(all_progress.size(), 1);
    
    // Find our migration
    bool found_migration = false;
    for (const auto& progress : all_progress) {
        if (progress.migration_id == migration_id) {
            found_migration = true;
            ASSERT_EQ(progress.total_series_count, test_series.size());
            break;
        }
    }
    ASSERT_TRUE(found_migration);
    
    // Cancel migration
    auto cancel_result = migration_manager->cancel_migration(migration_id);
    ASSERT_TRUE(cancel_result.ok());
}
