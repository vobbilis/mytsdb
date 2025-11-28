#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <regex>
#include <sstream>

namespace tsdb {
namespace prometheus {
namespace promql {

void RegisterLabelManipulationFunctions(FunctionRegistry& registry) {
    // label_replace(v instant-vector, dst_label string, replacement string, src_label string, regex string)
    // For each timeseries in v, label_replace(v, dst_label, replacement, src_label, regex) 
    // matches the regular expression regex against the value of the label src_label. 
    // If it matches, the value of the label dst_label in the returned timeseries will be 
    // the expansion of replacement, together with the original labels in the input.
    registry.Register({
        "label_replace",
        {ValueType::VECTOR, ValueType::STRING, ValueType::STRING, ValueType::STRING, ValueType::STRING},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            const std::string& dst_label = args[1].getString().value;
            const std::string& replacement = args[2].getString().value;
            const std::string& src_label = args[3].getString().value;
            const std::string& regex_str = args[4].getString().value;
            
            Vector result;
            
            try {
                std::regex regex(regex_str);
                
                for (const auto& sample : vec) {
                    // Get source label value
                    auto src_value_opt = sample.metric.GetLabelValue(src_label);
                    std::string src_value = src_value_opt ? *src_value_opt : "";
                    
                    // Try to match regex
                    std::smatch match;
                    LabelSet new_labels = sample.metric;
                    
                    if (std::regex_match(src_value, match, regex)) {
                        // Perform replacement with capture groups
                        std::string new_value = replacement;
                        
                        // Replace $1, $2, etc. with capture groups
                        for (size_t i = 1; i < match.size(); i++) {
                            std::string placeholder = "$" + std::to_string(i);
                            size_t pos = 0;
                            while ((pos = new_value.find(placeholder, pos)) != std::string::npos) {
                                new_value.replace(pos, placeholder.length(), match[i].str());
                                pos += match[i].length();
                            }
                        }
                        
                        // Set the destination label
                        new_labels.AddLabel(dst_label, new_value);
                    }
                    
                    result.push_back(Sample{new_labels, sample.timestamp, sample.value});
                }
            } catch (const std::regex_error& e) {
                // If regex is invalid, return original vector unchanged
                return Value(vec);
            }
            
            return Value(result);
        }
    });
    
    // label_join(v instant-vector, dst_label string, separator string, src_label_1 string, src_label_2 string, ...)
    // For each timeseries in v, label_join(v, dst_label, separator, src_label_1, src_label_2, ...) 
    // joins all the values of all the src_labels using separator and returns the timeseries 
    // with the label dst_label containing the joined value.
    registry.Register({
        "label_join",
        {ValueType::VECTOR, ValueType::STRING, ValueType::STRING}, // Minimum args
        true, // Variadic - can have more src_labels
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            if (args.size() < 4) {
                // Need at least: vector, dst_label, separator, and one src_label
                throw std::runtime_error("label_join requires at least 4 arguments");
            }
            
            const auto& vec = args[0].getVector();
            const std::string& dst_label = args[1].getString().value;
            const std::string& separator = args[2].getString().value;
            
            // Collect all source label names (args[3] onwards)
            std::vector<std::string> src_labels;
            for (size_t i = 3; i < args.size(); i++) {
                src_labels.push_back(args[i].getString().value);
            }
            
            Vector result;
            
            for (const auto& sample : vec) {
                // Join all source label values
                std::ostringstream joined;
                bool first = true;
                
                for (const auto& src_label : src_labels) {
                    if (!first) {
                        joined << separator;
                    }
                    first = false;
                    
                    auto value_opt = sample.metric.GetLabelValue(src_label);
                    if (value_opt) {
                        joined << *value_opt;
                    }
                    // If label doesn't exist, contribute empty string
                }
                
                // Create new labels with dst_label set to joined value
                LabelSet new_labels = sample.metric;
                new_labels.AddLabel(dst_label, joined.str());
                
                result.push_back(Sample{new_labels, sample.timestamp, sample.value});
            }
            
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
