#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/promql/query_metrics.h"
#include "tsdb/core/aggregation.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <typeinfo>
#include "tsdb/prometheus/promql/functions.h"
#include <stdexcept>
#include <map>
#include <chrono>


namespace tsdb {
namespace prometheus {
namespace promql {

// Helper to convert aggregation operator to string
std::string AggregateOpToString(TokenType op) {
    switch (op) {
        case TokenType::SUM: return "sum";
        case TokenType::AVG: return "avg";
        case TokenType::MIN: return "min";
        case TokenType::MAX: return "max";
        case TokenType::COUNT: return "count";
        case TokenType::STDDEV: return "stddev";
        case TokenType::STDVAR: return "stdvar";
        case TokenType::TOPK: return "topk";
        case TokenType::BOTTOMK: return "bottomk";
        case TokenType::QUANTILE: return "quantile";
        case TokenType::COUNT_VALUES: return "count_values";
        default: return "unknown";
    }
}

Evaluator::Evaluator(int64_t timestamp, int64_t lookback_delta, storage::StorageAdapter* storage)
    : timestamp_(timestamp), lookback_delta_(lookback_delta), storage_(storage) {}

Evaluator::Evaluator(int64_t start, int64_t end, int64_t step, int64_t lookback_delta, storage::StorageAdapter* storage)
    : timestamp_(start), start_(start), end_(end), step_(step), lookback_delta_(lookback_delta), storage_(storage) {
    if (step_ <= 0) {
        throw std::invalid_argument("Step must be positive");
    }
}

Value Evaluator::Evaluate(const ExprNode* node) {
    ScopedQueryTimer timer(ScopedQueryTimer::Type::EVAL);
    if (!node) {
        return Value{};
    }

    switch (node->type()) {
        case ExprNode::Type::AGGREGATE:
            return EvaluateAggregate(static_cast<const AggregateExprNode*>(node));
        case ExprNode::Type::BINARY:
            return EvaluateBinary(static_cast<const BinaryExprNode*>(node));
        case ExprNode::Type::CALL:
            return EvaluateCall(static_cast<const CallNode*>(node));
        case ExprNode::Type::MATRIX_SELECTOR:
            return EvaluateMatrixSelector(static_cast<const MatrixSelectorNode*>(node));
        case ExprNode::Type::NUMBER_LITERAL:
            return EvaluateNumberLiteral(static_cast<const NumberLiteralNode*>(node));
        case ExprNode::Type::PAREN:
            return EvaluateParen(static_cast<const ParenExprNode*>(node));
        case ExprNode::Type::STRING_LITERAL:
            return EvaluateStringLiteral(static_cast<const StringLiteralNode*>(node));
        case ExprNode::Type::SUBQUERY:
            return EvaluateSubquery(static_cast<const SubqueryExprNode*>(node));
        case ExprNode::Type::UNARY:
            return EvaluateUnary(static_cast<const UnaryExprNode*>(node));
        case ExprNode::Type::VECTOR_SELECTOR:
            return EvaluateVectorSelector(static_cast<const VectorSelectorNode*>(node));
        default:
            throw std::runtime_error("Unknown expression node type");
    }
}

Value Evaluator::EvaluateRange(const ExprNode* node) {
    if (!node) return Value{};
    if (step_ <= 0) {
        throw std::runtime_error("EvaluateRange requires positive step");
    }

    switch (node->type()) {
        // Optimized nodes
        case ExprNode::Type::VECTOR_SELECTOR:
            return EvaluateRangeVectorSelector(static_cast<const VectorSelectorNode*>(node));
        
        // Pass-through nodes (structural)
        case ExprNode::Type::PAREN:
            return EvaluateRange(static_cast<const ParenExprNode*>(node)->expr.get());
        
        // Literals are constant over time
        case ExprNode::Type::NUMBER_LITERAL:
        case ExprNode::Type::STRING_LITERAL:
            return EvaluateRangeLiteral(node);

        case ExprNode::Type::AGGREGATE:
            return EvaluateRangeAggregate(static_cast<const AggregateExprNode*>(node));
        case ExprNode::Type::CALL:
            return EvaluateRangeCall(static_cast<const CallNode*>(node));

        // TODO: Implement optimized versions for these
        case ExprNode::Type::BINARY:
        case ExprNode::Type::MATRIX_SELECTOR:
        case ExprNode::Type::SUBQUERY:
        case ExprNode::Type::UNARY:
        default:
            return EvaluateRangeDefault(node);
    }
}

Value Evaluator::EvaluateRangeDefault(const ExprNode* node) {
    // Fallback: Loop over steps
    struct LabelSetComparator {
        bool operator()(const LabelSet& a, const LabelSet& b) const {
            return a.labels() < b.labels();
        }
    };
    std::map<LabelSet, Series, LabelSetComparator> seriesMap;

    for (int64_t t = start_; t <= end_; t += step_) {
        timestamp_ = t;
        Value val = Evaluate(node);

        if (val.isVector()) {
            const Vector& vec = val.getVector();
            for (const auto& sample : vec) {
                auto& series = seriesMap[sample.metric];
                if (series.metric.labels().empty()) {
                        series.metric = sample.metric;
                }
                series.samples.emplace_back(t, sample.value);
            }
        } else if (val.isScalar()) {
            const Scalar& scalar = val.getScalar();
            LabelSet emptyLabels;
            auto& series = seriesMap[emptyLabels];
            series.metric = emptyLabels;
            series.samples.emplace_back(t, scalar.value);
        }
    }

    Matrix resultMatrix;
    resultMatrix.reserve(seriesMap.size());
    for (auto& pair : seriesMap) {
        resultMatrix.push_back(std::move(pair.second));
    }
    return Value(resultMatrix);
}

Value Evaluator::EvaluateRangeLiteral(const ExprNode* node) {
    // Scalar/String literal is the same value at every step
     Matrix resultMatrix;
     Series s;
     // Helper to get value
     double val = 0;
     if (node->type() == ExprNode::Type::NUMBER_LITERAL) {
         val = static_cast<const NumberLiteralNode*>(node)->value;
     } else {
         // String literal in range query? Usually not valid result unless top-level allows it?
         // Prometheus range query returns matrix. Series can't hold strings (yet, sample has double).
         // Fallback to default which might handle it (EvaluateStringLiteral returns Value(String)).
         // But here we need Matrix.
         // Let's defer to Default for String.
         return EvaluateRangeDefault(node);
     }
     
     s.metric = LabelSet{}; // Empty labels for scalar
     s.samples.reserve((end_ - start_) / step_ + 1);
     for (int64_t t = start_; t <= end_; t += step_) {
         s.samples.emplace_back(t, val);
     }
     resultMatrix.push_back(std::move(s));
     return Value(resultMatrix);
}

Value Evaluator::EvaluateRangeAggregate(const AggregateExprNode* node) {
    // 1. Evaluate child expression
    Value childResult = EvaluateRange(node->expr.get());
    
    if (childResult.type != ValueType::MATRIX) {
        return EvaluateRangeDefault(node);
    }

    const Matrix& inputMatrix = childResult.getMatrix();
    
    // 2. Prepare output structure using standard Map for series consolidation
    struct LabelSetComparator {
        bool operator()(const LabelSet& a, const LabelSet& b) const {
            return a.labels() < b.labels();
        }
    };
    std::map<LabelSet, Series, LabelSetComparator> outputSeriesMap;

    // 3. Iterate via time steps
    // To avoid O(N*M) lookups, we use iterators for each input series.
    std::vector<std::vector<tsdb::prometheus::Sample>::const_iterator> iterators;
    iterators.reserve(inputMatrix.size());
    for (const auto& s : inputMatrix) {
        iterators.push_back(s.samples.begin());
    }

    for (int64_t t = start_; t <= end_; t += step_) {
        timestamp_ = t; // Important for param evaluation in AggregateVector
        
        // Collect samples for this timestamp
        Vector inputVector;

        inputVector.reserve(inputMatrix.size());

        for (size_t i = 0; i < inputMatrix.size(); ++i) {
            auto& it = iterators[i];
            const auto& series = inputMatrix[i];
            
            // Advance iterator to >= t
            while (it != series.samples.end() && it->timestamp() < t) {
                it++;
            }
            
            // Check if match
            if (it != series.samples.end() && it->timestamp() == t) {
                // Found sample
                Sample s;
                s.metric = series.metric;
                s.value = it->value();
                s.timestamp = t;
                inputVector.push_back(s);
            }
        }
        
        // 4. Perform Aggregation
        if (inputVector.empty()) {
             // Some aggregations produce result even for empty input? (e.g. count? no, strictly no vector -> no result, except vector(0)?)
             // PromQL: aggregation over empty vector is empty vector.
             // Exception: vector(time())?
             continue;
        }

        Value aggregated = AggregateVector(inputVector, node);
        if (aggregated.isVector()) {
             const Vector& resVec = aggregated.getVector();
             for (const auto& s : resVec) {
                  auto& series = outputSeriesMap[s.metric];
                  if (series.metric.labels().empty()) {
                      series.metric = s.metric;
                  }
                  series.samples.emplace_back(t, s.value);
             }
        }
    }
    
    // Convert to Matrix
    Matrix resultMatrix;
    resultMatrix.reserve(outputSeriesMap.size());
    for (auto& pair : outputSeriesMap) {
        resultMatrix.push_back(std::move(pair.second));
    }
    return Value(resultMatrix);
}
Value Evaluator::EvaluateRangeVectorSelector(const VectorSelectorNode* node) {
    // 1. Calculate full fetch range
    // We need [start - lookback, end]
    // Each step t needs [t - lookback, t]
    // So union is [start - lookback, end]
    
    int64_t fetch_start = start_ - lookback_delta_;
    int64_t fetch_end = end_;
    
    if (node->offset() > 0) {
        fetch_start -= node->offset();
        fetch_end -= node->offset();
    }
    
    // 2. Prepare matchers
    std::vector<model::LabelMatcher> matchers = node->matchers();
    if (!node->name.empty()) {
        bool found = false;
        for (const auto& m : matchers) {
            if (m.name == "__name__") {
                found = true;
                break;
            }
        }
        if (!found) {
            matchers.emplace_back(model::MatcherType::EQUAL, "__name__", node->name);
        }
    }

    // 3. Bulk Fetch
    // SelectSeries returns a Matrix (list of Series) containing all raw samples in the range.
    Matrix rawData = storage_->SelectSeries(matchers, fetch_start, fetch_end);
    
    // 4. Resample / Align to Steps
    // For each series, we produce a result Series with samples at step times.
    Matrix resultMatrix;
    resultMatrix.reserve(rawData.size());

    int64_t offset = node->offset() * 1000; // convert to ms if needed? offset() returns seconds usually? 
    // In EvaluateVectorSelector loop: `end -= vector_selector->offset()`
    // Usually offset is seconds. Wait, in EvaluateMatrixSelector, `parsedRangeSeconds * 1000`.
    // Let's assume offset() is whatever unit storage expects. 
    // In `EvaluateVectorSelector` logic:
    // int64_t end = timestamp_; 
    // if (vector_selector->offset() > 0) end -= vector_selector->offset();
    // In Engine::ExecuteRange: `fetch_start -= ctx.node->offset()`. 
    // `offset()` seems to be milliseconds in Engine logic?
    // Let's check `VectorSelectorNode` definition in `ast.h` if possible, but assuming consistent usage.
    // Engine `CollectSelectors` uses `offset()`.
    
    for (const auto& rawSeries : rawData) {
        Series resSeries;
        resSeries.metric = rawSeries.metric;
        resSeries.samples.reserve((end_ - start_) / step_ + 1);
        
        // For each step t, find the latest sample in [t - lookback - offset, t - offset]
        // Optimization: Walk through raw samples as we advance t
        
        auto it = rawSeries.samples.begin();
        
        for (int64_t t = start_; t <= end_; t += step_) {
            int64_t ref_t = t - node->offset();
            int64_t window_start = ref_t - lookback_delta_;
            int64_t window_end = ref_t; // inclusive?
            // EvaluateVectorSelector uses: `it_samp->timestamp() > end` break. So end is inclusive boundary usually?
            // Instant query: latest sample <= t.
            // AND sample must be > t - lookback. (Staleness)
            
            // Advance iterator to window_start (we need samples > window_start)
            // But actually we want the LAST sample <= window_end.
            // So we can scan forward until sample > window_end. The one before that is the candidate.
            
            // 1. Advance to potentially relevant samples
            // We can check samples starting from current `it`.
            
            // Find candidate: last sample where timestamp <= window_end
            const tsdb::prometheus::Sample* candidate = nullptr;
            
            // Optimization: Since t increases, the window moves forward.
            // We don't strictly need to discard samples < window_start immediately if we just keep track of candidate.
            // But we must check the candidate's timestamp >= window_start check later.
            
            while (it != rawSeries.samples.end() && it->timestamp() <= window_end) {
                candidate = &(*it);
                ++it;
            }
            
            // Now candidate is the last sample <= window_end (if any)
            // But wait, `it` might be far ahead if we had a large gap?
            // If we increment `it`, we might skip it for the next step?
            // No, because next step `window_end` is larger. `it` is already > previous `window_end`.
            // So `it` is the first sample > previous `window_end`.
            // The new `window_end` is larger, so we continue scanning from `it`.
            // The `candidate` from previous step is NOT necessarily the candidate for this step 
            // IF we strictly advanced `it`. 
            // Wait, if `it` advanced past `window_end`, `candidate` was set.
            // For next step, `window_end` increases. We start from `it`.
            // If `it` > new `window_end`, then no NEW samples entered the window.
            // But the OLD candidate might still be valid (if it's still > new `window_start`).
            // So we need to keep track of the *potential* candidate.
            
            // Actually, `it` should point to the first sample > current `window_end`.
            // So for next step, we continue from `it` until > next `window_end`.
            // The candidate is the element before `it`.
            
            if (it != rawSeries.samples.begin()) {
                // There is at least one sample <= window_end seen so far.
                // The candidate is strictly `it - 1`.
                auto temp_candidate = std::prev(it);
                
                // Check staleness: must be > window_start
                if (temp_candidate->timestamp() > window_start) {
                    // Valid match
                    // Wait, Prometheus has "staleness" check. Default implementation is lookback.
                    // If sample is within lookback, it's valid.
                    // (Note: there's also specific Staleness marker handling, but basic lookback is primary here).
                    resSeries.samples.emplace_back(t, temp_candidate->value());
                }
            }
        }
        
        if (!resSeries.samples.empty()) {
            resultMatrix.push_back(std::move(resSeries));
        }
    }
    
    return Value(resultMatrix);
}

Value Evaluator::EvaluateNumberLiteral(const NumberLiteralNode* node) {
    return Value(Scalar{timestamp_, node->value});
}

Value Evaluator::EvaluateStringLiteral(const StringLiteralNode* node) {
    return Value(String{timestamp_, node->value});
}

Value Evaluator::EvaluateParen(const ParenExprNode* node) {
    return Evaluate(node->expr.get());
}

Value Evaluator::EvaluateAggregate(const AggregateExprNode* node) {
    // Check if pushdown is possible
    bool pushdown_enabled = true;
    if (pushdown_enabled) {
        if (auto vector_selector = dynamic_cast<const VectorSelectorNode*>(node->expr.get())) {
            // Check supported ops
            tsdb::core::AggregationOp core_op;
            bool supported = false;
            
            switch (node->op()) {
                case TokenType::SUM: core_op = tsdb::core::AggregationOp::SUM; supported = true; break;
                case TokenType::MIN: core_op = tsdb::core::AggregationOp::MIN; supported = true; break;
                case TokenType::MAX: core_op = tsdb::core::AggregationOp::MAX; supported = true; break;
                case TokenType::COUNT: core_op = tsdb::core::AggregationOp::COUNT; supported = true; break;
                case TokenType::AVG: core_op = tsdb::core::AggregationOp::AVG; supported = true; break;
                case TokenType::STDDEV: core_op = tsdb::core::AggregationOp::STDDEV; supported = true; break;
                case TokenType::STDVAR: core_op = tsdb::core::AggregationOp::STDVAR; supported = true; break;
                case TokenType::QUANTILE: core_op = tsdb::core::AggregationOp::QUANTILE; supported = true; break;
                default: supported = false; break;
            }
            
            if (supported) {
                // Prepare aggregation request
                tsdb::core::AggregationRequest req;
                req.op = core_op;
                req.without = node->without;
                req.grouping_keys = node->grouping_labels();
                
                // Handle parameter for QUANTILE, TOPK, BOTTOMK
                if (node->param) {
                    Value param_val = Evaluate(node->param.get());
                    if (param_val.type == ValueType::SCALAR) {
                        req.param = param_val.getScalar().value;
                    } else {
                        // Parameter must be scalar
                        supported = false;
                    }
                }
                
                if (supported) {
                    // Execute pushdown query
                    try {
                        // Calculate time range same as EvaluateVectorSelector
                        int64_t end = timestamp_;
                        int64_t start = timestamp_ - lookback_delta_;
                        
                        if (vector_selector->offset() > 0) {
                            end -= vector_selector->offset();
                            start -= vector_selector->offset();
                        }

                        // Prepare matchers (include __name__ if present)
                        std::vector<model::LabelMatcher> matchers = vector_selector->matchers();
                        if (!vector_selector->name.empty()) {
                            bool found = false;
                            for (const auto& m : matchers) {
                                if (m.name == "__name__") {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                matchers.emplace_back(model::MatcherType::EQUAL, "__name__", vector_selector->name);
                            }
                        }

                        auto matrix = storage_->SelectAggregateSeries(
                            matchers,
                            start,
                            end,
                            req
                        );
                        
                        // Convert Matrix to Vector
                        Vector result_vector;
                        for (const auto& series : matrix) {
                            // Each series should have exactly one sample for the timestamp
                            if (!series.samples.empty()) {
                                Sample s;
                                s.metric = series.metric;
                                s.value = series.samples.back().value();
                                s.timestamp = series.samples.back().timestamp();
                                result_vector.push_back(s);
                            }
                        }
                        return Value(result_vector);
                    } catch (const std::exception& e) {
                        // Fallback to normal execution if pushdown fails
                    }
                }
            }
        }
    }

    // Fallback: Default execution
    
    // 1. Evaluate the inner expression
    Value inner_value = Evaluate(node->expr.get());
    
    if (!inner_value.isVector()) {
        throw std::runtime_error("Aggregation requires a vector argument");
    }
    
    const Vector& input_vector = inner_value.getVector();
    
    // 2. Delegate to helper
    Value result = AggregateVector(input_vector, node);
    
    return result;
}

Value Evaluator::AggregateVector(const Vector& input_vector, const AggregateExprNode* node) {
    // 2. Group samples
    // Map from result labels (signature) to list of values
    struct Group {
        LabelSet labels;
        std::vector<Sample> samples;
    };
    std::map<std::string, Group> groups; // Key is string representation of labels for uniqueness
    
    for (const auto& sample : input_vector) {
        LabelSet result_labels;
        
        if (node->without) {
            // Copy all labels EXCEPT those in grouping list
            for (const auto& [name, value] : sample.metric.labels()) {
                bool exclude = false;
                for (const auto& label : node->grouping_labels()) {
                    if (name == label) {
                        exclude = true;
                        break;
                    }
                }
                if (!exclude && name != "__name__") { // Also drop metric name
                    result_labels.AddLabel(name, value);
                }
            }
        } else {
            // BY clause: Copy ONLY labels in grouping list
            for (const auto& label : node->grouping_labels()) {
                auto value_opt = sample.metric.GetLabelValue(label);
                if (value_opt) {
                    result_labels.AddLabel(label, *value_opt);
                } else {
                    // Important: If a grouping label is missing, it should be treated as empty string for grouping?
                    // Prometheus behavior: "Labels that are not present in the grouping clause are dropped from the result."
                    // But for the *key*, if we group by 'job', and 'job' is missing, it's effectively empty.
                    // However, usually we only group by labels that exist.
                    // If we don't add it to result_labels, it won't be in the key.
                    // This seems correct.
                }
            }
        }
        
        std::string key = result_labels.ToString();
        if (groups.find(key) == groups.end()) {
            groups[key] = Group{result_labels, {}};
        }
        groups[key].samples.push_back(sample);
    }
    
    // 3. Aggregate each group
    Vector result_vector;
    
    // Evaluate parameter if present (for topk, bottomk, quantile, count_values)
    double param_value = 0;
    std::string param_string;

    if (node->param) {
        // Evaluate parameter in current context (timestamp_)
        Value param_result = Evaluate(node->param.get());
        if (node->op() == TokenType::COUNT_VALUES) {
             if (!param_result.isString()) {
                 throw std::runtime_error("count_values parameter must be a string");
             }
             param_string = param_result.getString().value;
        } else {
            if (param_result.isScalar()) {
                param_value = param_result.getScalar().value;
            } else {
                throw std::runtime_error("Aggregation parameter must be a scalar");
            }
        }
    }

    for (const auto& [key, group] : groups) {
        double result_value = 0;
        
        if (node->op() == TokenType::SUM) {
            for (const auto& s : group.samples) result_value += s.value;
        } else if (node->op() == TokenType::AVG) {
            double sum = 0;
            for (const auto& s : group.samples) sum += s.value;
            result_value = sum / group.samples.size();
        } else if (node->op() == TokenType::MIN) {
            result_value = std::numeric_limits<double>::infinity();
            for (const auto& s : group.samples) result_value = std::min(result_value, s.value);
        } else if (node->op() == TokenType::MAX) {
            result_value = -std::numeric_limits<double>::infinity();
            for (const auto& s : group.samples) result_value = std::max(result_value, s.value);
        } else if (node->op() == TokenType::COUNT) {
            result_value = static_cast<double>(group.samples.size());
        } else if (node->op() == TokenType::STDDEV || node->op() == TokenType::STDVAR) {
            // Calculate standard deviation / variance
            double sum = 0;
            for (const auto& s : group.samples) sum += s.value;
            double mean = sum / group.samples.size();
            
            double variance = 0;
            for (const auto& s : group.samples) {
                double diff = s.value - mean;
                variance += diff * diff;
            }
            variance /= group.samples.size();
            
            result_value = (node->op() == TokenType::STDDEV) ? std::sqrt(variance) : variance;
        } else if (node->op() == TokenType::TOPK || node->op() == TokenType::BOTTOMK) {
            int k = static_cast<int>(param_value);
            if (k < 1) continue;
            
            std::vector<Sample> sorted = group.samples;
            if (node->op() == TokenType::TOPK) {
                std::sort(sorted.begin(), sorted.end(), [](const Sample& a, const Sample& b) {
                    return a.value > b.value;
                });
            } else {
                std::sort(sorted.begin(), sorted.end(), [](const Sample& a, const Sample& b) {
                    return a.value < b.value;
                });
            }
            
            for (int i = 0; i < k && i < sorted.size(); ++i) {
                result_vector.push_back(sorted[i]);
            }
            continue; // Skip default push_back
        } else if (node->op() == TokenType::COUNT_VALUES) {
            std::map<double, int> value_counts;
            for (const auto& s : group.samples) {
                value_counts[s.value]++;
            }

            for (const auto& [val, count] : value_counts) {
                LabelSet new_labels = group.labels;
                // Simple string conversion. For production, use better formatting (e.g. remove trailing zeros)
                std::string val_str = std::to_string(val);
                if (val == static_cast<int64_t>(val)) {
                    val_str = std::to_string(static_cast<int64_t>(val));
                }
                
                new_labels.AddLabel(param_string, val_str);
                result_vector.push_back(Sample{new_labels, timestamp_, static_cast<double>(count)});
            }
            continue;
        } else if (node->op() == TokenType::QUANTILE) {
            double phi = param_value;
            if (group.samples.empty()) {
                result_value = std::numeric_limits<double>::quiet_NaN();
            } else {
                std::vector<double> values;
                values.reserve(group.samples.size());
                for (const auto& s : group.samples) values.push_back(s.value);
                std::sort(values.begin(), values.end());
                
                double rank = phi * (values.size() - 1);
                size_t lower = static_cast<size_t>(rank);
                size_t upper = lower + 1;
                double weight = rank - lower;
                
                if (upper >= values.size()) {
                    result_value = values[lower];
                } else {
                    result_value = values[lower] * (1.0 - weight) + values[upper] * weight;
                }
            }
        } else {
            throw std::runtime_error("Unsupported aggregation operator");
        }
        
        result_vector.push_back(Sample{group.labels, timestamp_, result_value});
    }
    
    return Value(result_vector);
}

// Helper to apply binary operation
double ApplyOp(TokenType op, double lval, double rval) {
    switch (op) {
        case TokenType::ADD: return lval + rval;
        case TokenType::SUB: return lval - rval;
        case TokenType::MUL: return lval * rval;
        case TokenType::DIV: 
            if (rval == 0) return std::numeric_limits<double>::infinity(); // Prometheus uses Inf for div by zero
            return lval / rval;
        case TokenType::MOD: 
            if (rval == 0) return std::numeric_limits<double>::quiet_NaN();
            return std::fmod(lval, rval);
        case TokenType::POW: return std::pow(lval, rval);
        case TokenType::EQL: return (lval == rval) ? 1.0 : 0.0;
        case TokenType::NEQ: return (lval != rval) ? 1.0 : 0.0;
        case TokenType::GTR: return (lval > rval) ? 1.0 : 0.0;
        case TokenType::LSS: return (lval < rval) ? 1.0 : 0.0;
        case TokenType::GTE: return (lval >= rval) ? 1.0 : 0.0;
        case TokenType::LTE: return (lval <= rval) ? 1.0 : 0.0;
        default: throw std::runtime_error("Unsupported binary operator");
    }
}

// Helper to generate signature for vector matching
std::string GenerateSignature(const LabelSet& labels, const std::vector<std::string>& matchingLabels, bool on) {
    std::string signature;
    // TODO: This is a naive implementation. Should be optimized.
    // Also need to handle __name__ exclusion if standard PromQL behavior is desired (usually yes).
    
    if (on) {
        // Only include labels in matchingLabels
        for (const auto& name : matchingLabels) {
            auto val = labels.GetLabelValue(name);
            if (val) {
                signature += name + "=" + *val + ",";
            } else {
                signature += name + "=,"; // Empty value for missing label
            }
        }
    } else {
        // Include all labels EXCEPT matchingLabels (ignoring)
        // And usually exclude __name__
        for (const auto& pair : labels.labels()) {
            if (pair.first == "__name__") continue;
            
            bool ignored = false;
            for (const auto& ignore : matchingLabels) {
                if (pair.first == ignore) {
                    ignored = true;
                    break;
                }
            }
            if (!ignored) {
                signature += pair.first + "=" + pair.second + ",";
            }
        }
    }
    return signature;
}

Value Evaluator::EvaluateBinary(const BinaryExprNode* node) {

    Value lhs = Evaluate(node->lhs.get());
    Value rhs = Evaluate(node->rhs.get());

    // Logical Operators (Set Operations) - Vector-Vector only
    if (node->op == TokenType::AND || node->op == TokenType::OR || node->op == TokenType::UNLESS) {
        if (!lhs.isVector() || !rhs.isVector()) {
            throw std::runtime_error("Logical operators must be between vectors");
        }
        
        Vector result_vector;
        const auto& lvec = lhs.getVector();
        const auto& rvec = rhs.getVector();
        
        std::cout << "DEBUG: Logical op " << static_cast<int>(node->op) << " LHS=" << lvec.size() << " RHS=" << rvec.size() << std::endl;

        // Build map for RHS signatures
        std::map<std::string, const Sample*> rhs_map;
        for (const auto& sample : rvec) {
            std::string sig = GenerateSignature(sample.metric, node->matchingLabels, node->on);
            rhs_map[sig] = &sample;
        }
        
        if (node->op == TokenType::AND) {
            // Intersection: Keep LHS if signature exists in RHS
            for (const auto& lsample : lvec) {
                std::string sig = GenerateSignature(lsample.metric, node->matchingLabels, node->on);
                if (rhs_map.find(sig) != rhs_map.end()) {
                    result_vector.push_back(lsample);
                }
            }
        } else if (node->op == TokenType::UNLESS) {
            // Difference: Keep LHS if signature does NOT exist in RHS
            for (const auto& lsample : lvec) {
                std::string sig = GenerateSignature(lsample.metric, node->matchingLabels, node->on);
                if (rhs_map.find(sig) == rhs_map.end()) {
                    result_vector.push_back(lsample);
                }
            }
        } else if (node->op == TokenType::OR) {
            // Union: Keep all LHS, then add RHS if signature not in LHS
            // Note: Prometheus OR matching is slightly different:
            // It includes all LHS elements.
            // Then includes RHS elements that do not match any LHS element.
            
            // First add all LHS
            std::map<std::string, bool> lhs_sigs;
            for (const auto& lsample : lvec) {
                result_vector.push_back(lsample);
                std::string sig = GenerateSignature(lsample.metric, node->matchingLabels, node->on);
                lhs_sigs[sig] = true;
            }
            
            // Then add RHS if not in LHS
            for (const auto& rsample : rvec) {
                std::string sig = GenerateSignature(rsample.metric, node->matchingLabels, node->on);
                if (lhs_sigs.find(sig) == lhs_sigs.end()) {
                    result_vector.push_back(rsample);
                }
            }
        }
        
        return Value(result_vector);
    }

    // Scalar-Scalar
    if (lhs.isScalar() && rhs.isScalar()) {
        double result = ApplyOp(node->op, lhs.getScalar().value, rhs.getScalar().value);
        return Value(Scalar{timestamp_, result});
    }

    // Vector-Scalar
    if (lhs.isVector() && rhs.isScalar()) {
        Vector result_vector;
        double rval = rhs.getScalar().value;
        for (const auto& sample : lhs.getVector()) {
            double res = ApplyOp(node->op, sample.value, rval);
            // For comparison, we filter unless bool modifier is present
            if (TokenType::EQL <= node->op && node->op <= TokenType::GTR) {
                if (node->returnBool) {
                    Sample s = sample;
                    s.value = (res != 0) ? 1.0 : 0.0;
                    s.metric.RemoveLabel("__name__");
                    result_vector.push_back(s);
                } else {
                    if (res != 0) {
                        result_vector.push_back(Sample{sample.metric, timestamp_, sample.value}); // Keep original value
                    }
                }
            } else {
                // Arithmetic
                Sample s = sample;
                s.value = res;
                s.metric.RemoveLabel("__name__"); // Drop metric name
                result_vector.push_back(s);
            }
        }
        return Value(result_vector);
    }

    // Scalar-Vector
    if (lhs.isScalar() && rhs.isVector()) {
        Vector result_vector;
        double lval = lhs.getScalar().value;
        for (const auto& sample : rhs.getVector()) {
            double res = ApplyOp(node->op, lval, sample.value);
            if (TokenType::EQL <= node->op && node->op <= TokenType::GTR) {
                if (node->returnBool) {
                    Sample s = sample;
                    s.value = (res != 0) ? 1.0 : 0.0;
                    s.metric.RemoveLabel("__name__");
                    result_vector.push_back(s);
                } else {
                    if (res != 0) {
                         result_vector.push_back(Sample{sample.metric, timestamp_, sample.value});
                    }
                }
            } else {
                Sample s = sample;
                s.value = res;
                s.metric.RemoveLabel("__name__");
                result_vector.push_back(s);
            }
        }
        return Value(result_vector);
    }

    // Vector-Vector
    if (lhs.isVector() && rhs.isVector()) {
        Vector result_vector;
        const auto& lvec = lhs.getVector();
        const auto& rvec = rhs.getVector();

        // Build map for RHS
        // Use multimap to support One-to-Many (group_right) where RHS has multiple matches
        std::multimap<std::string, const Sample*> rhs_map;
        for (const auto& sample : rvec) {
            std::string sig = GenerateSignature(sample.metric, node->matchingLabels, node->on);
            rhs_map.insert({sig, &sample});
        }

        for (const auto& lsample : lvec) {
            std::string sig = GenerateSignature(lsample.metric, node->matchingLabels, node->on);
            auto range = rhs_map.equal_range(sig);
            
            // If no match, skip
            if (range.first == range.second) continue;
            
            // Check cardinality
            size_t match_count = std::distance(range.first, range.second);
            
            if (node->groupSide == "left") {
                // Many-to-One: LHS is Many, RHS must be One
                if (match_count > 1) {
                    throw std::runtime_error("multiple matches for labels: many-to-one matching must be unique on right side");
                }
            } else if (node->groupSide == "right") {
                // One-to-Many: LHS is One, RHS is Many
                // LHS must be unique? (We iterate LHS, so if LHS has duplicates, we just process them. 
                // But usually One-to-Many implies LHS is the "One" side).
                // If LHS has multiple samples with same signature, it effectively becomes Many-to-Many which is invalid unless explicitly allowed?
                // Prometheus allows Many-to-Many only with logical operators.
                // For arithmetic/comparison, one side MUST be "One".
                // So if group_right, LHS must be unique.
                // But we are iterating LHS. We can't easily check uniqueness of LHS globally here without another pass.
                // However, if we assume valid input (or just standard behavior), we process each LHS.
            } else {
                // One-to-One (default)
                if (match_count > 1) {
                    throw std::runtime_error("multiple matches for labels: one-to-one matching must be unique on both sides");
                }
            }

            // Process matches
            for (auto it = range.first; it != range.second; ++it) {
                const Sample* rsample = it->second;
                double res = ApplyOp(node->op, lsample.value, rsample->value);
                
                if (TokenType::EQL <= node->op && node->op <= TokenType::GTR) {
                    if (node->returnBool) {
                        Sample s = lsample;
                        s.value = (res != 0) ? 1.0 : 0.0;
                        s.metric.RemoveLabel("__name__");
                        // Handle included labels? (Usually bool modifier doesn't include labels? Or does it?)
                        // Assuming standard behavior.
                        result_vector.push_back(s);
                    } else {
                        if (res != 0) {
                            // Result is LHS sample, potentially with included labels from RHS
                            Sample s = lsample;
                            if (!node->includeLabels.empty()) {
                                for (const auto& label : node->includeLabels) {
                                    auto val = rsample->metric.GetLabelValue(label);
                                    if (val) {
                                        s.metric.AddLabel(label, *val);
                                    }
                                }
                            }
                            result_vector.push_back(s);
                        }
                    }
                } else {
                    Sample s = lsample; // Result has LHS labels
                    s.value = res;
                    s.metric.RemoveLabel("__name__");
                    
                    // Handle included labels
                    if (!node->includeLabels.empty()) {
                        for (const auto& label : node->includeLabels) {
                            auto val = rsample->metric.GetLabelValue(label);
                            if (val) {
                                s.metric.AddLabel(label, *val);
                            }
                        }
                    }
                    
                    result_vector.push_back(s);
                }
            }
        }
        return Value(result_vector);
    }

    throw std::runtime_error("Vector binary operations not implemented yet");
}

Value Evaluator::EvaluateCall(const CallNode* node) {
    auto start = std::chrono::high_resolution_clock::now();

    // 1. Evaluate arguments
    std::vector<Value> args;
    for (const auto& arg_expr : node->arguments()) {
        args.push_back(Evaluate(arg_expr.get()));
    }

    // 2. Look up function
    auto* signature = FunctionRegistry::Instance().Get(node->func_name());
    if (!signature) {
        throw std::runtime_error("Unknown function: " + node->func_name());
    }

    // 3. Call function
    Value result = signature->implementation(args, this);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    
    return result;
}

Value Evaluator::EvaluateMatrixSelector(const MatrixSelectorNode* node) {
    if (!storage_) {
        throw std::runtime_error("No storage adapter configured");
    }

    // Matrix selector selects a range of samples.
    // Range is [timestamp - range - offset, timestamp - offset]
    
    int64_t range = node->range_duration().count();
    int64_t end = timestamp_;
    
    // Handle offset
    // Note: MatrixSelectorNode doesn't seem to have offset() method directly on it?
    // Usually MatrixSelector wraps a VectorSelector which has the offset?
    // Let's check AST. MatrixSelectorNode has `vector_selector()` which is a VectorSelectorNode.
    // The range is applied to the vector selector.
    
    // Wait, AST definition:
    // class MatrixSelectorNode : public ExprNode { ... const ExprNode* vector_selector() ... }
    // Actually vector_selector is usually a VectorSelectorNode.
    
    const auto* vs = dynamic_cast<const VectorSelectorNode*>(node->vector_selector());
    if (!vs) {
        throw std::runtime_error("Matrix selector must wrap a vector selector");
    }
    
    if (vs->offset() > 0) {
        end -= vs->offset();
    }
    
    int64_t start = end - range;
    
    // Prepare matchers
    std::vector<model::LabelMatcher> matchers = vs->matchers();
    if (!vs->name.empty()) {
        bool found = false;
        for (const auto& m : matchers) {
            if (m.name == "__name__") {
                found = true;
                break;
            }
        }
        if (!found) {
            matchers.emplace_back(model::MatcherType::EQUAL, "__name__", vs->name);
        }
    }
    
    // Fetch series
    Matrix matrix = storage_->SelectSeries(matchers, start, end);
    
    return Value(matrix);
}

Value Evaluator::EvaluateSubquery(const SubqueryExprNode* node) {
    if (!storage_) {
        throw std::runtime_error("No storage adapter configured");
    }

    int64_t range = node->parsedRangeSeconds * 1000; // ms
    int64_t resolution = node->parsedResolutionSeconds * 1000; // ms
    
    // Default resolution if 0?
    if (resolution <= 0) {
        // Use a default or global step. For now hardcode 1m (60000ms) or 1s?
        // Prometheus uses query evaluation interval (step).
        // Since we don't have global step passed down easily here (it's not in Evaluator?), 
        // we might need to assume 1m or 1s.
        // Let's use 1000ms (1s) as safe default or 60s.
        resolution = 60000; 
    }
    
    int64_t end = timestamp_;
    if (node->parsedOffsetSeconds > 0) {
        end -= (node->parsedOffsetSeconds * 1000);
    }
    
    int64_t start = end - range;
    
    // Align start to resolution?
    // Prometheus aligns: start = start - (start % resolution)
    // But let's keep it simple for now.
    
    Matrix result_matrix;
    // Map signature -> Series index in result_matrix
    std::map<std::string, size_t> series_map;
    
    for (int64_t t = start; t <= end; t += resolution) {
        // Create sub-evaluator
        Evaluator sub_eval(t, lookback_delta_, storage_);
        Value v = sub_eval.Evaluate(node->expr.get());
        
        if (v.isVector()) {
            for (const auto& sample : v.getVector()) {
                std::string sig = sample.metric.ToString(); 
                
                if (series_map.find(sig) == series_map.end()) {
                    Series series;
                    series.metric = sample.metric;
                    result_matrix.push_back(series);
                    series_map[sig] = result_matrix.size() - 1;
                }
                
                size_t idx = series_map[sig];
                result_matrix[idx].samples.emplace_back(t, sample.value);
            }
        } else if (v.isScalar()) {
            // Scalar result -> Series with no labels
            std::string sig = "{}";
            if (series_map.find(sig) == series_map.end()) {
                Series series;
                // Empty labels
                result_matrix.push_back(series);
                series_map[sig] = result_matrix.size() - 1;
            }
            size_t idx = series_map[sig];
            result_matrix[idx].samples.emplace_back(t, v.getScalar().value);
        }
    }
    
    return Value(result_matrix);
}

Value Evaluator::EvaluateUnary(const UnaryExprNode* node) {
    (void)node;
    throw std::runtime_error("Unary expressions not implemented yet");
}

// Helper for rate calculation (adapted from rate.cpp)
// Helper for rate calculation (adapted from rate.cpp)
static double CalculateRateHelper(const std::vector<const tsdb::prometheus::Sample*>& samples, bool is_counter, bool is_rate) {
    if (samples.size() < 2) {
        return 0.0;
    }

    double result_value = 0.0;
    
    // Duration from first to last sample
    double duration = (samples.back()->timestamp() - samples.front()->timestamp()) / 1000.0;
    
    if (duration == 0) return 0.0;

    if (!is_counter) {
        result_value = samples.back()->value() - samples.front()->value();
    } else {
        double value = 0.0;
        for (size_t i = 1; i < samples.size(); ++i) {
            double prev = samples[i-1]->value();
            double curr = samples[i]->value();
            
            if (curr < prev) {
                 value += prev; 
                 value += curr;
            } else {
                value += (curr - prev);
            }
        }
        result_value = value;
    }

    if (is_rate) {
        return result_value / duration;
    } else {
        return result_value;
    }
}

Value Evaluator::EvaluateRangeCall(const CallNode* node) {
    if (node->func_name() != "rate" && node->func_name() != "increase" && node->func_name() != "irate") {
        return EvaluateRangeDefault(node);
    }
    
    if (node->arguments().empty() || node->arguments()[0]->type() != ExprNode::Type::MATRIX_SELECTOR) {
        return EvaluateRangeDefault(node);
    }
    
    const MatrixSelectorNode* matrixNode = static_cast<const MatrixSelectorNode*>(node->arguments()[0].get());
    int64_t range = matrixNode->range_duration().count();
    
    // 1. Calculate fetch range
    int64_t fetchStart = start_ - range;
    int64_t fetchEnd = end_;
    
    if (matrixNode->vectorSelector->offset() > 0) {
        fetchStart -= matrixNode->vectorSelector->offset();
        fetchEnd -= matrixNode->vectorSelector->offset();
    }
    
    // 2. Fetch all series
    auto rawSeries = storage_->SelectSeries(matrixNode->vectorSelector->matchers(), fetchStart, fetchEnd);
    
    // 3. Prepare result
    Matrix resultMatrix;
    resultMatrix.reserve(rawSeries.size());
    
    bool is_counter = true; // rate/increase usually on counters
    bool is_rate = (node->func_name() == "rate" || node->func_name() == "irate");
    
    for (const auto& series : rawSeries) {
        Series resSeries;
        resSeries.metric = series.metric;
        resSeries.metric.RemoveLabel("__name__");
        resSeries.samples.reserve((end_ - start_) / step_ + 1);
        
        auto it = series.samples.begin();
        
        for (int64_t t = start_; t <= end_; t += step_) {
            int64_t evalT = t;
            if (matrixNode->vectorSelector->offset() > 0) evalT -= matrixNode->vectorSelector->offset();
            
            int64_t windowStart = evalT - range;
            int64_t windowEnd = evalT;
            
            while (it != series.samples.end() && it->timestamp() < windowStart) {
                it++;
            }
            
            std::vector<const tsdb::prometheus::Sample*> windowSamples;
            auto windowIt = it;
            while (windowIt != series.samples.end() && windowIt->timestamp() <= windowEnd) {
                windowSamples.push_back(&(*windowIt));
                windowIt++;
            }
            
            double value = 0;
            if (node->func_name() == "irate") {
                 if (windowSamples.size() >= 2) {
                     const auto* last = windowSamples.back();
                     const auto* prev = windowSamples[windowSamples.size()-2];
                     double dur = (last->timestamp() - prev->timestamp()) / 1000.0;
                     if (dur > 0) {
                         double delta = last->value() - prev->value();
                         if (delta < 0) delta = last->value(); // Reset
                         value = delta / dur;
                     }
                 }
            } else {
                 value = CalculateRateHelper(windowSamples, is_counter, is_rate);
            }
            
            if (windowSamples.size() >= 2) {
                resSeries.samples.emplace_back(t, value);
            }
        }
        
        if (!resSeries.samples.empty()) {
            resultMatrix.push_back(std::move(resSeries));
        }
    }
    
    return Value(resultMatrix);
}

Value Evaluator::EvaluateVectorSelector(const VectorSelectorNode* node) {
    if (!storage_) {
        throw std::runtime_error("No storage adapter configured");
    }

    // Calculate time range
    // Instant vector selector selects the single latest sample before timestamp, 
    // looking back up to lookback_delta.
    // However, SelectSeries returns a Matrix (Range Vector).
    // We need to fetch range [timestamp - lookback_delta, timestamp]
    // and then for each series, pick the last sample.
    
    int64_t end = timestamp_;
    int64_t start = timestamp_ - lookback_delta_;
    
    // Handle offset
    if (node->offset() > 0) {
        end -= node->offset();
        start -= node->offset();
    }
    
    // Prepare matchers
    std::vector<model::LabelMatcher> matchers = node->matchers();
    if (!node->name.empty()) {
        // Check if __name__ matcher already exists (to avoid duplicates if parser added it)
        bool found = false;
        for (const auto& m : matchers) {
            if (m.name == "__name__") {
                found = true;
                break;
            }
        }
        if (!found) {
            matchers.emplace_back(model::MatcherType::EQUAL, "__name__", node->name);
        }
    }

    // Fetch series
    Matrix matrix = storage_->SelectSeries(matchers, start, end);
    
    // Convert to Vector (Instant Vector)
    Vector result_vector;
    
    for (const auto& series : matrix) {
        if (series.samples.empty()) continue;
        
        // Pick the last sample (closest to evaluation time)
        // Since samples are sorted by timestamp, it's the last one.
        const auto& last_sample = series.samples.back();
        
        // Check staleness? (Usually handled by lookback delta implicitly)
        // If sample is older than lookback_delta (relative to eval time), it shouldn't be here
        // because we queried with that range.
        
        // We need to construct a Sample with the metric labels and the value/timestamp
        // Note: The timestamp of the result sample is the timestamp of the source sample,
        // NOT the evaluation timestamp (unless it's a derived value like rate).
        // For instant vector selectors, we preserve the sample timestamp.
        
        result_vector.push_back(Sample{series.metric, last_sample.timestamp(), last_sample.value()});
    }
    
    return Value(result_vector);
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
