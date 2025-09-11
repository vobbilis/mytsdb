#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_REASONING_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_REASONING_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Temporal Reasoning Implementation
 * 
 * Provides advanced temporal reasoning capabilities including pattern recognition,
 * trend analysis, anomaly detection, and forecasting for time series data.
 * Updated for architecture unity and linter cache refresh.
 */
class TemporalReasoningImpl {
public:
    explicit TemporalReasoningImpl(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);
    virtual ~TemporalReasoningImpl() = default;
    
    // Temporal reasoning operations
    core::Result<std::vector<core::Anomaly>> detect_anomalies(const core::SeriesID& series_id);
    core::Result<std::vector<core::Prediction>> generate_predictions(const core::SeriesID& series_id, size_t forecast_horizon);
    core::Result<std::vector<core::Correlation>> analyze_temporal_correlations(const std::vector<core::SeriesID>& series_ids);
    core::Result<std::vector<core::Correlation>> detect_lagged_correlations(const std::vector<core::SeriesID>& series_ids, size_t max_lag);
    
    // Pattern recognition operations
    core::Result<std::vector<core::TemporalReasoning::PatternType>> recognize_patterns(const core::SeriesID& series_id);
    core::Result<std::vector<core::SeriesID>> find_similar_patterns(const core::SeriesID& reference_series, double similarity_threshold);
    core::Result<double> calculate_pattern_similarity(const core::SeriesID& series_a, const core::SeriesID& series_b);
    core::Result<bool> validate_seasonal_pattern(const core::SeriesID& series_id, size_t expected_period);
    
    // Trend analysis operations
    core::Result<double> calculate_trend_strength(const core::SeriesID& series_id);
    core::Result<std::vector<double>> decompose_seasonal_trend(const core::SeriesID& series_id);
    core::Result<bool> detect_regime_change(const core::SeriesID& series_id);
    core::Result<std::vector<std::chrono::system_clock::time_point>> find_breakpoints(const core::SeriesID& series_id);
    
    // Configuration and optimization
    core::Result<void> set_reasoning_type(core::TemporalReasoning::ReasoningType reasoning_type);
    core::Result<void> update_pattern_threshold(double threshold);
    core::Result<void> configure_anomaly_detection(double threshold, size_t window_size);
    
    // Performance monitoring  
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);
    core::semantic_vector::SemanticVectorConfig::AnalyticsConfig get_config() const;

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_anomaly_detection_time_ms{0.0};
        std::atomic<double> average_prediction_time_ms{0.0};
        std::atomic<size_t> total_anomaly_detections{0};
        std::atomic<size_t> total_predictions_generated{0};
        std::atomic<size_t> total_patterns_recognized{0};
        std::atomic<double> average_pattern_recognition_time_ms{0.0};
        std::atomic<size_t> anomaly_detection_errors{0};
        std::atomic<size_t> prediction_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::AnalyticsConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal reasoning structures
    std::unique_ptr<class AnomalyDetectionEngine> anomaly_engine_;
    std::unique_ptr<class ForecastingEngine> forecasting_engine_;
    std::unique_ptr<class PatternRecognitionEngine> pattern_engine_;
    std::unique_ptr<class TrendAnalysisEngine> trend_engine_;
    std::unique_ptr<class SeasonalDecompositionEngine> seasonal_engine_;
    
    // Internal helper methods
    core::Result<void> initialize_temporal_reasoning_structures();
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<TemporalReasoningImpl> CreateTemporalReasoning(
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);

std::unique_ptr<TemporalReasoningImpl> CreateTemporalReasoningForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& base_config = core::semantic_vector::SemanticVectorConfig::AnalyticsConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateTemporalReasoningConfig(
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_REASONING_H_