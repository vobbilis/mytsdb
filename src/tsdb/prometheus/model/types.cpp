#include "types.h"
#include <regex>
#include <sstream>

namespace tsdb {
namespace prometheus {

namespace {
    const std::regex kLabelNameRegex("[a-zA-Z_][a-zA-Z0-9_]*");
    const std::regex kMetricNameRegex("[a-zA-Z_:][a-zA-Z0-9_:]*");
    const int64_t kMinTimestamp = 0;  // Unix epoch
    const int64_t kMaxTimestamp = 253402300799999;  // Year 9999
}

// Sample implementation
Sample::Sample(int64_t timestamp, double value)
    : timestamp_(timestamp), value_(value) {}

bool Sample::operator==(const Sample& other) const {
    return timestamp_ == other.timestamp_ && value_ == other.value_;
}

bool Sample::operator!=(const Sample& other) const {
    return !(*this == other);
}

// LabelSet implementation
LabelSet::LabelSet(const LabelMap& labels) : labels_(labels) {
    for (const auto& [name, value] : labels) {
        ValidateLabel(name, value);
    }
}

void LabelSet::AddLabel(const std::string& name, const std::string& value) {
    ValidateLabel(name, value);
    labels_[name] = value;
}

void LabelSet::RemoveLabel(const std::string& name) {
    labels_.erase(name);
}

bool LabelSet::HasLabel(const std::string& name) const {
    return labels_.find(name) != labels_.end();
}

std::optional<std::string> LabelSet::GetLabelValue(const std::string& name) const {
    auto it = labels_.find(name);
    if (it != labels_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool LabelSet::operator==(const LabelSet& other) const {
    return labels_ == other.labels_;
}

bool LabelSet::operator!=(const LabelSet& other) const {
    return !(*this == other);
}

std::string LabelSet::ToString() const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [name, value] : labels_) {
        if (!first) {
            oss << ",";
        }
        oss << name << "=\"" << value << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

void LabelSet::ValidateLabel(const std::string& name, const std::string& value) const {
    if (name.empty()) {
        throw InvalidLabelError("Label name cannot be empty");
    }
    if (!std::regex_match(name, kLabelNameRegex)) {
        throw InvalidLabelError("Invalid label name: " + name);
    }
    if (value.empty()) {
        throw InvalidLabelError("Label value cannot be empty");
    }
}

// TimeSeries implementation
TimeSeries::TimeSeries(const LabelSet& labels) : labels_(labels) {}

void TimeSeries::AddSample(const Sample& sample) {
    ValidateTimestamp(sample.timestamp());
    samples_.push_back(sample);
}

void TimeSeries::AddSample(int64_t timestamp, double value) {
    AddSample(Sample(timestamp, value));
}

void TimeSeries::Clear() {
    samples_.clear();
}

bool TimeSeries::operator==(const TimeSeries& other) const {
    return labels_ == other.labels_ && samples_ == other.samples_;
}

bool TimeSeries::operator!=(const TimeSeries& other) const {
    return !(*this == other);
}

void TimeSeries::ValidateTimestamp(int64_t timestamp) const {
    if (timestamp < kMinTimestamp || timestamp > kMaxTimestamp) {
        throw InvalidTimestampError(
            "Timestamp out of range: " + std::to_string(timestamp));
    }
}

// MetricFamily implementation
MetricFamily::MetricFamily(const std::string& name, Type type, const std::string& help)
    : name_(name), type_(type), help_(help) {
    ValidateMetricName(name);
}

void MetricFamily::AddTimeSeries(const TimeSeries& series) {
    series_.push_back(series);
}

void MetricFamily::RemoveTimeSeries(const LabelSet& labels) {
    series_.erase(
        std::remove_if(series_.begin(), series_.end(),
            [&labels](const TimeSeries& series) {
                return series.labels() == labels;
            }),
        series_.end());
}

bool MetricFamily::operator==(const MetricFamily& other) const {
    return name_ == other.name_ &&
           type_ == other.type_ &&
           help_ == other.help_ &&
           series_ == other.series_;
}

bool MetricFamily::operator!=(const MetricFamily& other) const {
    return !(*this == other);
}

void MetricFamily::ValidateMetricName(const std::string& name) const {
    if (name.empty()) {
        throw InvalidMetricError("Metric name cannot be empty");
    }
    if (!std::regex_match(name, kMetricNameRegex)) {
        throw InvalidMetricError("Invalid metric name: " + name);
    }
}

} // namespace prometheus
} // namespace tsdb 