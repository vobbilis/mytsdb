#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_CAUSAL_INFERENCE_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_CAUSAL_INFERENCE_H_

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
 * @brief Causal Inference Implementation
 * 
 * Provides advanced causal inference capabilities including Granger causality,
 * PC algorithm, and multi-variate causal discovery for time series data.
 * Updated for architecture unity and linter cache refresh.
 */
class CausalInferenceImpl {
public:
    explicit CausalInferenceImpl(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);
    virtual ~CausalInferenceImpl() = default;
    
    // Causal inference operations
    core::Result<std::vector<core::Correlation>> analyze_causality(const std::vector<core::SeriesID>& series_ids);
    core::Result<core::Correlation> test_granger_causality(const core::SeriesID& cause_series, const core::SeriesID& effect_series);
    core::Result<std::vector<core::Correlation>> discover_causal_network(const std::vector<core::SeriesID>& series_ids);
    core::Result<std::vector<core::Correlation>> pc_algorithm_discovery(const std::vector<core::SeriesID>& series_ids);
    
    // Causal relationship analysis
    core::Result<core::CausalInference::Direction> determine_causal_direction(const core::SeriesID& series_a, const core::SeriesID& series_b);
    core::Result<core::CausalInference::Strength> assess_causal_strength(const core::Correlation& correlation);
    core::Result<std::vector<core::SeriesID>> find_causal_ancestors(const core::SeriesID& target_series);
    core::Result<std::vector<core::SeriesID>> find_causal_descendants(const core::SeriesID& target_series);
    
    // Algorithm configuration
    core::Result<void> set_causal_algorithm(core::CausalInference::Algorithm algorithm);
    core::Result<void> update_significance_threshold(double threshold);
    core::Result<void> configure_lag_parameters(size_t max_lag, size_t optimal_lag);
    
    // Performance monitoring  
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);
    core::semantic_vector::SemanticVectorConfig::AnalyticsConfig get_config() const;

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_causality_analysis_time_ms{0.0};
        std::atomic<double> average_granger_test_time_ms{0.0};
        std::atomic<size_t> total_causality_analyses{0};
        std::atomic<size_t> total_granger_tests{0};
        std::atomic<size_t> total_causal_relationships_found{0};
        std::atomic<double> average_causal_discovery_time_ms{0.0};
        std::atomic<size_t> causality_analysis_errors{0};
        std::atomic<size_t> granger_test_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::AnalyticsConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal analytics structures
    std::unique_ptr<class GrangerCausalityEngine> granger_engine_;
    std::unique_ptr<class PCAlgorithmEngine> pc_algorithm_engine_;
    std::unique_ptr<class CausalNetworkBuilder> network_builder_;
    std::unique_ptr<class StatisticalTestSuite> statistical_tests_;
    
    // Internal helper methods
    core::Result<void> initialize_causal_inference_structures();
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<CausalInferenceImpl> CreateCausalInference(
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);

std::unique_ptr<CausalInferenceImpl> CreateCausalInferenceForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& base_config = core::semantic_vector::SemanticVectorConfig::AnalyticsConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateCausalInferenceConfig(
    const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_CAUSAL_INFERENCE_H_