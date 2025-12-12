#pragma once

#include <cstdint>
#include <string>

#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace parquet {

// Stable CRC32 (IEEE) of the canonical labels string (e.g. "k=v,k=v").
uint32_t LabelsCrc32(const std::string& canonical_labels_str);

// SeriesID derived from canonical labels string. Default is std::hash<string>.
core::SeriesID SeriesIdFromLabelsString(const std::string& canonical_labels_str);

// ---- Test seam ----
// Allows tests to force deterministic SeriesID collisions to validate
// collision-defense logic. Not intended for production use.
using SeriesIdHasherFn = core::SeriesID(*)(const std::string&);
void SetSeriesIdHasherForTests(SeriesIdHasherFn fn);
void ResetSeriesIdHasherForTests();

} // namespace parquet
} // namespace storage
} // namespace tsdb


