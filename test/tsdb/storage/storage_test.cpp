#include <gtest/gtest.h>
#include "tsdb/storage/storage.h"
#include <filesystem>
#include <random>
#include <thread>
#include <future>
#include <limits>

namespace tsdb {
namespace storage {
namespace {

// Helper function to generate test samples
std::vector<core::Sample> GenerateTestSamples(
    core::Timestamp start,
    core::Timestamp interval,
    size_t count,
    std::function<double(size_t)> value_fn) {
    std::vector<core::Sample> samples;
    samples.reserve(count);
    for (size_t i = 0; i < count; i++) {
        samples.emplace_back(start + i * interval, value_fn(i));
    }
    return samples;
}

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_test";
        std::filesystem::create_directories(test_dir_);
        
        StorageOptions options;
        options.data_dir = test_dir_.string();
        options.block_size = 4096;  // Small blocks for testing
        storage_ = CreateStorage(options);
    }
    
    void TearDown() override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    // Helper to create a test series
    core::Result<core::SeriesID> CreateTestSeries(
        const std::string& name,
        const std::string& instance = "test",
        core::MetricType type = core::MetricType::GAUGE,
        const core::Granularity& granularity = core::Granularity::Normal()) {
        core::Labels labels = {
            {"__name__", name},
            {"instance", instance}
        };
        return storage_->CreateSeries(labels, type, granularity);
    }
    
    std::filesystem::path test_dir_;
    std::unique_ptr<Storage> storage_;
};

TEST_F(StorageTest, CreateAndReadSeries) {
    // Create a series
    core::Labels labels = {{"name", "test_metric"}, {"host", "localhost"}};
    auto series_id = storage_->CreateSeries(
        labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    ASSERT_TRUE(series_id.ok());
    
    // Write some samples
    std::vector<core::Sample> samples = {
        {1000, 1.0},
        {2000, 2.0},
        {3000, 3.0}
    };
    auto write_result = storage_->Write(series_id.value(), samples);
    ASSERT_TRUE(write_result.ok());
    
    // Read the samples back
    auto read_result = storage_->Read(series_id.value(), 0, 4000);
    ASSERT_TRUE(read_result.ok());
    
    const auto& read_samples = read_result.value();
    ASSERT_EQ(read_samples.size(), 3);
    
    for (size_t i = 0; i < samples.size(); i++) {
        EXPECT_EQ(read_samples[i].timestamp, samples[i].timestamp);
        EXPECT_DOUBLE_EQ(read_samples[i].value, samples[i].value);
    }
}

TEST_F(StorageTest, QuerySeries) {
    // Create multiple series
    core::Labels labels1 = {{"name", "test_metric"}, {"host", "host1"}};
    core::Labels labels2 = {{"name", "test_metric"}, {"host", "host2"}};
    
    auto series1_id = storage_->CreateSeries(
        labels1,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    ASSERT_TRUE(series1_id.ok());
    
    auto series2_id = storage_->CreateSeries(
        labels2,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    ASSERT_TRUE(series2_id.ok());
    
    // Write samples to both series
    std::vector<core::Sample> samples1 = {{1000, 1.0}, {2000, 2.0}};
    std::vector<core::Sample> samples2 = {{1000, 10.0}, {2000, 20.0}};
    
    ASSERT_TRUE(storage_->Write(series1_id.value(), samples1).ok());
    ASSERT_TRUE(storage_->Write(series2_id.value(), samples2).ok());
    
    // Query series by label matcher
    core::Labels matcher = {{"name", "test_metric"}};
    auto query_result = storage_->Query(matcher, 0, 3000);
    ASSERT_TRUE(query_result.ok());
    ASSERT_EQ(query_result.value().size(), 2);
    
    // Query series with specific host
    core::Labels host_matcher = {{"host", "host1"}};
    query_result = storage_->Query(host_matcher, 0, 3000);
    ASSERT_TRUE(query_result.ok());
    ASSERT_EQ(query_result.value().size(), 1);
}

TEST_F(StorageTest, DeleteSeries) {
    // Create a series
    core::Labels labels = {{"name", "test_metric"}};
    auto series_id = storage_->CreateSeries(
        labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    ASSERT_TRUE(series_id.ok());
    
    // Write some samples
    std::vector<core::Sample> samples = {{1000, 1.0}};
    ASSERT_TRUE(storage_->Write(series_id.value(), samples).ok());
    
    // Delete the series
    ASSERT_TRUE(storage_->DeleteSeries(series_id.value()).ok());
    
    // Try to read from deleted series
    auto read_result = storage_->Read(series_id.value(), 0, 2000);
    ASSERT_FALSE(read_result.ok());
    EXPECT_EQ(read_result.error().code(), core::Error::Code::NOT_FOUND);
}

TEST_F(StorageTest, HighFrequencyData) {
    // Create a high-frequency series
    core::Labels labels = {{"name", "high_freq_metric"}};
    auto series_id = storage_->CreateSeries(
        labels,
        core::MetricType::GAUGE,
        core::Granularity::HighFrequency());
    ASSERT_TRUE(series_id.ok());
    
    // Generate 10000 samples with 100μs intervals
    std::vector<core::Sample> samples;
    samples.reserve(10000);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> d(100.0, 10.0);
    
    core::Timestamp ts = 0;
    for (int i = 0; i < 10000; i++) {
        samples.push_back({ts, d(gen)});
        ts += 100;  // 100μs intervals
    }
    
    // Write samples in batches
    const size_t batch_size = 1000;
    for (size_t i = 0; i < samples.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, samples.size());
        std::vector<core::Sample> batch(
            samples.begin() + i,
            samples.begin() + end);
        ASSERT_TRUE(storage_->Write(series_id.value(), batch).ok());
    }
    
    // Read all samples back
    auto read_result = storage_->Read(series_id.value(), 0, ts);
    ASSERT_TRUE(read_result.ok());
    
    const auto& read_samples = read_result.value();
    ASSERT_EQ(read_samples.size(), samples.size());
    
    // Verify timestamps are monotonically increasing
    for (size_t i = 1; i < read_samples.size(); i++) {
        EXPECT_LT(read_samples[i-1].timestamp, read_samples[i].timestamp);
    }
}

// New comprehensive tests

// Test block management
TEST_F(StorageTest, BlockManagement) {
    auto series_id = CreateTestSeries("test_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Write enough samples to create multiple blocks
    const size_t samples_per_block = 100;
    const size_t num_blocks = 5;
    
    for (size_t block = 0; block < num_blocks; block++) {
        auto samples = GenerateTestSamples(
            block * 1000,
            10,
            samples_per_block,
            [](size_t i) { return static_cast<double>(i); });
        
        ASSERT_TRUE(storage_->Write(series_id.value(), samples).ok());
    }
    
    // Verify all samples can be read
    auto result = storage_->Read(
        series_id.value(),
        0,
        num_blocks * 1000 + samples_per_block * 10);
    
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), num_blocks * samples_per_block);
    
    // Verify samples are in order
    for (size_t i = 1; i < result.value().size(); i++) {
        EXPECT_LT(result.value()[i-1].timestamp, result.value()[i].timestamp);
    }
}

// Test concurrent operations
TEST_F(StorageTest, ConcurrentOperations) {
    const size_t num_series = 10;
    const size_t num_samples = 1000;
    const size_t num_threads = 4;
    
    // Create test series
    std::vector<core::SeriesID> series_ids;
    for (size_t i = 0; i < num_series; i++) {
        auto series_id = CreateTestSeries(
            "concurrent_metric",
            "instance_" + std::to_string(i));
        ASSERT_TRUE(series_id.ok());
        series_ids.push_back(series_id.value());
    }
    
    // Launch concurrent writers
    std::vector<std::future<void>> futures;
    for (size_t t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            for (size_t i = 0; i < num_samples; i++) {
                size_t series_idx = (t + i) % num_series;
                std::vector<core::Sample> samples = {{
                    static_cast<core::Timestamp>(i * 1000),
                    static_cast<double>(t * num_samples + i)
                }};
                auto result = storage_->Write(series_ids[series_idx], samples);
                EXPECT_TRUE(result.ok()) << result.error().what();
            }
        }));
    }
    
    // Wait for all writers
    for (auto& future : futures) {
        future.get();
    }
    
    // Verify data integrity
    for (auto series_id : series_ids) {
        auto result = storage_->Read(series_id, 0, num_samples * 1000);
        ASSERT_TRUE(result.ok());
        EXPECT_GT(result.value().size(), 0);
        
        // Verify timestamps are ordered
        for (size_t i = 1; i < result.value().size(); i++) {
            EXPECT_LE(result.value()[i-1].timestamp, result.value()[i].timestamp);
        }
    }
}

// Test error conditions
TEST_F(StorageTest, ErrorConditions) {
    // Test invalid series ID
    auto result = storage_->Read(999999, 0, 1000);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::NOT_FOUND);
    
    // Test invalid time range
    auto series_id = CreateTestSeries("error_metric");
    ASSERT_TRUE(series_id.ok());
    
    result = storage_->Read(series_id.value(), 1000, 0);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test duplicate series
    auto series2 = CreateTestSeries("error_metric", "test");
    EXPECT_FALSE(series2.ok());
    EXPECT_EQ(series2.error().code(), core::Error::Code::INVALID_ARGUMENT);
}

// Test different granularities
TEST_F(StorageTest, GranularityHandling) {
    // Create series with different granularities
    auto high_freq = CreateTestSeries(
        "high_freq_metric", "test", core::MetricType::GAUGE,
        core::Granularity::HighFrequency());
    ASSERT_TRUE(high_freq.ok());
    
    auto normal = CreateTestSeries(
        "normal_metric", "test", core::MetricType::GAUGE,
        core::Granularity::Normal());
    ASSERT_TRUE(normal.ok());
    
    auto low_freq = CreateTestSeries(
        "low_freq_metric", "test", core::MetricType::GAUGE,
        core::Granularity::LowFrequency());
    ASSERT_TRUE(low_freq.ok());
    
    // Write samples with appropriate intervals
    auto high_freq_samples = GenerateTestSamples(
        0, 100, 1000,  // 100μs intervals
        [](size_t i) { return static_cast<double>(i); });
    ASSERT_TRUE(storage_->Write(high_freq.value(), high_freq_samples).ok());
    
    auto normal_samples = GenerateTestSamples(
        0, 1000, 100,  // 1ms intervals
        [](size_t i) { return static_cast<double>(i); });
    ASSERT_TRUE(storage_->Write(normal.value(), normal_samples).ok());
    
    auto low_freq_samples = GenerateTestSamples(
        0, 60000, 10,  // 60s intervals
        [](size_t i) { return static_cast<double>(i); });
    ASSERT_TRUE(storage_->Write(low_freq.value(), low_freq_samples).ok());
    
    // Verify samples were written correctly
    auto result = storage_->Read(high_freq.value(), 0, 1000000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 1000);
    
    result = storage_->Read(normal.value(), 0, 1000000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 100);
    
    result = storage_->Read(low_freq.value(), 0, 1000000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 10);
}

// Test compaction
TEST_F(StorageTest, Compaction) {
    auto series_id = CreateTestSeries("compaction_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Write samples in multiple blocks
    const size_t blocks = 5;
    const size_t samples_per_block = 100;
    
    for (size_t block = 0; block < blocks; block++) {
        auto samples = GenerateTestSamples(
            block * 1000,
            10,
            samples_per_block,
            [](size_t i) { return static_cast<double>(i); });
        
        ASSERT_TRUE(storage_->Write(series_id.value(), samples).ok());
    }
    
    // Trigger compaction
    ASSERT_TRUE(storage_->Compact().ok());
    
    // Verify data is still accessible
    auto result = storage_->Read(
        series_id.value(),
        0,
        blocks * 1000 + samples_per_block * 10);
    
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), blocks * samples_per_block);
    
    // Verify data ordering
    for (size_t i = 1; i < result.value().size(); i++) {
        EXPECT_LT(result.value()[i-1].timestamp, result.value()[i].timestamp);
    }
}

// Test query performance with large datasets
TEST_F(StorageTest, LargeDatasetQuery) {
    const size_t num_series = 100;
    const size_t samples_per_series = 10000;
    
    // Create series with different labels
    std::vector<core::SeriesID> series_ids;
    for (size_t i = 0; i < num_series; i++) {
        auto series_id = CreateTestSeries(
            "large_metric",
            "instance_" + std::to_string(i % 10),  // 10 different instances
            core::MetricType::GAUGE,
            core::Granularity::Normal());
        ASSERT_TRUE(series_id.ok());
        series_ids.push_back(series_id.value());
        
        // Write samples
        auto samples = GenerateTestSamples(
            0, 1000, samples_per_series,
            [](size_t i) { return static_cast<double>(i); });
        ASSERT_TRUE(storage_->Write(series_id.value(), samples).ok());
    }
    
    // Test different query patterns
    
    // 1. Query by instance
    core::Labels instance_query = {
        {"instance", "instance_0"}
    };
    auto result = storage_->Query(instance_query, 0, samples_per_series * 1000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 10);  // 10 series per instance
    
    // 2. Query by time range
    core::Labels all_query = {
        {"__name__", "large_metric"}
    };
    result = storage_->Query(all_query, 0, 1000);  // First second only
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), num_series);
    
    // 3. Query with multiple label matchers
    core::Labels complex_query = {
        {"__name__", "large_metric"},
        {"instance", "instance_5"}
    };
    result = storage_->Query(complex_query, 5000, 6000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 10);
}

// Test boundary conditions for timestamps
TEST_F(StorageTest, TimestampBoundaries) {
    auto series_id = CreateTestSeries("boundary_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Test minimum timestamp
    std::vector<core::Sample> samples = {
        {std::numeric_limits<core::Timestamp>::min(), 1.0}
    };
    auto result = storage_->Write(series_id.value(), samples);
    EXPECT_TRUE(result.ok()) << result.error().what();
    
    // Test maximum timestamp
    samples = {{std::numeric_limits<core::Timestamp>::max(), 1.0}};
    result = storage_->Write(series_id.value(), samples);
    EXPECT_TRUE(result.ok()) << result.error().what();
    
    // Test reading across full timestamp range
    auto read_result = storage_->Read(
        series_id.value(),
        std::numeric_limits<core::Timestamp>::min(),
        std::numeric_limits<core::Timestamp>::max());
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(read_result.value().size(), 2);
    
    // Test zero timestamp
    samples = {{0, 1.0}};
    result = storage_->Write(series_id.value(), samples);
    EXPECT_TRUE(result.ok());
    
    // Test negative timestamp (should fail)
    samples = {{-1, 1.0}};
    result = storage_->Write(series_id.value(), samples);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
}

// Test boundary conditions for values
TEST_F(StorageTest, ValueBoundaries) {
    auto series_id = CreateTestSeries("value_boundary_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Test special floating point values
    std::vector<core::Sample> samples = {
        {1000, std::numeric_limits<double>::infinity()},     // Infinity
        {2000, -std::numeric_limits<double>::infinity()},    // Negative infinity
        {3000, std::numeric_limits<double>::quiet_NaN()},    // NaN
        {4000, std::numeric_limits<double>::min()},          // Minimum normal
        {5000, std::numeric_limits<double>::max()},          // Maximum normal
        {6000, std::numeric_limits<double>::denorm_min()},   // Minimum denormal
        {7000, -0.0},                                        // Negative zero
        {8000, 0.0}                                         // Positive zero
    };
    
    // Write special values
    auto result = storage_->Write(series_id.value(), samples);
    ASSERT_TRUE(result.ok());
    
    // Read back and verify
    auto read_result = storage_->Read(series_id.value(), 0, 9000);
    ASSERT_TRUE(read_result.ok());
    const auto& read_samples = read_result.value();
    
    EXPECT_TRUE(std::isinf(read_samples[0].value) && read_samples[0].value > 0);
    EXPECT_TRUE(std::isinf(read_samples[1].value) && read_samples[1].value < 0);
    EXPECT_TRUE(std::isnan(read_samples[2].value));
    EXPECT_DOUBLE_EQ(read_samples[3].value, std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(read_samples[4].value, std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(read_samples[5].value, std::numeric_limits<double>::denorm_min());
    EXPECT_TRUE(std::signbit(read_samples[6].value));  // Check for negative zero
    EXPECT_FALSE(std::signbit(read_samples[7].value)); // Check for positive zero
}

// Test series label boundaries
TEST_F(StorageTest, LabelBoundaries) {
    // Test empty labels (should fail)
    core::Labels empty_labels;
    auto result = storage_->CreateSeries(
        empty_labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test empty label name (should fail)
    core::Labels invalid_name = {{"", "value"}};
    result = storage_->CreateSeries(
        invalid_name,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test empty label value (should fail)
    core::Labels invalid_value = {{"name", ""}};
    result = storage_->CreateSeries(
        invalid_value,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test very long label name and value
    std::string long_string(1024 * 1024, 'a');  // 1MB string
    core::Labels long_labels = {{"name", long_string}};
    result = storage_->CreateSeries(
        long_labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test invalid characters in labels
    core::Labels invalid_chars = {{"name\n", "value"}, {"key", "value\0"}};
    result = storage_->CreateSeries(
        invalid_chars,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
}

// Test block boundaries
TEST_F(StorageTest, BlockBoundaries) {
    auto series_id = CreateTestSeries("block_boundary_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Test writing exactly block size samples
    const size_t samples_per_block = 4096 / sizeof(core::Sample);
    std::vector<core::Sample> samples;
    samples.reserve(samples_per_block);
    
    for (size_t i = 0; i < samples_per_block; i++) {
        samples.emplace_back(i * 1000, static_cast<double>(i));
    }
    
    auto result = storage_->Write(series_id.value(), samples);
    EXPECT_TRUE(result.ok());
    
    // Test writing one more sample (should create new block)
    std::vector<core::Sample> one_more = {{samples_per_block * 1000, 0.0}};
    result = storage_->Write(series_id.value(), one_more);
    EXPECT_TRUE(result.ok());
    
    // Verify all samples are readable
    auto read_result = storage_->Read(
        series_id.value(), 0, (samples_per_block + 1) * 1000);
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(read_result.value().size(), samples_per_block + 1);
}

// Test concurrent series creation and deletion
TEST_F(StorageTest, ConcurrentSeriesManagement) {
    const size_t num_threads = 4;
    const size_t series_per_thread = 100;
    
    // Create series concurrently
    std::vector<std::future<void>> futures;
    std::vector<std::vector<core::SeriesID>> thread_series(num_threads);
    
    for (size_t t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async,
            [this, t, series_per_thread, &thread_series]() {
                for (size_t i = 0; i < series_per_thread; i++) {
                    auto series_id = CreateTestSeries(
                        "concurrent_series",
                        "instance_" + std::to_string(t) + "_" + std::to_string(i));
                    EXPECT_TRUE(series_id.ok()) << series_id.error().what();
                    if (series_id.ok()) {
                        thread_series[t].push_back(series_id.value());
                    }
                }
            }));
    }
    
    // Wait for creation to complete
    for (auto& future : futures) {
        future.get();
    }
    
    // Verify total number of series
    EXPECT_EQ(storage_->NumSeries(), num_threads * series_per_thread);
    
    // Delete series concurrently
    futures.clear();
    for (size_t t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async,
            [this, &thread_series, t]() {
                for (auto series_id : thread_series[t]) {
                    auto result = storage_->DeleteSeries(series_id);
                    EXPECT_TRUE(result.ok()) << result.error().what();
                }
            }));
    }
    
    // Wait for deletion to complete
    for (auto& future : futures) {
        future.get();
    }
    
    // Verify all series were deleted
    EXPECT_EQ(storage_->NumSeries(), 0);
}

// Test out-of-order writes
TEST_F(StorageTest, OutOfOrderWrites) {
    auto series_id = CreateTestSeries("out_of_order_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Write samples in reverse order
    std::vector<core::Sample> samples = {
        {3000, 3.0},
        {2000, 2.0},
        {1000, 1.0}
    };
    
    auto result = storage_->Write(series_id.value(), samples);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Write samples with duplicate timestamps
    samples = {
        {1000, 1.0},
        {1000, 2.0}
    };
    
    result = storage_->Write(series_id.value(), samples);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Write interleaved samples
    samples = {{1000, 1.0}};
    result = storage_->Write(series_id.value(), samples);
    ASSERT_TRUE(result.ok());
    
    samples = {{500, 0.5}};  // Earlier timestamp
    result = storage_->Write(series_id.value(), samples);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
}

// Test query edge cases
TEST_F(StorageTest, QueryEdgeCases) {
    // Create test series
    auto series_id = CreateTestSeries("query_edge_metric");
    ASSERT_TRUE(series_id.ok());
    
    // Write single sample
    std::vector<core::Sample> samples = {{1000, 1.0}};
    ASSERT_TRUE(storage_->Write(series_id.value(), samples).ok());
    
    // Test empty time range
    auto result = storage_->Read(series_id.value(), 1000, 1000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 1);  // Should include boundary
    
    // Test zero-length time range
    result = storage_->Read(series_id.value(), 1000, 999);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error().code(), core::Error::Code::INVALID_ARGUMENT);
    
    // Test range before data
    result = storage_->Read(series_id.value(), 0, 500);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.value().empty());
    
    // Test range after data
    result = storage_->Read(series_id.value(), 2000, 3000);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.value().empty());
    
    // Test exact boundary matches
    result = storage_->Read(series_id.value(), 1000, 1000);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 1);
    
    // Test label matchers
    core::Labels exact_match = {
        {"__name__", "query_edge_metric"}
    };
    auto query_result = storage_->Query(exact_match, 0, 2000);
    ASSERT_TRUE(query_result.ok());
    EXPECT_EQ(query_result.value().size(), 1);
    
    // Test non-existent label
    core::Labels no_match = {
        {"non_existent", "value"}
    };
    query_result = storage_->Query(no_match, 0, 2000);
    ASSERT_TRUE(query_result.ok());
    EXPECT_TRUE(query_result.value().empty());
    
    // Test partial label match
    core::Labels partial_match = {
        {"__name__", "query_edge_metric"},
        {"non_existent", "value"}
    };
    query_result = storage_->Query(partial_match, 0, 2000);
    ASSERT_TRUE(query_result.ok());
    EXPECT_TRUE(query_result.value().empty());
}

} // namespace
} // namespace storage
} // namespace tsdb 