#ifndef TSDB_STORAGE_INTERNAL_BLOCK_IMPL_H_
#define TSDB_STORAGE_INTERNAL_BLOCK_IMPL_H_

#include <map>
#include <mutex>
#include <fstream>
#include "block_internal.h"
#include "tsdb/storage/compression.h"

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Implementation of block
 */
class BlockImpl : public BlockInternal {
public:
    BlockImpl(const BlockHeader& header,
             std::unique_ptr<TimestampCompressor> ts_compressor,
             std::unique_ptr<ValueCompressor> val_compressor,
             std::unique_ptr<LabelCompressor> label_compressor);
    
    // Block interface
    size_t size() const override;
    size_t num_series() const override;
    size_t num_samples() const override {
        size_t total = 0;
        for (const auto& [_, data] : series_) {
            total += data.timestamps.size();
        }
        return total;
    }
    int64_t start_time() const override { return header_.start_time; }
    int64_t end_time() const override { return header_.end_time; }
    core::TimeSeries read(const core::Labels& labels) const override;
    std::vector<core::TimeSeries> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) const override;
    
    // BlockInternal interface
    void write(const core::TimeSeries& series) override;
    const BlockHeader& header() const override;
    void flush() override;
    void close() override;
    
private:
    struct SeriesData {
        std::vector<uint8_t> timestamps;
        std::vector<uint8_t> values;
    };
    
    BlockHeader header_;
    std::map<core::Labels, SeriesData> series_;
    std::unique_ptr<TimestampCompressor> ts_compressor_;
    std::unique_ptr<ValueCompressor> val_compressor_;
    std::unique_ptr<LabelCompressor> label_compressor_;
    mutable std::mutex mutex_;
    bool dirty_;
    
    // Helper functions
    void update_header();
    void write_header(std::ofstream& out) const;
    void write_series(std::ofstream& out) const;
    void read_header(std::ifstream& in);
    void read_series(std::ifstream& in);
    uint32_t calculate_crc() const;
};

/**
 * @brief Implementation of block reader
 */
class BlockReaderImpl : public BlockReader {
public:
    BlockReaderImpl();
    
    std::unique_ptr<BlockInternal> read(const std::string& path) override;
    
private:
    // No CompressorFactory member needed, compressors will be created in read() using factory functions
};

/**
 * @brief Implementation of block writer
 */
class BlockWriterImpl : public BlockWriter {
public:
    BlockWriterImpl();
    
    void write(const std::string& path, const BlockInternal& block) override;
    
private:
    // No CompressorFactory member needed, uses compressors from the provided block
};

} // namespace internal
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_INTERNAL_BLOCK_IMPL_H_ 