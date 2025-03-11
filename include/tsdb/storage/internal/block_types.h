#ifndef TSDB_STORAGE_INTERNAL_BLOCK_TYPES_H_
#define TSDB_STORAGE_INTERNAL_BLOCK_TYPES_H_

#include <cstdint>
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Block header structure
 */
struct BlockHeader {
    static constexpr uint64_t MAGIC = 0x4253445354534254;  // "TBSTDSB"
    static constexpr uint32_t VERSION = 1;
    
    uint64_t magic;      // Magic number for validation
    uint32_t version;    // Block format version
    uint32_t flags;      // Block flags (compression, etc.)
    uint32_t crc32;      // CRC32 of block data
    int64_t start_time;  // Block start time
    int64_t end_time;    // Block end time
    uint32_t reserved;   // Reserved for future use
    
    bool is_valid() const {
        return magic == MAGIC && version == VERSION;
    }
};

/**
 * @brief Block flags defining block properties and features
 * 
 * Flags that indicate various properties of the block:
 * - NONE: No special properties
 * - COMPRESSED: Block data is compressed
 * - SORTED: Series data within the block is sorted by timestamp
 * - CHECKSUM: Block has checksum verification enabled
 */
enum class BlockFlags : uint32_t {
    NONE = 0,
    COMPRESSED = 1 << 0,
    SORTED = 1 << 1,
    CHECKSUM = 1 << 2
};

/**
 * @brief Block format version 1 layout
 * 
 * Block Layout:
 * +----------------+----------------+----------------+
 * | Block Header   | Series Data    | Index         |
 * +----------------+----------------+----------------+
 * 
 * Series Data Layout:
 * +----------------+----------------+----------------+
 * | Series Count   | Series 1      | Series 2      |
 * +----------------+----------------+----------------+
 * 
 * Series Layout:
 * +----------------+----------------+----------------+
 * | Labels         | Timestamps     | Values        |
 * +----------------+----------------+----------------+
 */
struct BlockFormatV1 {
    static constexpr size_t HEADER_SIZE = sizeof(BlockHeader);
    static constexpr size_t SERIES_COUNT_SIZE = sizeof(uint32_t);
    static constexpr size_t LABEL_COUNT_SIZE = sizeof(uint32_t);
    static constexpr size_t SAMPLE_COUNT_SIZE = sizeof(uint32_t);
    
    struct SeriesOffset {
        uint64_t labels_offset;
        uint64_t timestamps_offset;
        uint64_t values_offset;
    };
};

/**
 * @brief Histogram block format layout
 * 
 * Histogram Block Layout:
 * +----------------+----------------+----------------+
 * | Block Header   | Histogram Data | Index         |
 * +----------------+----------------+----------------+
 * 
 * Histogram Data Layout:
 * +----------------+----------------+----------------+
 * | Histogram Type | Bucket Data    | Statistics    |
 * +----------------+----------------+----------------+
 */
struct HistogramBlockFormat {
    enum class Type : uint32_t {
        FIXED,          // Fixed-width buckets
        EXPONENTIAL,    // Exponential buckets (OpenTelemetry compatible)
        DDSKETCH       // DDSketch-based adaptive histograms
    };
    
    struct Statistics {
        double min;
        double max;
        double sum;
        uint64_t count;
    };
    
    struct BucketData {
        uint32_t bucket_count;
        uint32_t flags;
        double base;        // For exponential histograms
        double scale;      // For DDSketch
    };
    
    static constexpr size_t STATS_SIZE = sizeof(Statistics);
    static constexpr size_t BUCKET_HEADER_SIZE = sizeof(BucketData);
};

/**
 * @brief Storage tier-specific block properties
 */
struct BlockTier {
    enum class Type : uint8_t {
        HOT,    // Recent, uncompressed data
        WARM,   // Compressed, frequently accessed data
        COLD    // Highly compressed, archived data
    };
    
    struct Properties {
        Type tier_type;
        uint32_t compression_level;
        uint32_t retention_days;
        bool allow_mmap;
    };
};

}  // namespace internal
}  // namespace storage
}  // namespace tsdb

#endif  // TSDB_STORAGE_INTERNAL_BLOCK_TYPES_H_ 