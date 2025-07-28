#include "tsdb/storage/predictive_cache.h"
#include "tsdb/core/error.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <chrono>

namespace tsdb {
namespace storage {

// PrefetchStats implementation
double PrefetchStats::get_success_rate() const {
    size_t total = total_prefetches.load();
    if (total == 0) return 0.0;
    
    size_t successful = successful_prefetches.load();
    return static_cast<double>(successful) / static_cast<double>(total);
}

void PrefetchStats::record_result(bool success) {
    total_prefetches.fetch_add(1);
    if (success) {
        successful_prefetches.fetch_add(1);
    } else {
        failed_prefetches.fetch_add(1);
    }
    
    std::lock_guard<std::mutex> lock(results_mutex);
    recent_results.push_back(success);
    cleanup_old_results(100); // Keep last 100 results
}

void PrefetchStats::cleanup_old_results(size_t window_size) {
    while (recent_results.size() > window_size) {
        recent_results.pop_front();
    }
}

// PredictiveCache implementation
PredictiveCache::PredictiveCache(const PredictiveCacheConfig& config)
    : config_(config) {
    
    if (config_.enable_background_cleanup) {
        cleanup_thread_ = std::make_unique<std::thread>(&PredictiveCache::background_cleanup_worker, this);
    }
}

PredictiveCache::~PredictiveCache() {
    shutdown_flag_.store(true);
    if (cleanup_thread_ && cleanup_thread_->joinable()) {
        cleanup_thread_->join();
    }
}

void PredictiveCache::record_access(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    // Add to global access sequence
    global_access_sequence_.push_back(series_id);
    
    // Limit global sequence length
    if (global_access_sequence_.size() > config_.max_pattern_length * 10) {
        global_access_sequence_.pop_front();
    }
    
    // Detect patterns from the global sequence
    detect_patterns();
    
    // Cleanup if we have too many tracked series
    if (access_sequences_.size() > config_.max_tracked_series) {
        // Remove oldest entries
        auto oldest = access_sequences_.begin();
        for (auto it = access_sequences_.begin(); it != access_sequences_.end(); ++it) {
            if (it->second.empty() || (oldest->second.empty() && !it->second.empty())) {
                oldest = it;
            }
        }
        if (!oldest->second.empty()) {
            access_sequences_.erase(oldest);
        }
    }
}

void PredictiveCache::detect_patterns() {
    const auto& sequence = global_access_sequence_;
    
    // Look for patterns of different lengths
    for (size_t pattern_length = 2; pattern_length <= std::min(sequence.size(), config_.max_pattern_length); ++pattern_length) {
        for (size_t start = 0; start <= sequence.size() - pattern_length; ++start) {
            std::vector<core::SeriesID> pattern(sequence.begin() + start, sequence.begin() + start + pattern_length);
            std::string pattern_key = pattern_to_string(pattern);
            
            auto it = detected_patterns_.find(pattern_key);
            if (it != detected_patterns_.end()) {
                it->second.occurrences++;
                it->second.last_seen = std::chrono::steady_clock::now();
                it->second.confidence = calculate_confidence(it->second);
            } else {
                detected_patterns_[pattern_key] = AccessPattern(pattern, 1);
                detected_patterns_[pattern_key].confidence = calculate_confidence(detected_patterns_[pattern_key]);
            }
        }
    }
}

std::string PredictiveCache::pattern_to_string(const std::vector<core::SeriesID>& pattern) const {
    std::ostringstream oss;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (i > 0) oss << ",";
        oss << pattern[i];
    }
    return oss.str();
}

std::vector<core::SeriesID> PredictiveCache::string_to_pattern(const std::string& pattern_str) const {
    std::vector<core::SeriesID> pattern;
    std::istringstream iss(pattern_str);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        try {
            pattern.push_back(std::stoull(token));
        } catch (const std::exception&) {
            // Skip invalid tokens
        }
    }
    
    return pattern;
}

double PredictiveCache::calculate_confidence(const AccessPattern& pattern) const {
    if (pattern.occurrences < config_.min_pattern_confidence) {
        return 0.0;
    }
    
    // Simple confidence calculation based on frequency
    // Could be enhanced with more sophisticated algorithms
    double base_confidence = std::min(1.0, static_cast<double>(pattern.occurrences) / 5.0);
    
    // Decay confidence based on time since last seen
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::hours>(now - pattern.last_seen);
    double time_decay = std::exp(-time_since_last.count() / 24.0); // Decay over 24 hours
    
    return base_confidence * time_decay;
}

std::vector<std::pair<core::SeriesID, double>> PredictiveCache::get_predictions(core::SeriesID current_series) const {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    std::vector<std::pair<core::SeriesID, double>> predictions;
    auto matching_patterns = find_matching_patterns(current_series);
    
    for (const auto& [pattern, confidence] : matching_patterns) {
        if (confidence >= config_.confidence_threshold && pattern.sequence.size() > 1) {
            // Predict the next item in the sequence
            core::SeriesID predicted_series = pattern.sequence[1]; // Next item after current_series
            predictions.emplace_back(predicted_series, confidence);
        }
    }
    
    // Sort by confidence (highest first)
    std::sort(predictions.begin(), predictions.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit to max prefetch size
    if (predictions.size() > config_.max_prefetch_size) {
        predictions.resize(config_.max_prefetch_size);
    }
    
    return predictions;
}

std::vector<std::pair<AccessPattern, double>> PredictiveCache::find_matching_patterns(core::SeriesID series_id) const {
    std::vector<std::pair<AccessPattern, double>> matching_patterns;
    
    for (const auto& [pattern_key, pattern] : detected_patterns_) {
        if (!pattern.sequence.empty() && pattern.sequence[0] == series_id) {
            double confidence = calculate_confidence(pattern);
            if (confidence >= config_.confidence_threshold) {
                matching_patterns.emplace_back(pattern, confidence);
            }
        }
    }
    
    // Sort by confidence (highest first)
    std::sort(matching_patterns.begin(), matching_patterns.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return matching_patterns;
}

size_t PredictiveCache::prefetch_predictions(CacheHierarchy& cache_hierarchy, core::SeriesID current_series) {
    if (!config_.integrate_with_cache_hierarchy) {
        return 0;
    }
    
    auto predictions = get_predictions(current_series);
    size_t prefetched_count = 0;
    
    for (const auto& [series_id, confidence] : predictions) {
        // Check if the series is already in cache
        auto existing = cache_hierarchy.get(series_id);
        if (existing) {
            // Already cached, record as successful prefetch
            prefetch_stats_.record_result(true);
            continue;
        }
        
        // For now, we'll just record the prediction
        // In a real implementation, we would prefetch the actual data
        // This would involve calling the storage layer to load the series
        prefetched_count++;
        
        // Record as prefetch attempt (we'll update success/failure later)
        prefetch_stats_.record_result(false); // Will be updated when actual access occurs
    }
    
    return prefetched_count;
}

void PredictiveCache::record_prefetch_result(core::SeriesID series_id, bool was_accessed) {
    // This method would be called when a prefetched item is actually accessed
    // For now, we just record the result
    prefetch_stats_.record_result(was_accessed);
}

size_t PredictiveCache::get_adaptive_prefetch_size() const {
    if (!config_.enable_adaptive_prefetch) {
        return config_.max_prefetch_size;
    }
    
    double success_rate = prefetch_stats_.get_success_rate();
    
    // Adjust prefetch size based on success rate
    if (success_rate > 0.8) {
        return std::min(config_.max_prefetch_size + 2, config_.max_prefetch_size);
    } else if (success_rate > 0.6) {
        return config_.max_prefetch_size;
    } else if (success_rate > 0.4) {
        return std::max(1ul, config_.max_prefetch_size - 1);
    } else {
        return std::max(1ul, config_.max_prefetch_size - 2);
    }
}

std::string PredictiveCache::get_stats() const {
    std::ostringstream oss;
    
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    oss << "Predictive Cache Statistics:\n";
    oss << "  Global Access Sequence Length: " << global_access_sequence_.size() << "\n";
    oss << "  Detected Patterns: " << detected_patterns_.size() << "\n";
    oss << "  Total Prefetches: " << prefetch_stats_.total_prefetches.load() << "\n";
    oss << "  Successful Prefetches: " << prefetch_stats_.successful_prefetches.load() << "\n";
    oss << "  Failed Prefetches: " << prefetch_stats_.failed_prefetches.load() << "\n";
    oss << "  Success Rate: " << std::fixed << std::setprecision(2) 
        << (prefetch_stats_.get_success_rate() * 100.0) << "%\n";
    oss << "  Adaptive Prefetch Size: " << get_adaptive_prefetch_size() << "\n";
    
    // Show top patterns
    std::vector<std::pair<std::string, AccessPattern>> sorted_patterns;
    for (const auto& [key, pattern] : detected_patterns_) {
        sorted_patterns.emplace_back(key, pattern);
    }
    
    std::sort(sorted_patterns.begin(), sorted_patterns.end(),
              [](const auto& a, const auto& b) { 
                  return a.second.confidence > b.second.confidence; 
              });
    
    oss << "  Top Patterns:\n";
    for (size_t i = 0; i < std::min(5ul, sorted_patterns.size()); ++i) {
        const auto& [key, pattern] = sorted_patterns[i];
        oss << "    " << (i + 1) << ". " << key 
            << " (confidence: " << std::fixed << std::setprecision(2) << pattern.confidence
            << ", occurrences: " << pattern.occurrences << ")\n";
    }
    
    return oss.str();
}

void PredictiveCache::clear() {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    access_sequences_.clear();
    global_access_sequence_.clear();
    detected_patterns_.clear();
    
    // Reset prefetch stats
    prefetch_stats_.total_prefetches.store(0);
    prefetch_stats_.successful_prefetches.store(0);
    prefetch_stats_.failed_prefetches.store(0);
    
    std::lock_guard<std::mutex> results_lock(prefetch_stats_.results_mutex);
    prefetch_stats_.recent_results.clear();
}

void PredictiveCache::update_config(const PredictiveCacheConfig& new_config) {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    config_ = new_config;
}

void PredictiveCache::cleanup_old_patterns() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - std::chrono::hours(24); // Remove patterns older than 24 hours
    
    std::vector<std::string> patterns_to_remove;
    
    for (const auto& [key, pattern] : detected_patterns_) {
        if (pattern.last_seen < cutoff_time && pattern.occurrences < config_.min_pattern_confidence) {
            patterns_to_remove.push_back(key);
        }
    }
    
    for (const auto& key : patterns_to_remove) {
        detected_patterns_.erase(key);
    }
}

void PredictiveCache::background_cleanup_worker() {
    while (!shutdown_flag_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.cleanup_interval_ms));
        
        if (shutdown_flag_.load()) break;
        
        cleanup_old_patterns();
    }
}

} // namespace storage
} // namespace tsdb 