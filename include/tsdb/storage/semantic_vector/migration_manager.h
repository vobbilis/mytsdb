#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_MIGRATION_MANAGER_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_MIGRATION_MANAGER_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <queue>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Migration Manager Implementation
 * 
 * Provides comprehensive migration capabilities including progress tracking,
 * batch processing, rollback mechanisms, and status reporting.
 * Updated for architecture unity and linter cache refresh.
 */
class MigrationManagerImpl {
public:
    explicit MigrationManagerImpl(const core::semantic_vector::SemanticVectorConfig::MigrationConfig& config);
    virtual ~MigrationManagerImpl() = default;
    
    // Migration lifecycle management
    core::Result<std::string> start_migration(const std::vector<core::SeriesID>& series_to_migrate, core::MigrationManager::MigrationStrategy strategy);
    core::Result<void> pause_migration(const std::string& migration_id);
    core::Result<void> resume_migration(const std::string& migration_id);
    core::Result<void> cancel_migration(const std::string& migration_id);
    core::Result<void> wait_for_completion(const std::string& migration_id, double timeout_seconds = 0.0);
    
    // Migration progress tracking
    core::Result<core::MigrationProgress> get_migration_progress(const std::string& migration_id);
    core::Result<std::vector<core::MigrationProgress>> get_all_migrations_progress();
    core::Result<void> update_migration_progress(const std::string& migration_id, const core::MigrationProgress& progress);
    
    // Batch migration operations
    core::Result<std::vector<core::MigrationBatch>> create_migration_batches(const std::vector<core::SeriesID>& series_ids, size_t batch_size);
    core::Result<void> process_migration_batch(const std::string& migration_id, core::MigrationBatch& batch);
    core::Result<void> retry_failed_batch(const std::string& migration_id, size_t batch_id);
    core::Result<std::vector<core::MigrationBatch>> get_failed_batches(const std::string& migration_id);
    
    // Checkpoint and rollback operations
    core::Result<std::string> create_checkpoint(const std::string& migration_id);
    core::Result<void> rollback_to_checkpoint(const std::string& migration_id, const std::string& checkpoint_id);
    core::Result<void> rollback_migration(const std::string& migration_id, core::MigrationManager::RollbackStrategy strategy);
    core::Result<std::vector<core::MigrationCheckpoint>> get_migration_checkpoints(const std::string& migration_id);
    core::Result<void> cleanup_old_checkpoints(const std::string& migration_id, size_t keep_count);
    
    // Status reporting and monitoring
    core::Result<core::MigrationStatusReport> generate_status_report(const std::string& migration_id);
    core::Result<std::vector<core::MigrationStatusReport>> generate_all_status_reports();
    core::Result<void> export_migration_logs(const std::string& migration_id, const std::string& output_path);
    
    // Data validation and integrity
    core::Result<bool> validate_migration_data(const std::string& migration_id, const std::vector<core::SeriesID>& sample_series);
    core::Result<double> calculate_data_consistency_score(const std::string& migration_id);
    core::Result<void> verify_data_integrity(const std::string& migration_id);
    core::Result<std::vector<std::string>> detect_data_corruption(const std::string& migration_id);
    
    // Performance optimization
    core::Result<void> optimize_migration_performance(const std::string& migration_id);
    core::Result<void> adjust_batch_size(const std::string& migration_id, size_t new_batch_size);
    core::Result<void> scale_migration_workers(const std::string& migration_id, size_t worker_count);
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::MigrationConfig& config);
    core::semantic_vector::SemanticVectorConfig::MigrationConfig get_config() const;
    
    // Performance monitoring
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_migration_rate_series_per_second{0.0};
        std::atomic<double> average_batch_processing_time_seconds{0.0};
        std::atomic<size_t> total_series_migrated{0};
        std::atomic<size_t> total_batches_processed{0};
        std::atomic<size_t> total_migration_errors{0};
        std::atomic<size_t> total_rollbacks_performed{0};
        std::atomic<double> average_data_consistency_score{1.0};
        std::atomic<size_t> checkpoints_created{0};
        std::atomic<size_t> validations_performed{0};
    };
    
    // Migration state management
    struct MigrationState {
        core::MigrationProgress progress;
        std::vector<core::MigrationBatch> batches;
        std::vector<core::MigrationCheckpoint> checkpoints;
        std::vector<core::MigrationStatusReport> status_reports;
        std::atomic<bool> is_active{false};
        std::atomic<bool> is_paused{false};
        std::atomic<bool> should_cancel{false};
        std::thread worker_thread;
        mutable std::shared_mutex state_mutex;
    };
    
    // Configuration and monitoring
    core::semantic_vector::SemanticVectorConfig::MigrationConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex global_mutex_;
    
    // Migration state tracking
    std::map<std::string, std::unique_ptr<MigrationState>> active_migrations_;
    std::queue<std::string> completed_migrations_;
    std::queue<std::string> failed_migrations_;
    
    // Internal worker structures
    std::unique_ptr<class BatchProcessor> batch_processor_;
    std::unique_ptr<class CheckpointManager> checkpoint_manager_;
    std::unique_ptr<class ValidationEngine> validation_engine_;
    std::unique_ptr<class PerformanceOptimizer> performance_optimizer_;
    std::unique_ptr<class StatusReporter> status_reporter_;
    
    // Internal helper methods
    core::Result<void> initialize_migration_structures();
    core::Result<void> execute_migration_worker(const std::string& migration_id);
    core::Result<void> process_migration_batches(MigrationState& state);
    core::Result<void> handle_migration_error(const std::string& migration_id, const std::string& error_message);
    core::Result<void> cleanup_completed_migration(const std::string& migration_id);
    core::Result<void> update_performance_metrics(const std::string& operation, double value, bool success) const;
    core::Result<std::string> generate_migration_id() const;
    core::Result<bool> should_create_checkpoint(const MigrationState& state) const;
    core::Result<bool> should_trigger_rollback(const MigrationState& state) const;
};

// Factory functions
std::unique_ptr<MigrationManagerImpl> CreateMigrationManager(
    const core::semantic_vector::SemanticVectorConfig::MigrationConfig& config);

std::unique_ptr<MigrationManagerImpl> CreateMigrationManagerForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::MigrationConfig& base_config = core::semantic_vector::SemanticVectorConfig::MigrationConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateMigrationManagerConfig(
    const core::semantic_vector::SemanticVectorConfig::MigrationConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_MIGRATION_MANAGER_H_