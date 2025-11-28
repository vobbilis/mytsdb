# Design: Global Logger

## 1. Problem Statement
The codebase currently uses ad-hoc logging mechanisms, including direct `std::cout`/`std::cerr` calls and local macro definitions (e.g., in `storage_impl.cpp`). This makes it difficult to:
*   Control log verbosity globally.
*   Redirect logs to files or other sinks.
*   Ensure consistent log formatting.

## 2. Goals
*   **Unified Interface**: Provide a single, consistent API for logging across the entire codebase.
*   **Global Control**: Allow setting the log level (DEBUG, INFO, WARN, ERROR) globally at runtime.
*   **Backend Agnostic**: Wrap the underlying logging library (`spdlog`) so it can be swapped or disabled.
*   **Performance**: Minimal overhead when logging is disabled or below the current level.

## 3. Proposed Architecture

### 3.1. Interface (`include/tsdb/common/logger.h`)

```cpp
namespace tsdb {
namespace common {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    OFF
};

class Logger {
public:
    static void Init(LogLevel level = LogLevel::INFO);
    static void SetLevel(LogLevel level);
    static LogLevel GetLevel();
    
    template<typename... Args>
    static void Log(LogLevel level, const char* fmt, const Args&... args);
};

} // namespace common
} // namespace tsdb

// Macros for convenience and zero-overhead when disabled at compile time (optional)
#define TSDB_INFO(...)  tsdb::common::Logger::Log(tsdb::common::LogLevel::INFO, __VA_ARGS__)
#define TSDB_WARN(...)  tsdb::common::Logger::Log(tsdb::common::LogLevel::WARN, __VA_ARGS__)
#define TSDB_ERROR(...) tsdb::common::Logger::Log(tsdb::common::LogLevel::ERROR, __VA_ARGS__)
#define TSDB_DEBUG(...) tsdb::common::Logger::Log(tsdb::common::LogLevel::DEBUG, __VA_ARGS__)
```

### 3.2. Implementation (`src/tsdb/common/logger.cpp`)
*   Uses `spdlog` singleton registry.
*   Initializes a console sink (stdout/stderr) by default.
*   `SetLevel` maps to `spdlog::set_level`.

## 4. Migration Plan
1.  Implement `Logger` class.
2.  Update `StorageImpl` to use `TSDB_*` macros instead of local `#define spdlog_*`.
3.  Update `WAL`, `AsyncWALShard`, `Index` to use `TSDB_*` macros.
4.  Remove direct `std::cout`/`std::cerr` usage in core logic (keep in CLI tools if needed).

## 5. Build System
*   Add `src/tsdb/common/logger.cpp` to `tsdb_lib` sources.
*   Ensure `spdlog` is linked.
