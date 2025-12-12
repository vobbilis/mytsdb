#include "tsdb/storage/parquet/fingerprint.h"

#include <array>
#include <atomic>
#include <functional>

namespace tsdb {
namespace storage {
namespace parquet {
namespace {

constexpr uint32_t kCrc32Poly = 0xEDB88320u;

std::array<uint32_t, 256> MakeCrc32Table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (kCrc32Poly ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

uint32_t Crc32Bytes(const uint8_t* data, size_t len) {
    static const std::array<uint32_t, 256> table = MakeCrc32Table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

core::SeriesID DefaultHasher(const std::string& s) {
    return std::hash<std::string>{}(s);
}

std::atomic<SeriesIdHasherFn> g_hasher{&DefaultHasher};

} // namespace

uint32_t LabelsCrc32(const std::string& canonical_labels_str) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(canonical_labels_str.data());
    return Crc32Bytes(bytes, canonical_labels_str.size());
}

core::SeriesID SeriesIdFromLabelsString(const std::string& canonical_labels_str) {
    auto fn = g_hasher.load(std::memory_order_relaxed);
    return fn ? fn(canonical_labels_str) : DefaultHasher(canonical_labels_str);
}

void SetSeriesIdHasherForTests(SeriesIdHasherFn fn) {
    g_hasher.store(fn ? fn : &DefaultHasher, std::memory_order_relaxed);
}

void ResetSeriesIdHasherForTests() {
    g_hasher.store(&DefaultHasher, std::memory_order_relaxed);
}

} // namespace parquet
} // namespace storage
} // namespace tsdb


