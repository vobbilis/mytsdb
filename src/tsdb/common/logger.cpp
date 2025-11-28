#include "tsdb/common/logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace tsdb {
namespace common {

void Logger::Init() {
    try {
        auto console = spdlog::stdout_color_mt("console");
        spdlog::set_default_logger(console);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
        spdlog::set_level(spdlog::level::info);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void Logger::SetLevel(spdlog::level::level_enum level) {
    spdlog::set_level(level);
}

} // namespace common
} // namespace tsdb
