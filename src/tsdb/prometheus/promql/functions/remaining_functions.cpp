#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace tsdb {
namespace prometheus {
namespace promql {

// Helper function to calculate quantile (reused from aggregation_advanced.cpp)
static double CalculateQuantile(std::vector<double>& values, double phi) {
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    std::sort(values.begin(), values.end());
    
    if (phi < 0) phi = 0;
    if (phi > 1) phi = 1;
    
    if (phi == 0) return values.front();
    if (phi == 1) return values.back();
    
    size_t n = values.size();
    double pos = phi * (n - 1);
    size_t lower = static_cast<size_t>(std::floor(pos));
    size_t upper = static_cast<size_t>(std::ceil(pos));
    
    if (lower == upper) {
        return values[lower];
    }
    
    double fraction = pos - lower;
    return values[lower] * (1 - fraction) + values[upper] * fraction;
}

void RegisterOverTimeAggregations(FunctionRegistry& registry) {
    // ==================== BASIC OVER-TIME AGGREGATIONS ====================
    
    // sum_over_time(range-vector) - Sum of all values in the time window
    registry.Register({
        "sum_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double sum = 0.0;
                for (const auto& sample : series.samples) {
                    sum += sample.value();
                }
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), sum});
            }
            return Value(result);
        }
    });
    
    // avg_over_time(range-vector) - Average of all values in the time window
    registry.Register({
        "avg_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double sum = 0.0;
                for (const auto& sample : series.samples) {
                    sum += sample.value();
                }
                double avg = sum / series.samples.size();
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), avg});
            }
            return Value(result);
        }
    });
    
    // min_over_time(range-vector) - Minimum value in the time window
    registry.Register({
        "min_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double min_val = series.samples[0].value();
                for (const auto& sample : series.samples) {
                    min_val = std::min(min_val, sample.value());
                }
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), min_val});
            }
            return Value(result);
        }
    });
    
    // max_over_time(range-vector) - Maximum value in the time window
    registry.Register({
        "max_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double max_val = series.samples[0].value();
                for (const auto& sample : series.samples) {
                    max_val = std::max(max_val, sample.value());
                }
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), max_val});
            }
            return Value(result);
        }
    });
    
    // count_over_time(range-vector) - Count of samples in the time window
    registry.Register({
        "count_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double count = static_cast<double>(series.samples.size());
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), count});
            }
            return Value(result);
        }
    });

    // quantile_over_time(φ scalar, range-vector) - Quantile of values over time
    registry.Register({
        "quantile_over_time",
        {ValueType::SCALAR, ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            double phi = args[0].getScalar().value;
            const auto& matrix = args[1].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                std::vector<double> values;
                for (const auto& sample : series.samples) {
                    values.push_back(sample.value());
                }
                
                double q = CalculateQuantile(values, phi);
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), q});
            }
            return Value(result);
        }
    });
    
    // stddev_over_time(range-vector) - Standard deviation over time
    registry.Register({
        "stddev_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double sum = 0.0;
                double sum_sq = 0.0;
                size_t count = 0;
                
                for (const auto& sample : series.samples) {
                    double val = sample.value();
                    sum += val;
                    sum_sq += val * val;
                    count++;
                }
                
                if (count == 0) continue;
                
                double mean = sum / count;
                double variance = (sum_sq / count) - (mean * mean);
                double stddev = std::sqrt(variance);
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), stddev});
            }
            return Value(result);
        }
    });
    
    // stdvar_over_time(range-vector) - Standard variance over time
    registry.Register({
        "stdvar_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double sum = 0.0;
                double sum_sq = 0.0;
                size_t count = 0;
                
                for (const auto& sample : series.samples) {
                    double val = sample.value();
                    sum += val;
                    sum_sq += val * val;
                    count++;
                }
                
                if (count == 0) continue;
                
                double mean = sum / count;
                double variance = (sum_sq / count) - (mean * mean);
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), variance});
            }
            return Value(result);
        }
    });
    
    // last_over_time(range-vector) - Last value in time window
    registry.Register({
        "last_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                // Last sample is the most recent
                const auto& last = series.samples.back();
                result.push_back(Sample{series.metric, last.timestamp(), last.value()});
            }
            return Value(result);
        }
    });
    
    // present_over_time(range-vector) - Returns 1 if series has any values
    registry.Register({
        "present_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (!series.samples.empty()) {
                    result.push_back(Sample{series.metric, series.samples.back().timestamp(), 1.0});
                }
            }
            return Value(result);
        }
    });
    
    // absent_over_time(range-vector) - Returns 1 if series has no values
    registry.Register({
        "absent_over_time",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            // If matrix is empty or all series are empty, return 1
            bool has_values = false;
            for (const auto& series : matrix) {
                if (!series.samples.empty()) {
                    has_values = true;
                    break;
                }
            }
            
            if (!has_values) {
                LabelSet empty_labels;
                result.push_back(Sample{empty_labels, eval->timestamp(), 1.0});
            }
            
            return Value(result);
        }
    });
}

void RegisterRemainingAggregations(FunctionRegistry& registry) {
    // group(vector) - Returns 1 for each unique label set
    registry.Register({
        "group",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            
            Vector result;
            for (const auto& sample : vec) {
                // Return 1 for each unique label set
                result.push_back(Sample{sample.metric, sample.timestamp, 1.0});
            }
            return Value(result);
        }
    });
    
    // count_values(label, vector) - Count occurrences of each value
    registry.Register({
        "count_values",
        {ValueType::STRING, ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            std::string label_name = args[0].getString().value;
            const auto& vec = args[1].getVector();
            
            // Map from value to count
            std::map<double, int> value_counts;
            int64_t timestamp = 0;
            LabelSet base_labels;  // Use first sample's labels as base
            
            for (const auto& sample : vec) {
                timestamp = sample.timestamp;
                if (base_labels.labels().empty()) {
                    base_labels = sample.metric;
                }
                value_counts[sample.value]++;
            }
            
            Vector result;
            for (const auto& [value, count] : value_counts) {
                LabelSet labels = base_labels;
                // Add the value as a label
                labels.AddLabel(label_name, std::to_string(value));
                result.push_back(Sample{labels, timestamp, static_cast<double>(count)});
            }
            
            return Value(result);
        }
    });
}

void RegisterRemainingUtilityFunctions(FunctionRegistry& registry) {
    // sort_by_label(v, label) - Sort by label value (simplified to single label)
    registry.Register({
        "sort_by_label",
        {ValueType::VECTOR, ValueType::STRING},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            if (args.empty()) return Value(Vector{});
            
            auto vec = args[0].getVector();
            std::string label_name = args.size() > 1 ? std::string(args[1].getString().value) : "__name__";
            
            // Sort by label value
            std::sort(vec.begin(), vec.end(), [&label_name](const Sample& a, const Sample& b) {
                auto a_val = a.metric.GetLabelValue(label_name);
                auto b_val = b.metric.GetLabelValue(label_name);
                
                std::string a_str = a_val.has_value() ? a_val.value() : "";
                std::string b_str = b_val.has_value() ? b_val.value() : "";
                
                return a_str < b_str;
            });
            
            return Value(vec);
        }
    });
    
    // sort_by_label_desc(v, label) - Sort by label value descending
    registry.Register({
        "sort_by_label_desc",
        {ValueType::VECTOR, ValueType::STRING},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            if (args.empty()) return Value(Vector{});
            
            auto vec = args[0].getVector();
            std::string label_name = args.size() > 1 ? std::string(args[1].getString().value) : "__name__";
            
            // Sort by label value descending
            std::sort(vec.begin(), vec.end(), [&label_name](const Sample& a, const Sample& b) {
                auto a_val = a.metric.GetLabelValue(label_name);
                auto b_val = b.metric.GetLabelValue(label_name);
                
                std::string a_str = a_val.has_value() ? a_val.value() : "";
                std::string b_str = b_val.has_value() ? b_val.value() : "";
                
                return a_str > b_str;  // Descending
            });
            
            return Value(vec);
        }
    });

    // changes(range-vector) - Returns number of times value changed
    registry.Register({
        "changes",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.empty()) continue;
                
                double changes_count = 0;
                if (series.samples.size() > 1) {
                    for (size_t i = 1; i < series.samples.size(); ++i) {
                        if (series.samples[i].value() != series.samples[i-1].value()) {
                            changes_count++;
                        }
                    }
                }
                
                // Result timestamp is the timestamp of the last sample (or evaluation timestamp? usually last sample in range vector aggregations)
                // Standard PromQL behavior for range vector functions is usually the evaluation timestamp, 
                // but here we use the last sample's timestamp to be consistent with other over_time functions in this file.
                // Actually, for instant vector result from range vector, it should be the evaluation timestamp.
                // But our simplified implementation often uses the last sample's timestamp. 
                // Let's stick to last sample's timestamp for consistency with other functions here.
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), changes_count});
            }
            return Value(result);
        }
    });

    // resets(range-vector) - Number of counter resets within the time window
    registry.Register({
        "resets",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.size() < 2) {
                    result.push_back(Sample{series.metric, series.samples.empty() ? 0 : series.samples.back().timestamp(), 0.0});
                    continue;
                }
                
                double resets_count = 0;
                for (size_t i = 1; i < series.samples.size(); ++i) {
                    if (series.samples[i].value() < series.samples[i-1].value()) {
                        resets_count++;
                    }
                }
                
                result.push_back(Sample{series.metric, series.samples.back().timestamp(), resets_count});
            }
            return Value(result);
        }
    });

    // idelta(range-vector) - Delta of the last two samples in the time window
    registry.Register({
        "idelta",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            
            Vector result;
            for (const auto& series : matrix) {
                if (series.samples.size() < 2) continue;
                
                // Use last two samples
                const auto& last = series.samples.back();
                const auto& prev = series.samples[series.samples.size() - 2];
                
                double delta = last.value() - prev.value();
                result.push_back(Sample{series.metric, last.timestamp(), delta});
            }
            return Value(result);
        }
    });

    // timestamp(vector) - Returns the timestamp of each sample
    registry.Register({
        "timestamp",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            
            Vector result;
            for (const auto& sample : vec) {
                // Return timestamp in seconds (PromQL convention)
                double ts_seconds = static_cast<double>(sample.timestamp) / 1000.0;
                result.push_back(Sample{sample.metric, sample.timestamp, ts_seconds});
            }
            return Value(result);
        }
    });
}

// Helper for histogram_quantile: extract bucket boundaries from "le" label
static double ExtractLeBound(const LabelSet& labels) {
    auto le = labels.GetLabelValue("le");
    if (!le.has_value()) return std::numeric_limits<double>::quiet_NaN();
    const std::string& le_str = le.value();
    if (le_str == "+Inf") return std::numeric_limits<double>::infinity();
    try {
        return std::stod(le_str);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

void RegisterHistogramFunctions(FunctionRegistry& registry) {
    // histogram_quantile(scalar, vector) - Calculate quantile from histogram buckets
    // Critical for P99 latency queries like histogram_quantile(0.99, rate(http_request_duration_seconds_bucket[5m]))
    registry.Register({
        "histogram_quantile",
        {ValueType::SCALAR, ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            double phi = args[0].getScalar().value;
            const auto& input_vec = args[1].getVector();
            
            if (phi < 0 || phi > 1) {
                return Value(Vector{});  // Invalid quantile
            }
            
            // Group samples by series (excluding "le" label)
            struct BucketGroup {
                LabelSet base_labels;
                std::vector<std::pair<double, double>> buckets;  // (le, count)
                int64_t timestamp;
            };
            
            std::map<std::string, BucketGroup> groups;
            
            for (const auto& sample : input_vec) {
                // Create base labels without "le"
                LabelSet base = sample.metric;
                base.RemoveLabel("le");
                std::string key = base.ToString();
                
                auto& group = groups[key];
                if (group.base_labels.labels().empty()) {
                    group.base_labels = base;
                    group.timestamp = sample.timestamp;
                }
                
                double le = ExtractLeBound(sample.metric);
                if (!std::isnan(le)) {
                    group.buckets.emplace_back(le, sample.value);
                }
            }
            
            Vector result;
            for (auto& [key, group] : groups) {
                if (group.buckets.empty()) continue;
                
                // Sort buckets by le
                std::sort(group.buckets.begin(), group.buckets.end());
                
                // Find total count (from +Inf bucket)
                double total = 0;
                for (const auto& [le, count] : group.buckets) {
                    if (std::isinf(le)) {
                        total = count;
                        break;
                    }
                }
                
                if (total == 0) {
                    result.push_back(Sample{group.base_labels, group.timestamp, std::numeric_limits<double>::quiet_NaN()});
                    continue;
                }
                
                // Calculate quantile using linear interpolation
                double target = phi * total;
                double prev_le = 0;
                double prev_count = 0;
                double quantile_value = std::numeric_limits<double>::quiet_NaN();
                
                for (const auto& [le, count] : group.buckets) {
                    if (count >= target) {
                        // Interpolate
                        double bucket_size = count - prev_count;
                        if (bucket_size > 0) {
                            double fraction = (target - prev_count) / bucket_size;
                            quantile_value = prev_le + fraction * (le - prev_le);
                        } else {
                            quantile_value = le;
                        }
                        break;
                    }
                    prev_le = le;
                    prev_count = count;
                }
                
                result.push_back(Sample{group.base_labels, group.timestamp, quantile_value});
            }
            
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb

