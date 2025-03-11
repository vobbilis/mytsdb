#pragma once

#include <random>
#include <chrono>
#include <string>
#include <map>
#include <vector>
#include "tsdb/types/series.hpp"

namespace tsdb::test {

class TestUtils {
public:
    static Series generateTestSeries(
        const std::map<std::string, std::string>& labels,
        size_t numSamples
    ) {
        Series series;
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 100.0);

        // Add labels
        for (const auto& [key, value] : labels) {
            series.addLabel(key, value);
        }

        // Add samples
        for (size_t i = 0; i < numSamples; ++i) {
            series.addSample(
                nowMs - (i * 1000),  // 1 second intervals
                dis(gen)
            );
        }

        return series;
    }

    static bool compareFloat64(double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) <= epsilon;
    }

    static std::vector<Series> generateTestData(
        size_t numSeries,
        size_t samplesPerSeries
    ) {
        std::vector<Series> series;
        series.reserve(numSeries);

        for (size_t i = 0; i < numSeries; ++i) {
            std::map<std::string, std::string> labels{
                {"__name__", "test_metric_" + std::to_string(i)},
                {"instance", "instance-" + std::to_string(i % 3)},
                {"job", "job-" + std::to_string(i % 2)}
            };
            series.push_back(generateTestSeries(labels, samplesPerSeries));
        }

        return series;
    }
};

} // namespace tsdb::test 