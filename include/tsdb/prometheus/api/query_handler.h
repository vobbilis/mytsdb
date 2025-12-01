#pragma once

#include <memory>
#include <string>
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/server/request.h"

namespace tsdb {
namespace prometheus {
namespace api {

class QueryHandler {
public:
    explicit QueryHandler(std::shared_ptr<promql::Engine> engine);

    void HandleInstantQuery(const Request& req, std::string& res);
    void HandleRangeQuery(const Request& req, std::string& res);
    void HandleLabelValues(const Request& req, std::string& res);

private:
    std::shared_ptr<promql::Engine> engine_;

    std::string FormatSuccessResponse(const promql::Value& value);
    std::string FormatErrorResponse(const std::string& error);
};

} // namespace api
} // namespace prometheus
} // namespace tsdb
