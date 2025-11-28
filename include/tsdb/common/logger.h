#ifndef TSDB_COMMON_LOGGER_H_
#define TSDB_COMMON_LOGGER_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace tsdb {
namespace common {

class Logger {
public:
    static void Init();
    static void SetLevel(spdlog::level::level_enum level);
};

} // namespace common
} // namespace tsdb

// Macros for convenient logging
#define TSDB_TRACE(...) spdlog::trace(__VA_ARGS__)
#define TSDB_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define TSDB_INFO(...)  spdlog::info(__VA_ARGS__)
#define TSDB_WARN(...)  spdlog::warn(__VA_ARGS__)
#define TSDB_ERROR(...) spdlog::error(__VA_ARGS__)
#define TSDB_CRITICAL(...) spdlog::critical(__VA_ARGS__)

#endif // TSDB_COMMON_LOGGER_H_
