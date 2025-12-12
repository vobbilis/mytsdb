#pragma once

#include <filesystem>
#include <string>
#include <chrono>

#include <gtest/gtest.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace tsdb {
namespace testutil {

inline std::string SanitizeForPath(std::string s) {
    for (auto& ch : s) {
        if (ch == '/' || ch == '\\' || ch == ' ' || ch == ':' || ch == '\t' || ch == '\n' || ch == '\r') {
            ch = '_';
        }
    }
    return s;
}

// Creates a unique per-test directory under the system temp directory.
//
// Why: many integration tests previously used fixed directory names like
// `/tmp/tsdb_e2e_workflow_test`, which is unsafe when CTest runs tests in parallel
// processes (directory deletion races, corrupt state, and sporadic segfaults).
inline std::filesystem::path MakeUniqueTestDir(const std::string& prefix) {
    std::string name = prefix;

    if (const auto* info = ::testing::UnitTest::GetInstance()->current_test_info()) {
        name += "_" + std::string(info->test_suite_name()) + "_" + std::string(info->name());
    }

#if defined(__unix__) || defined(__APPLE__)
    name += "_pid" + std::to_string(static_cast<long long>(::getpid()));
#endif

    // Add a monotonic-ish suffix to avoid collisions even within the same process.
    name += "_t" + std::to_string(
        static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));

    return std::filesystem::temp_directory_path() / SanitizeForPath(name);
}

} // namespace testutil
} // namespace tsdb


