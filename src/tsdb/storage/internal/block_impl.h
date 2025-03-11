#ifndef TSDB_STORAGE_BLOCK_IMPL_H_
#define TSDB_STORAGE_BLOCK_IMPL_H_

#include <map>
#include <mutex>
#include <fstream>
#include "block_internal.h"
#include "block_format.h"
#include "compression.h"

namespace tsdb {
namespace storage {

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
    size_t num_samples() const override;
    core::TimeSeries read(const core::Labels& labels) const override;
    std::vector<core::TimeSeries> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) const override;
    
    // BlockInternal interface
    void write(const core::TimeSeries& series) override;
    void flush() override;
    void close() override;
    
private:
    struct SeriesData {
        std::vector<uint8_t> timestamps;
        std::vector<uint8_t> values;
        size_t num_samples;
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
    BlockReaderImpl(std::unique_ptr<CompressorFactory> compressor_factory);
    
    std::unique_ptr<BlockInternal> read(const std::string& path) override;
    
private:
    std::unique_ptr<CompressorFactory> compressor_factory_;
};

/**
 * @brief Implementation of block writer
 */
class BlockWriterImpl : public BlockWriter {
public:
    BlockWriterImpl(std::unique_ptr<CompressorFactory> compressor_factory);
    
    void write(const std::string& path, const BlockInternal& block) override;
    
private:
    std::unique_ptr<CompressorFactory> compressor_factory_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_BLOCK_IMPL_H_ 