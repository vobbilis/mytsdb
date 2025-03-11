package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/vobbilis/go-tsdb/test/testutil"
)

func TestStorageBasicOperations(t *testing.T) {
	storage, err := NewStorage(StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer storage.Close()

	// Test data
	series := testutil.GenerateTestSeries(map[string]string{
		"__name__": "test_metric",
		"instance": "test-1",
	}, 100)

	// Test Write
	t.Run("Write", func(t *testing.T) {
		err := storage.WriteSeries(series)
		assert.NoError(t, err)
	})

	// Test Read
	t.Run("Read", func(t *testing.T) {
		end := time.Now()
		start := end.Add(-1 * time.Hour)

		result, err := storage.ReadSeries(series.Labels, start.UnixNano(), end.UnixNano())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.Equal(t, len(series.Samples), len(result.Samples))
	})

	// Test Query
	t.Run("Query", func(t *testing.T) {
		matcher := LabelMatcher{
			Type:  MatchEqual,
			Name:  "__name__",
			Value: "test_metric",
		}

		end := time.Now()
		start := end.Add(-1 * time.Hour)

		results, err := storage.Query([]LabelMatcher{matcher}, start.UnixNano(), end.UnixNano())
		assert.NoError(t, err)
		assert.NotEmpty(t, results)
	})
}

func TestStorageConcurrency(t *testing.T) {
	storage, err := NewStorage(StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer storage.Close()

	// Generate test data
	testData := testutil.GenerateTestData(10, 1000)

	// Test concurrent writes
	t.Run("ConcurrentWrites", func(t *testing.T) {
		done := make(chan error, len(testData))

		for _, series := range testData {
			go func(s *types.Series) {
				done <- storage.WriteSeries(s)
			}(series)
		}

		// Collect results
		for i := 0; i < len(testData); i++ {
			err := <-done
			assert.NoError(t, err)
		}
	})

	// Test concurrent reads
	t.Run("ConcurrentReads", func(t *testing.T) {
		end := time.Now()
		start := end.Add(-1 * time.Hour)

		done := make(chan error, len(testData))

		for _, series := range testData {
			go func(s *types.Series) {
				_, err := storage.ReadSeries(s.Labels, start.UnixNano(), end.UnixNano())
				done <- err
			}(series)
		}

		// Collect results
		for i := 0; i < len(testData); i++ {
			err := <-done
			assert.NoError(t, err)
		}
	})
}

func TestStorageCompaction(t *testing.T) {
	storage, err := NewStorage(StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024, // Small block size to trigger compaction
	})
	require.NoError(t, err)
	defer storage.Close()

	// Write enough data to trigger compaction
	series := testutil.GenerateTestSeries(map[string]string{
		"__name__": "test_metric",
		"instance": "test-1",
	}, 10000)

	t.Run("WriteAndCompact", func(t *testing.T) {
		err := storage.WriteSeries(series)
		assert.NoError(t, err)

		// Wait for compaction
		time.Sleep(2 * time.Second)

		// Verify data is still readable
		end := time.Now()
		start := end.Add(-1 * time.Hour)

		result, err := storage.ReadSeries(series.Labels, start.UnixNano(), end.UnixNano())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.Equal(t, len(series.Samples), len(result.Samples))
	})
}

func TestStorageRecovery(t *testing.T) {
	dir := t.TempDir()

	// Create and populate storage
	storage, err := NewStorage(StorageConfig{
		DataPath:     dir,
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)

	series := testutil.GenerateTestSeries(map[string]string{
		"__name__": "test_metric",
		"instance": "test-1",
	}, 100)

	err = storage.WriteSeries(series)
	require.NoError(t, err)

	// Close storage
	storage.Close()

	// Reopen storage
	storage2, err := NewStorage(StorageConfig{
		DataPath:     dir,
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer storage2.Close()

	// Verify data survived
	t.Run("RecoveredData", func(t *testing.T) {
		end := time.Now()
		start := end.Add(-1 * time.Hour)

		result, err := storage2.ReadSeries(series.Labels, start.UnixNano(), end.UnixNano())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.Equal(t, len(series.Samples), len(result.Samples))
	})
}
