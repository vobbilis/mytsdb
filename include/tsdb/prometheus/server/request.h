#pragma once

#include <string>
#include <map>
#include <vector>

namespace tsdb {
namespace prometheus {

struct Request {
    std::string method;
    std::string path;
    std::multimap<std::string, std::string> params;
    std::map<std::string, std::string> path_params;
    std::string body;
    std::map<std::string, std::string> headers;
    
    std::string GetParam(const std::string& key) const {
        auto it = params.find(key);
        if (it != params.end()) {
            return it->second;
        }
        return "";
    }

    std::vector<std::string> GetMultiParam(const std::string& key) const {
        std::vector<std::string> values;
        auto range = params.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            values.push_back(it->second);
        }
        return values;
    }

    std::string GetPathParam(const std::string& key) const {
        auto it = path_params.find(key);
        if (it != path_params.end()) {
            return it->second;
        }
        return "";
    }
};

} // namespace prometheus
} // namespace tsdb
