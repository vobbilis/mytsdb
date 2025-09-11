#pragma once

#define TSDB_TEST_DATA_DIR "/Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test/data"

// Test data paths
#define TSDB_TEST_BLOCKS_DIR TSDB_TEST_DATA_DIR "/storage/blocks"
#define TSDB_TEST_HISTOGRAMS_DIR TSDB_TEST_DATA_DIR "/storage/histograms"
#define TSDB_TEST_COMPRESSION_DIR TSDB_TEST_DATA_DIR "/storage/compression"

// Test configuration
#define ENABLE_SIMD
#define COMPILER_SUPPORTS_AVX512 
