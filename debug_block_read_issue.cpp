#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <limits>

// Simulate the core types
struct Sample {
    int64_t timestamp_;
    double value_;
    
    Sample(int64_t ts, double val) : timestamp_(ts), value_(val) {}
    int64_t timestamp() const { return timestamp_; }
    double value() const { return value_; }
};

class Labels {
private:
    std::map<std::string, std::string> labels_;
    
public:
    void add(const std::string& name, const std::string& value) {
        labels_[name] = value;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& [name, value] : labels_) {
            if (!first) {
                oss << ", ";
            }
            oss << name << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }
    
    bool operator==(const Labels& other) const {
        return labels_ == other.labels_;
    }
    
    bool operator<(const Labels& other) const {
        return labels_ < other.labels_;
    }
};

class TimeSeries {
private:
    Labels labels_;
    std::vector<Sample> samples_;
    
public:
    TimeSeries(const Labels& labels) : labels_(labels) {}
    
    void add_sample(const Sample& sample) {
        samples_.push_back(sample);
    }
    
    const Labels& labels() const { return labels_; }
    const std::vector<Sample>& samples() const { return samples_; }
    bool empty() const { return samples_.empty(); }
};

// Simulate BlockImpl
class BlockImpl {
private:
    int64_t start_time_;
    int64_t end_time_;
    std::map<Labels, std::vector<Sample>> series_data_;
    
public:
    BlockImpl() : start_time_(std::numeric_limits<int64_t>::max()), 
                  end_time_(std::numeric_limits<int64_t>::min()) {}
    
    void write(const TimeSeries& series) {
        series_data_[series.labels()] = series.samples();
        
        // Update time range
        for (const auto& sample : series.samples()) {
            start_time_ = std::min(start_time_, sample.timestamp());
            end_time_ = std::max(end_time_, sample.timestamp());
        }
    }
    
    TimeSeries read(const Labels& labels) const {
        auto it = series_data_.find(labels);
        if (it == series_data_.end()) {
            return TimeSeries(labels); // Empty series if not found
        }
        
        TimeSeries result(labels);
        for (const auto& sample : it->second) {
            result.add_sample(sample);
        }
        return result;
    }
    
    int64_t start_time() const { return start_time_; }
    int64_t end_time() const { return end_time_; }
};

// Simulate the problematic read_from_blocks logic
class StorageImpl {
private:
    std::map<std::size_t, std::vector<std::shared_ptr<BlockImpl>>> series_blocks_;
    
    std::size_t calculate_series_id(const Labels& labels) const {
        std::hash<std::string> hasher;
        return hasher(labels.to_string());
    }
    
public:
    void write_series(const TimeSeries& series, std::shared_ptr<BlockImpl> block) {
        auto series_id = calculate_series_id(series.labels());
        series_blocks_[series_id].push_back(block);
        
        std::cout << "WRITE: Series ID = " << series_id << std::endl;
        std::cout << "WRITE: Labels = " << series.labels().to_string() << std::endl;
        std::cout << "WRITE: Block time range = [" << block->start_time() << ", " << block->end_time() << "]" << std::endl;
        std::cout << "WRITE: Series samples = " << series.samples().size() << std::endl;
    }
    
    TimeSeries read_from_blocks(const Labels& labels, int64_t start_time, int64_t end_time) {
        auto series_id = calculate_series_id(labels);
        TimeSeries result(labels);
        
        std::cout << "\nREAD: Series ID = " << series_id << std::endl;
        std::cout << "READ: Labels = " << labels.to_string() << std::endl;
        std::cout << "READ: Time range = [" << start_time << ", " << end_time << "]" << std::endl;
        
        // Find blocks for this series
        auto it = series_blocks_.find(series_id);
        if (it == series_blocks_.end()) {
            std::cout << "READ: No blocks found for series ID " << series_id << std::endl;
            std::cout << "READ: Available series IDs: ";
            for (const auto& [id, blocks] : series_blocks_) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
            return result;
        }
        
        std::cout << "READ: Found " << it->second.size() << " blocks for series" << std::endl;
        
        // Read from all blocks for this series
        for (size_t i = 0; i < it->second.size(); ++i) {
            const auto& block = it->second[i];
            if (!block) continue;
            
            std::cout << "READ: Block " << i << " time range = [" << block->start_time() << ", " << block->end_time() << "]" << std::endl;
            
            // Check if block overlaps with time range
            if (block->end_time() < start_time || block->start_time() > end_time) {
                std::cout << "READ: Block " << i << " does not overlap with time range - SKIPPING" << std::endl;
                continue;  // Block doesn't overlap with requested range
            }
            
            std::cout << "READ: Block " << i << " overlaps with time range - READING" << std::endl;
            
            // Read series from block
            auto block_series = block->read(labels);
            std::cout << "READ: Block " << i << " returned " << block_series.samples().size() << " samples" << std::endl;
            
            // Add samples within time range
            for (const auto& sample : block_series.samples()) {
                if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                    result.add_sample(sample);
                    std::cout << "READ: Added sample at " << sample.timestamp() << " with value " << sample.value() << std::endl;
                } else {
                    std::cout << "READ: Skipped sample at " << sample.timestamp() << " (outside range)" << std::endl;
                }
            }
        }
        
        std::cout << "READ: Final result has " << result.samples().size() << " samples" << std::endl;
        return result;
    }
};

int main() {
    std::cout << "🔍 DEBUGGING BLOCK READ ISSUE\n";
    std::cout << "=============================\n\n";
    
    // Create the test scenario from the failing test
    Labels test_labels;
    test_labels.add("__name__", "boundary_large");
    test_labels.add("test", "phase1");
    test_labels.add("pool_test", "true");
    test_labels.add("size", "large");
    
    // Create test series with 100 samples (timestamps 1000-1099)
    TimeSeries test_series(test_labels);
    for (int i = 0; i < 100; ++i) {
        test_series.add_sample(Sample(1000 + i, 100.0 + i * 0.1));
    }
    
    std::cout << "Created test series with " << test_series.samples().size() << " samples" << std::endl;
    std::cout << "Sample time range: [" << test_series.samples().front().timestamp() 
              << ", " << test_series.samples().back().timestamp() << "]" << std::endl;
    
    // Create block and write series
    auto block = std::make_shared<BlockImpl>();
    block->write(test_series);
    
    // Create storage and register the series
    StorageImpl storage;
    storage.write_series(test_series, block);
    
    // Try to read with the same time range as the test
    std::cout << "\n=== ATTEMPTING READ ===\n";
    auto read_result = storage.read_from_blocks(test_labels, 0, std::numeric_limits<int64_t>::max());
    
    std::cout << "\n=== FINAL RESULT ===\n";
    std::cout << "Read result has " << read_result.samples().size() << " samples" << std::endl;
    
    if (read_result.samples().size() == 0) {
        std::cout << "❌ ISSUE CONFIRMED: 0 samples returned instead of 100!" << std::endl;
    } else {
        std::cout << "✅ SUCCESS: " << read_result.samples().size() << " samples returned" << std::endl;
    }
    
    return 0;
}
