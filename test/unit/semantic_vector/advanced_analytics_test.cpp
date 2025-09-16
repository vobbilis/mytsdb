#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/causal_inference.h"
#include "tsdb/storage/semantic_vector/temporal_reasoning.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
using namespace tsdb::core;

TEST(SemVecSmoke, CausalInferenceBasic) {
    // Create basic analytics config
    semantic_vector::SemanticVectorConfig::AnalyticsConfig config;
    config.enable_causal_inference = true;
    config.causal_algorithm = semantic_vector::CausalInference::Algorithm::GRANGER_CAUSALITY;
    config.causal_significance_threshold = 0.05;
    
    // Create causal inference instance
    auto causal_inference = CreateCausalInference(config);
    ASSERT_NE(causal_inference, nullptr);
    
    // Test basic causality analysis
    std::vector<SeriesID> series_ids = {"series1", "series2", "series3"};
    auto causality_result = causal_inference->analyze_causality(series_ids);
    ASSERT_TRUE(causality_result.is_ok());
    
    auto correlations = causality_result.value();
    ASSERT_GT(correlations.size(), 0);
    
    // Test Granger causality test
    auto granger_result = causal_inference->test_granger_causality("series1", "series2");
    ASSERT_TRUE(granger_result.is_ok());
    
    auto correlation = granger_result.value();
    ASSERT_EQ(correlation.series_a, "series1");
    ASSERT_EQ(correlation.series_b, "series2");
    ASSERT_GE(correlation.confidence, 0.0);
    ASSERT_LE(correlation.confidence, 1.0);
    
    // Test causal direction determination
    auto direction_result = causal_inference->determine_causal_direction("series1", "series2");
    ASSERT_TRUE(direction_result.is_ok());
    
    // Test causal strength assessment
    auto strength_result = causal_inference->assess_causal_strength(correlation);
    ASSERT_TRUE(strength_result.is_ok());
    
    // Test performance metrics
    auto metrics_result = causal_inference->get_performance_metrics();
    ASSERT_TRUE(metrics_result.is_ok());
}

TEST(SemVecSmoke, TemporalReasoningBasic) {
    // Create basic analytics config
    semantic_vector::SemanticVectorConfig::AnalyticsConfig config;
    config.enable_temporal_reasoning = true;
    config.reasoning_type = semantic_vector::TemporalReasoning::ReasoningType::PATTERN_RECOGNITION;
    config.enable_anomaly_detection = true;
    config.anomaly_threshold = 3.0;
    
    // Create temporal reasoning instance
    auto temporal_reasoning = CreateTemporalReasoning(config);
    ASSERT_NE(temporal_reasoning, nullptr);
    
    // Test anomaly detection
    auto anomaly_result = temporal_reasoning->detect_anomalies("test_series");
    ASSERT_TRUE(anomaly_result.is_ok());
    
    auto anomalies = anomaly_result.value();
    ASSERT_GE(anomalies.size(), 0);
    
    // If anomalies were found, verify their structure
    if (!anomalies.empty()) {
        auto& anomaly = anomalies[0];
        ASSERT_EQ(anomaly.series_id, "test_series");
        ASSERT_GE(anomaly.confidence, 0.0);
        ASSERT_LE(anomaly.confidence, 1.0);
        ASSERT_FALSE(anomaly.anomaly_type.empty());
    }
    
    // Test prediction generation
    auto prediction_result = temporal_reasoning->generate_predictions("test_series", 5);
    ASSERT_TRUE(prediction_result.is_ok());
    
    auto predictions = prediction_result.value();
    ASSERT_EQ(predictions.size(), 5);
    
    for (const auto& prediction : predictions) {
        ASSERT_EQ(prediction.series_id, "test_series");
        ASSERT_GE(prediction.prediction_confidence, 0.0);
        ASSERT_LE(prediction.prediction_confidence, 1.0);
        ASSERT_LE(prediction.confidence_interval_low, prediction.predicted_value);
        ASSERT_GE(prediction.confidence_interval_high, prediction.predicted_value);
    }
    
    // Test pattern recognition
    auto pattern_result = temporal_reasoning->recognize_patterns("test_series");
    ASSERT_TRUE(pattern_result.is_ok());
    
    auto patterns = pattern_result.value();
    ASSERT_GE(patterns.size(), 0);
    
    // Test performance metrics
    auto metrics_result = temporal_reasoning->get_performance_metrics();
    ASSERT_TRUE(metrics_result.is_ok());
}

TEST(SemVecSmoke, AnalyticsUseCases) {
    // Test causal inference use cases
    auto high_accuracy_causal = CreateCausalInferenceForUseCase("high_accuracy");
    ASSERT_NE(high_accuracy_causal, nullptr);
    
    auto high_speed_causal = CreateCausalInferenceForUseCase("high_speed");
    ASSERT_NE(high_speed_causal, nullptr);
    
    auto comprehensive_causal = CreateCausalInferenceForUseCase("comprehensive");
    ASSERT_NE(comprehensive_causal, nullptr);
    
    // Test temporal reasoning use cases
    auto anomaly_detection_temporal = CreateTemporalReasoningForUseCase("anomaly_detection");
    ASSERT_NE(anomaly_detection_temporal, nullptr);
    
    auto forecasting_temporal = CreateTemporalReasoningForUseCase("forecasting");
    ASSERT_NE(forecasting_temporal, nullptr);
    
    auto pattern_analysis_temporal = CreateTemporalReasoningForUseCase("pattern_analysis");
    ASSERT_NE(pattern_analysis_temporal, nullptr);
    
    auto comprehensive_temporal = CreateTemporalReasoningForUseCase("comprehensive");
    ASSERT_NE(comprehensive_temporal, nullptr);
}

TEST(SemVecSmoke, AnalyticsConfigValidation) {
    // Test causal inference config validation
    semantic_vector::SemanticVectorConfig::AnalyticsConfig valid_causal_config;
    valid_causal_config.causal_significance_threshold = 0.05;
    valid_causal_config.max_causal_lag = 10;
    valid_causal_config.target_inference_time_ms = 50.0;
    
    auto causal_validation = ValidateCausalInferenceConfig(valid_causal_config);
    ASSERT_TRUE(causal_validation.is_ok());
    ASSERT_TRUE(causal_validation.value().is_valid);
    
    // Test invalid causal config
    semantic_vector::SemanticVectorConfig::AnalyticsConfig invalid_causal_config;
    invalid_causal_config.causal_significance_threshold = 0.5; // Too high
    invalid_causal_config.max_causal_lag = 0; // Too low
    
    auto invalid_causal_validation = ValidateCausalInferenceConfig(invalid_causal_config);
    ASSERT_TRUE(invalid_causal_validation.is_ok());
    ASSERT_FALSE(invalid_causal_validation.value().is_valid);
    
    // Test temporal reasoning config validation
    semantic_vector::SemanticVectorConfig::AnalyticsConfig valid_temporal_config;
    valid_temporal_config.pattern_threshold = 0.7;
    valid_temporal_config.anomaly_threshold = 3.0;
    valid_temporal_config.min_pattern_length = 10;
    valid_temporal_config.target_reasoning_time_ms = 30.0;
    
    auto temporal_validation = ValidateTemporalReasoningConfig(valid_temporal_config);
    ASSERT_TRUE(temporal_validation.is_ok());
    ASSERT_TRUE(temporal_validation.value().is_valid);
    
    // Test invalid temporal config
    semantic_vector::SemanticVectorConfig::AnalyticsConfig invalid_temporal_config;
    invalid_temporal_config.pattern_threshold = 1.5; // Too high
    invalid_temporal_config.anomaly_threshold = 0.5; // Too low
    
    auto invalid_temporal_validation = ValidateTemporalReasoningConfig(invalid_temporal_config);
    ASSERT_TRUE(invalid_temporal_validation.is_ok());
    ASSERT_FALSE(invalid_temporal_validation.value().is_valid);
}

TEST(SemVecSmoke, AdvancedAnalyticsIntegration) {
    // Test integrated causal inference and temporal reasoning workflow
    semantic_vector::SemanticVectorConfig::AnalyticsConfig config;
    config.enable_causal_inference = true;
    config.enable_temporal_reasoning = true;
    config.enable_anomaly_detection = true;
    config.causal_algorithm = semantic_vector::CausalInference::Algorithm::GRANGER_CAUSALITY;
    config.reasoning_type = semantic_vector::TemporalReasoning::ReasoningType::CORRELATION_ANALYSIS;
    
    auto causal_inference = CreateCausalInference(config);
    auto temporal_reasoning = CreateTemporalReasoning(config);
    
    ASSERT_NE(causal_inference, nullptr);
    ASSERT_NE(temporal_reasoning, nullptr);
    
    // Test workflow: detect anomalies, then find causal relationships
    std::vector<SeriesID> series_ids = {"anomaly_series", "potential_cause"};
    
    // Step 1: Detect anomalies
    auto anomalies = temporal_reasoning->detect_anomalies("anomaly_series");
    ASSERT_TRUE(anomalies.is_ok());
    
    // Step 2: Find potential causal relationships
    auto causality = causal_inference->analyze_causality(series_ids);
    ASSERT_TRUE(causality.is_ok());
    
    // Step 3: Generate predictions
    auto predictions = temporal_reasoning->generate_predictions("anomaly_series", 3);
    ASSERT_TRUE(predictions.is_ok());
    ASSERT_EQ(predictions.value().size(), 3);
    
    // Verify both components can provide performance metrics
    auto causal_metrics = causal_inference->get_performance_metrics();
    auto temporal_metrics = temporal_reasoning->get_performance_metrics();
    
    ASSERT_TRUE(causal_metrics.is_ok());
    ASSERT_TRUE(temporal_metrics.is_ok());
}




























