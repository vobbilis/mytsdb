#include "tsdb/prometheus/remote/converter.h"
#include "remote.pb.h"
#include <stdexcept>

namespace tsdb {
namespace prometheus {
namespace remote {

std::vector<core::TimeSeries> Converter::FromWriteRequest(
    const ::prometheus::WriteRequest& request) {
    std::vector<core::TimeSeries> result;
    result.reserve(request.timeseries_size());
    
    for (const auto& proto_ts : request.timeseries()) {
        core::Labels labels;
        for (const auto& proto_label : proto_ts.labels()) {
            labels.add(proto_label.name(), proto_label.value());
        }
        
        core::TimeSeries ts(labels);
        for (const auto& proto_sample : proto_ts.samples()) {
            ts.add_sample(proto_sample.timestamp(), proto_sample.value());
        }
        
        result.push_back(std::move(ts));
    }
    
    return result;
}

::prometheus::ReadResponse Converter::ToReadResponse(
    const std::vector<core::TimeSeries>& series) {
    ::prometheus::ReadResponse response;
    auto* query_result = response.add_results();
    
    for (const auto& ts : series) {
        auto* proto_ts = query_result->add_timeseries();
        
        // Convert labels
        for (const auto& [name, value] : ts.labels().map()) {
            auto* proto_label = proto_ts->add_labels();
            proto_label->set_name(name);
            proto_label->set_value(value);
        }
        
        // Convert samples
        for (const auto& sample : ts.samples()) {
            auto* proto_sample = proto_ts->add_samples();
            proto_sample->set_timestamp(sample.timestamp());
            proto_sample->set_value(sample.value());
        }
    }
    
    return response;
}

core::LabelMatcher Converter::FromProtoMatcher(
    const ::prometheus::LabelMatcher& matcher) {
    core::MatcherType type;
    switch (matcher.type()) {
        case ::prometheus::LabelMatcher::EQ:
            type = core::MatcherType::Equal;
            break;
        case ::prometheus::LabelMatcher::NEQ:
            type = core::MatcherType::NotEqual;
            break;
        case ::prometheus::LabelMatcher::RE:
            type = core::MatcherType::RegexMatch;
            break;
        case ::prometheus::LabelMatcher::NRE:
            type = core::MatcherType::RegexNoMatch;
            break;
        default:
            throw std::invalid_argument("Unknown matcher type");
    }
    
    return core::LabelMatcher(type, matcher.name(), matcher.value());
}

core::Sample Converter::FromProtoSample(
    const ::prometheus::Sample& sample) {
    return core::Sample(sample.timestamp(), sample.value());
}

::prometheus::Sample Converter::ToProtoSample(
    const core::Sample& sample) {
    ::prometheus::Sample proto_sample;
    proto_sample.set_timestamp(sample.timestamp());
    proto_sample.set_value(sample.value());
    return proto_sample;
}

} // namespace remote
} // namespace prometheus
} // namespace tsdb
