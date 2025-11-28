#include "tsdb/prometheus/api/query_handler.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace rapidjson;

namespace tsdb {
namespace prometheus {
namespace api {

namespace {
    int64_t ParseTime(const std::string& time_str) {
        if (time_str.empty()) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
        try {
            // Try parsing as float (seconds)
            double t = std::stod(time_str);
            return static_cast<int64_t>(t * 1000);
        } catch (...) {
            // TODO: Parse RFC3339 string
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    int64_t ParseDuration(const std::string& dur_str) {
        if (dur_str.empty()) return 0;
        try {
            // Simple parsing: assume seconds if number, or simple suffix
            // TODO: Full PromQL duration parsing
            char suffix = dur_str.back();
            if (isdigit(suffix)) {
                return static_cast<int64_t>(std::stod(dur_str) * 1000);
            }
            
            double val = std::stod(dur_str.substr(0, dur_str.length() - 1));
            switch (suffix) {
                case 's': return static_cast<int64_t>(val * 1000);
                case 'm': return static_cast<int64_t>(val * 60 * 1000);
                case 'h': return static_cast<int64_t>(val * 3600 * 1000);
                case 'd': return static_cast<int64_t>(val * 24 * 3600 * 1000);
                default: return static_cast<int64_t>(val * 1000);
            }
        } catch (...) {
            return 0;
        }
    }
}

QueryHandler::QueryHandler(std::shared_ptr<promql::Engine> engine)
    : engine_(std::move(engine)) {}

void QueryHandler::HandleInstantQuery(const Request& req, std::string& res) {
    std::string query = req.GetParam("query");
    std::string time_str = req.GetParam("time");

    if (query.empty()) {
        res = FormatErrorResponse("missing query parameter");
        return;
    }

    int64_t time = ParseTime(time_str);

    try {
        auto result = engine_->ExecuteInstant(query, time);
        if (!result.error.empty()) {
            res = FormatErrorResponse(result.error);
        } else {
            res = FormatSuccessResponse(result.value);
        }
    } catch (const std::exception& e) {
        res = FormatErrorResponse(e.what());
    }
}

void QueryHandler::HandleRangeQuery(const Request& req, std::string& res) {
    std::string query = req.GetParam("query");
    std::string start_str = req.GetParam("start");
    std::string end_str = req.GetParam("end");
    std::string step_str = req.GetParam("step");

    if (query.empty()) {
        res = FormatErrorResponse("missing query parameter");
        return;
    }

    int64_t start = ParseTime(start_str);
    int64_t end = ParseTime(end_str);
    int64_t step = ParseDuration(step_str);

    if (step <= 0) step = 1000; // Default 1s
    if (end < start) end = start;

    try {
        auto result = engine_->ExecuteRange(query, start, end, step);
        if (!result.error.empty()) {
            res = FormatErrorResponse(result.error);
        } else {
            res = FormatSuccessResponse(result.value);
        }
    } catch (const std::exception& e) {
        res = FormatErrorResponse(e.what());
    }
}

std::string QueryHandler::FormatSuccessResponse(const promql::Value& value) {
    Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    doc.AddMember("status", "success", allocator);
    
    Value data(kObjectType);
    
    std::string resultTypeStr;
    if (value.isVector()) resultTypeStr = "vector";
    else if (value.isMatrix()) resultTypeStr = "matrix";
    else if (value.isScalar()) resultTypeStr = "scalar";
    else resultTypeStr = "string";
    
    data.AddMember("resultType", StringRef(resultTypeStr.c_str()), allocator);
    
    Value result(kArrayType);
    
    if (value.isVector()) {
        for (const auto& sample : value.getVector()) {
            Value item(kObjectType);
            
            // Metric
            Value metric(kObjectType);
            for (const auto& [k, v] : sample.metric.labels()) {
                metric.AddMember(StringRef(k.c_str()), StringRef(v.c_str()), allocator);
            }
            item.AddMember("metric", metric, allocator);
            
            // Value
            Value val(kArrayType);
            val.PushBack(static_cast<double>(sample.timestamp) / 1000.0, allocator);
            std::string valStr = std::to_string(sample.value);
            val.PushBack(StringRef(valStr.c_str()), allocator); // Value as string
            item.AddMember("value", val, allocator);
            
            result.PushBack(item, allocator);
        }
    } else if (value.isMatrix()) {
        for (const auto& series : value.getMatrix()) {
            Value item(kObjectType);
            
            // Metric
            Value metric(kObjectType);
            for (const auto& [k, v] : series.metric.labels()) {
                metric.AddMember(StringRef(k.c_str()), StringRef(v.c_str()), allocator);
            }
            item.AddMember("metric", metric, allocator);
            
            // Values
            Value values(kArrayType);
            for (const auto& sample : series.samples) {
                Value val(kArrayType);
                val.PushBack(static_cast<double>(sample.timestamp()) / 1000.0, allocator);
                std::string valStr = std::to_string(sample.value());
                val.PushBack(Value(valStr.c_str(), allocator).Move(), allocator);
                values.PushBack(val, allocator);
            }
            item.AddMember("values", values, allocator);
            
            result.PushBack(item, allocator);
        }
    } else if (value.isScalar()) {
        Value val(kArrayType);
        val.PushBack(static_cast<double>(value.getScalar().timestamp) / 1000.0, allocator);
        std::string valStr = std::to_string(value.getScalar().value);
        val.PushBack(StringRef(valStr.c_str()), allocator);
        result.PushBack(val, allocator); // Scalar result is just [t, v] inside result array? No, scalar is usually just the value?
        // Prometheus API for scalar: result: [t, "v"]
        // Wait, result is array of results?
        // For scalar, result is just the array [t, "v"] directly?
        // Let's check docs. "result": [ <unix_time>, "<scalar_value>" ]
        // So result is NOT an array of items, but the item itself.
        // But I initialized result as kArrayType.
        // Let's fix this.
        // Actually, for consistency with vector/matrix, let's keep it as array for now, but Prometheus spec says:
        // resultType: "scalar", result: [t, "v"]
        // So I should overwrite result.
        
        result.SetArray(); // Clear
        result.PushBack(static_cast<double>(value.getScalar().timestamp) / 1000.0, allocator);
        result.PushBack(StringRef(valStr.c_str()), allocator);
    }
    
    data.AddMember("result", result, allocator);
    doc.AddMember("data", data, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

std::string QueryHandler::FormatErrorResponse(const std::string& error) {
    Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    doc.AddMember("status", "error", allocator);
    doc.AddMember("errorType", "execution", allocator);
    doc.AddMember("error", StringRef(error.c_str()), allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

} // namespace api
} // namespace prometheus
} // namespace tsdb
