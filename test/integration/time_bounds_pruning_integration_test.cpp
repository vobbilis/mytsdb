#include <gtest/gtest.h>

#include "tsdb/core/config.h"
#include "tsdb/core/matcher.h"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/storage/storage_impl.h"
#include "test_util/temp_dir.h"

namespace tsdb {
namespace storage {
namespace {

core::TimeSeries MakeSeries(
    const std::string& metric,
    const std::string& instance,
    int64_t ts,
    double value) {
    core::Labels labels;
    labels.add("__name__", metric);
    labels.add("instance", instance);
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(ts, value));
    return series;
}

TEST(TimeBoundsPruningIntegrationTest, QueryPrunesNonOverlappingSeries) {
    auto test_dir = tsdb::testutil::MakeUniqueTestDir("tsdb_time_bounds_pruning_it");

    core::StorageConfig config;
    config.data_dir = test_dir.string();
    config.background_config.enable_background_processing = false;
    config.background_config.enable_auto_compaction = false;
    config.background_config.enable_auto_cleanup = false;
    config.background_config.enable_metrics_collection = false;

    StorageImpl storage;
    ASSERT_TRUE(storage.init(config).ok());

    ASSERT_TRUE(storage.write(MakeSeries("metric_early", "host1", 1000, 1.0)).ok());
    ASSERT_TRUE(storage.write(MakeSeries("metric_late", "host1", 100000, 2.0)).ok());

    ReadPerformanceInstrumentation::instance().reset_stats();

    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "instance", "host1");

    auto res = storage.query(matchers, 0, 5000);
    ASSERT_TRUE(res.ok()) << res.error();
    ASSERT_EQ(res.value().size(), 1);
    EXPECT_EQ(res.value()[0].labels().get("__name__").value(), "metric_early");

    auto stats = ReadPerformanceInstrumentation::instance().get_stats();
    EXPECT_GE(stats.series_time_bounds_checks, 1u);
    EXPECT_EQ(stats.series_time_bounds_pruned, 1u);

    ASSERT_TRUE(storage.close().ok());
}

}  // namespace
}  // namespace storage
}  // namespace tsdb


