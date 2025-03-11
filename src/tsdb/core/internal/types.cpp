#include "tsdb/core/types.h"
#include "tsdb/core/error.h"
#include <sstream>
#include <algorithm>

namespace tsdb {
namespace core {

Labels::Labels(const Map& labels) : labels_(labels) {}

void Labels::add(const std::string& name, const std::string& value) {
    if (name.empty()) {
        throw InvalidArgumentError("Label name cannot be empty");
    }
    labels_[name] = value;
}

void Labels::remove(const std::string& name) {
    labels_.erase(name);
}

bool Labels::has(const std::string& name) const {
    return labels_.find(name) != labels_.end();
}

std::optional<std::string> Labels::get(const std::string& name) const {
    auto it = labels_.find(name);
    if (it != labels_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Labels::operator==(const Labels& other) const {
    return labels_ == other.labels_;
}

bool Labels::operator!=(const Labels& other) const {
    return !(*this == other);
}

bool Labels::operator<(const Labels& other) const {
    return labels_ < other.labels_;
}

std::string Labels::to_string() const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [name, value] : labels_) {
        if (!first) {
            oss << ", ";
        }
        oss << name << "=\"" << value << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

Sample::Sample(Timestamp ts, Value val)
    : timestamp_(ts), value_(val) {}

bool Sample::operator==(const Sample& other) const {
    return timestamp_ == other.timestamp_ && value_ == other.value_;
}

bool Sample::operator!=(const Sample& other) const {
    return !(*this == other);
}

TimeSeries::TimeSeries(const Labels& labels)
    : labels_(labels) {}

void TimeSeries::add_sample(const Sample& sample) {
    // Ensure samples are added in chronological order
    if (!samples_.empty() && samples_.back().timestamp() >= sample.timestamp()) {
        throw InvalidArgumentError("Samples must be added in chronological order");
    }
    samples_.push_back(sample);
}

void TimeSeries::add_sample(Timestamp ts, Value val) {
    add_sample(Sample(ts, val));
}

void TimeSeries::clear() {
    samples_.clear();
}

bool TimeSeries::operator==(const TimeSeries& other) const {
    return labels_ == other.labels_ && samples_ == other.samples_;
}

bool TimeSeries::operator!=(const TimeSeries& other) const {
    return !(*this == other);
}

} // namespace core
} // namespace tsdb 