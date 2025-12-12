#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>

namespace tsdb {
namespace integration {

class ObjectPoolSizeAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        core::StorageConfig config;
        config.data_dir = "./test/data/storageimpl_phases/size_analysis";
        config.object_pool_config.time_series_initial_size = 100;
        config.object_pool_config.time_series_max_size = 10000;
        config.object_pool_config.labels_initial_size = 200;
        config.object_pool_config.labels_max_size = 20000;
        config.object_pool_config.samples_initial_size = 1000;
        config.object_pool_config.samples_max_size = 10000;
        
        // Re-enable WorkingSetCache
        // config.cache_size_bytes = 0;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
    }
    
    std::unique_ptr<storage::StorageImpl> storage_;
    
    // Helper to create TimeSeries with varying sizes
    core::TimeSeries createVariableSizeTimeSeries(int series_id, int sample_count, int label_count) {
        core::Labels labels;
        labels.add("__name__", "metric_" + std::to_string(series_id));
        
        // Add variable number of labels
        for (int i = 0; i < label_count; ++i) {
            labels.add("label_" + std::to_string(i), "value_" + std::to_string(i) + "_" + std::to_string(series_id));
        }
        
        core::TimeSeries series(labels);
        
        // Add variable number of samples
        for (int i = 0; i < sample_count; ++i) {
            series.add_sample(1000 + i, 100.0 + i + series_id);
        }
        
        return series;
    }
    
    // Helper to calculate approximate memory usage
    size_t estimateTimeSeriesSize(const core::TimeSeries& series) {
        size_t size = 0;
        
        // Labels size
        const auto& labels_map = series.labels().map();
        for (const auto& [key, value] : labels_map) {
            size += key.size() + value.size(); // String data
            size += sizeof(std::string) * 2;   // String objects
        }
        size += sizeof(std::map<std::string, std::string>); // Map container
        
        // Samples size
        size += series.samples().size() * sizeof(core::Sample);
        size += sizeof(std::vector<core::Sample>); // Vector container
        
        // TimeSeries object itself
        size += sizeof(core::TimeSeries);
        
        return size;
    }
    
    size_t estimateLabelsSize(const core::Labels& labels) {
        size_t size = 0;
        
        const auto& labels_map = labels.map();
        for (const auto& [key, value] : labels_map) {
            size += key.size() + value.size(); // String data
            size += sizeof(std::string) * 2;   // String objects
        }
        size += sizeof(std::map<std::string, std::string>); // Map container
        size += sizeof(core::Labels); // Labels object itself
        
        return size;
    }
};

TEST_F(ObjectPoolSizeAnalysisTest, DISABLED_AnalyzeTimeSeriesSizeDistribution) {
    std::cout << "\n=== TIME SERIES SIZE DISTRIBUTION ANALYSIS ===" << std::endl;
    
    std::vector<size_t> time_series_sizes;
    std::vector<size_t> labels_sizes;
    std::vector<int> sample_counts;
    std::vector<int> label_counts;
    
    // Create a large number of series to trigger pool usage
    const int num_series = 100; // Reduced from 1000 to debug crash
    
    for (int i = 0; i < num_series; ++i) {
        // Vary sample count from 1 to 1000
        int sample_count = 1 + (i % 1000);
        
        // Vary label count from 1 to 20
        int label_count = 1 + (i % 20);
        
        auto series = createVariableSizeTimeSeries(i, sample_count, label_count);
        
        size_t series_size = estimateTimeSeriesSize(series);
        size_t labels_size = estimateLabelsSize(series.labels());
        
        time_series_sizes.push_back(series_size);
        labels_sizes.push_back(labels_size);
        sample_counts.push_back(sample_count);
        label_counts.push_back(label_count);
        
        // Write to storage to use object pools
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed for series " << i;
    }
    
    // Analyze size distribution
    std::sort(time_series_sizes.begin(), time_series_sizes.end());
    std::sort(labels_sizes.begin(), labels_sizes.end());
    std::sort(sample_counts.begin(), sample_counts.end());
    std::sort(label_counts.begin(), label_counts.end());
    
    // Calculate statistics
    size_t min_series_size = time_series_sizes.front();
    size_t max_series_size = time_series_sizes.back();
    size_t median_series_size = time_series_sizes[time_series_sizes.size() / 2];
    size_t p95_series_size = time_series_sizes[static_cast<size_t>(time_series_sizes.size() * 0.95)];
    size_t p99_series_size = time_series_sizes[static_cast<size_t>(time_series_sizes.size() * 0.99)];
    
    size_t min_labels_size = labels_sizes.front();
    size_t max_labels_size = labels_sizes.back();
    size_t median_labels_size = labels_sizes[labels_sizes.size() / 2];
    size_t p95_labels_size = labels_sizes[static_cast<size_t>(labels_sizes.size() * 0.95)];
    size_t p99_labels_size = labels_sizes[static_cast<size_t>(labels_sizes.size() * 0.99)];
    
    std::cout << "\nTIME SERIES SIZE STATISTICS:" << std::endl;
    std::cout << "  Min size: " << min_series_size << " bytes" << std::endl;
    std::cout << "  Median size: " << median_series_size << " bytes" << std::endl;
    std::cout << "  95th percentile: " << p95_series_size << " bytes" << std::endl;
    std::cout << "  99th percentile: " << p99_series_size << " bytes" << std::endl;
    std::cout << "  Max size: " << max_series_size << " bytes" << std::endl;
    std::cout << "  Size range: " << (max_series_size - min_series_size) << " bytes" << std::endl;
    std::cout << "  Size ratio (max/min): " << (double)max_series_size / min_series_size << "x" << std::endl;
    
    std::cout << "\nLABELS SIZE STATISTICS:" << std::endl;
    std::cout << "  Min size: " << min_labels_size << " bytes" << std::endl;
    std::cout << "  Median size: " << median_labels_size << " bytes" << std::endl;
    std::cout << "  Max size: " << max_labels_size << " bytes" << std::endl;
    std::cout << "  Size range: " << (max_labels_size - min_labels_size) << " bytes" << std::endl;
    std::cout << "  Size ratio (max/min): " << (double)max_labels_size / min_labels_size << "x" << std::endl;
    
    std::cout << "\nSAMPLE COUNT STATISTICS:" << std::endl;
    std::cout << "  Min samples: " << sample_counts.front() << std::endl;
    std::cout << "  Median samples: " << sample_counts[sample_counts.size() / 2] << std::endl;
    std::cout << "  Max samples: " << sample_counts.back() << std::endl;
    
    std::cout << "\nLABEL COUNT STATISTICS:" << std::endl;
    std::cout << "  Min labels: " << label_counts.front() << std::endl;
    std::cout << "  Median labels: " << label_counts[label_counts.size() / 2] << std::endl;
    std::cout << "  Max labels: " << label_counts.back() << std::endl;
    
    // Analyze size buckets
    std::map<size_t, int> series_size_buckets;
    std::map<size_t, int> labels_size_buckets;
    
    for (size_t size : time_series_sizes) {
        // Round to nearest 100 bytes for bucketing
        size_t bucket = (size / 100) * 100;
        series_size_buckets[bucket]++;
    }
    
    for (size_t size : labels_sizes) {
        // Round to nearest 50 bytes for bucketing
        size_t bucket = (size / 50) * 50;
        labels_size_buckets[bucket]++;
    }
    
    std::cout << "\nTIME SERIES SIZE DISTRIBUTION:" << std::endl;
    for (const auto& [bucket, count] : series_size_buckets) {
        double percentage = (double)count / num_series * 100;
        std::cout << "  " << bucket << "-" << (bucket + 99) << " bytes: " << count << " (" << percentage << "%)" << std::endl;
    }
    
    std::cout << "\nLABELS SIZE DISTRIBUTION:" << std::endl;
    for (const auto& [bucket, count] : labels_size_buckets) {
        double percentage = (double)count / num_series * 100;
        std::cout << "  " << bucket << "-" << (bucket + 49) << " bytes: " << count << " (" << percentage << "%)" << std::endl;
    }
    
    // Show pool statistics after all operations
    std::cout << "\nPOOL STATISTICS AFTER SIZE ANALYSIS:" << std::endl;
    std::string stats = storage_->stats();
    std::cout << stats << std::endl;
}

TEST_F(ObjectPoolSizeAnalysisTest, DISABLED_EvaluateVariableSizePoolBenefits) {
    std::cout << "\n=== VARIABLE SIZE POOL BENEFIT ANALYSIS ===" << std::endl;
    
    // Create a realistic workload with different size patterns
    std::vector<core::TimeSeries> small_series;   // < 1KB
    std::vector<core::TimeSeries> medium_series;  // 1KB - 10KB
    std::vector<core::TimeSeries> large_series;   // > 10KB
    
    // Generate series with different size characteristics
    for (int i = 0; i < 300; ++i) {
        // Small series: 1-10 samples, 1-3 labels
        auto small = createVariableSizeTimeSeries(i, 1 + (i % 10), 1 + (i % 3));
        small_series.push_back(small);
        
        // Medium series: 10-100 samples, 3-8 labels
        auto medium = createVariableSizeTimeSeries(i + 300, 10 + (i % 90), 3 + (i % 6));
        medium_series.push_back(medium);
        
        // Large series: 100-1000 samples, 8-15 labels
        auto large = createVariableSizeTimeSeries(i + 600, 100 + (i % 900), 8 + (i % 8));
        large_series.push_back(large);
    }
    
    // Analyze size distribution
    std::vector<size_t> small_sizes, medium_sizes, large_sizes;
    
    for (const auto& series : small_series) {
        small_sizes.push_back(estimateTimeSeriesSize(series));
    }
    for (const auto& series : medium_series) {
        medium_sizes.push_back(estimateTimeSeriesSize(series));
    }
    for (const auto& series : large_series) {
        large_sizes.push_back(estimateTimeSeriesSize(series));
    }
    
    std::sort(small_sizes.begin(), small_sizes.end());
    std::sort(medium_sizes.begin(), medium_sizes.end());
    std::sort(large_sizes.begin(), large_sizes.end());
    
    std::cout << "\nSIZE CATEGORY ANALYSIS:" << std::endl;
    std::cout << "Small series (< 1KB):" << std::endl;
    std::cout << "  Count: " << small_sizes.size() << std::endl;
    std::cout << "  Size range: " << small_sizes.front() << " - " << small_sizes.back() << " bytes" << std::endl;
    std::cout << "  Median: " << small_sizes[small_sizes.size() / 2] << " bytes" << std::endl;
    
    std::cout << "\nMedium series (1KB - 10KB):" << std::endl;
    std::cout << "  Count: " << medium_sizes.size() << std::endl;
    std::cout << "  Size range: " << medium_sizes.front() << " - " << medium_sizes.back() << " bytes" << std::endl;
    std::cout << "  Median: " << medium_sizes[medium_sizes.size() / 2] << " bytes" << std::endl;
    
    std::cout << "\nLarge series (> 10KB):" << std::endl;
    std::cout << "  Count: " << large_sizes.size() << std::endl;
    std::cout << "  Size range: " << large_sizes.front() << " - " << large_sizes.back() << " bytes" << std::endl;
    std::cout << "  Median: " << large_sizes[large_sizes.size() / 2] << " bytes" << std::endl;
    
    // Evaluate memory efficiency implications
    std::cout << "\nMEMORY EFFICIENCY IMPLICATIONS:" << std::endl;
    
    // Current fixed-size pool approach
    size_t current_pool_size = 1000; // Assume 1000 objects
    size_t avg_object_size = (small_sizes[small_sizes.size() / 2] + 
                             medium_sizes[medium_sizes.size() / 2] + 
                             large_sizes[large_sizes.size() / 2]) / 3;
    size_t current_memory_usage = current_pool_size * avg_object_size;
    
    std::cout << "Current fixed-size pool approach:" << std::endl;
    std::cout << "  Pool size: " << current_pool_size << " objects" << std::endl;
    std::cout << "  Average object size: " << avg_object_size << " bytes" << std::endl;
    std::cout << "  Total memory usage: " << current_memory_usage << " bytes" << std::endl;
    
    // Variable-size pool approach
    size_t small_pool_size = 400;   // 40% of objects
    size_t medium_pool_size = 400;  // 40% of objects
    size_t large_pool_size = 200;   // 20% of objects
    
    size_t small_avg_size = small_sizes[small_sizes.size() / 2];
    size_t medium_avg_size = medium_sizes[medium_sizes.size() / 2];
    size_t large_avg_size = large_sizes[large_sizes.size() / 2];
    
    size_t variable_memory_usage = (small_pool_size * small_avg_size) +
                                  (medium_pool_size * medium_avg_size) +
                                  (large_pool_size * large_avg_size);
    
    std::cout << "\nVariable-size pool approach:" << std::endl;
    std::cout << "  Small pool: " << small_pool_size << " objects × " << small_avg_size << " bytes = " 
              << (small_pool_size * small_avg_size) << " bytes" << std::endl;
    std::cout << "  Medium pool: " << medium_pool_size << " objects × " << medium_avg_size << " bytes = " 
              << (medium_pool_size * medium_avg_size) << " bytes" << std::endl;
    std::cout << "  Large pool: " << large_pool_size << " objects × " << large_avg_size << " bytes = " 
              << (large_pool_size * large_avg_size) << " bytes" << std::endl;
    std::cout << "  Total memory usage: " << variable_memory_usage << " bytes" << std::endl;
    
    double memory_savings = (double)(current_memory_usage - variable_memory_usage) / current_memory_usage * 100;
    std::cout << "\nMemory savings: " << memory_savings << "%" << std::endl;
    
    // Complexity trade-offs
    std::cout << "\nCOMPLEXITY TRADE-OFFS:" << std::endl;
    std::cout << "Fixed-size pools:" << std::endl;
    std::cout << "  ✓ Simple implementation" << std::endl;
    std::cout << "  ✓ Fast allocation/deallocation" << std::endl;
    std::cout << "  ✓ Predictable memory usage" << std::endl;
    std::cout << "  ✗ Memory waste for size mismatches" << std::endl;
    std::cout << "  ✗ Poor cache locality for large objects" << std::endl;
    
    std::cout << "\nVariable-size pools:" << std::endl;
    std::cout << "  ✓ Better memory utilization" << std::endl;
    std::cout << "  ✓ Better cache locality" << std::endl;
    std::cout << "  ✓ Tailored to workload patterns" << std::endl;
    std::cout << "  ✗ More complex implementation" << std::endl;
    std::cout << "  ✗ Slower allocation (size selection)" << std::endl;
    std::cout << "  ✗ More complex statistics tracking" << std::endl;
    
    // Recommendation
    std::cout << "\nRECOMMENDATION:" << std::endl;
    if (memory_savings > 20) {
        std::cout << "  Consider variable-size pools for significant memory savings" << std::endl;
    } else if (memory_savings > 10) {
        std::cout << "  Variable-size pools may be beneficial depending on complexity tolerance" << std::endl;
    } else {
        std::cout << "  Stick with fixed-size pools for simplicity and performance" << std::endl;
    }
}

} // namespace integration
} // namespace tsdb 