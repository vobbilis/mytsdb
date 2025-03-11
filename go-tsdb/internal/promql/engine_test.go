package promql

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/vobbilis/go-tsdb/internal/storage"
	"github.com/vobbilis/go-tsdb/test/testutil"
)

func TestPromQLBasicQueries(t *testing.T) {
	// Setup storage
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	// Setup test data
	series := []*storage.Series{
		testutil.GenerateTestSeries(map[string]string{
			"__name__": "http_requests_total",
			"method":   "GET",
			"status":   "200",
		}, 100),
		testutil.GenerateTestSeries(map[string]string{
			"__name__": "http_requests_total",
			"method":   "POST",
			"status":   "200",
		}, 100),
		testutil.GenerateTestSeries(map[string]string{
			"__name__": "http_requests_total",
			"method":   "GET",
			"status":   "500",
		}, 100),
	}

	// Write test data
	for _, s := range series {
		err := store.WriteSeries(s)
		require.NoError(t, err)
	}

	// Create PromQL engine
	engine := NewEngine(store)

	// Test instant query
	t.Run("InstantQuery", func(t *testing.T) {
		query := `http_requests_total{method="GET"}`
		result, err := engine.InstantQuery(query, time.Now())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.Len(t, result.Series, 2) // GET/200 and GET/500
	})

	// Test range query
	t.Run("RangeQuery", func(t *testing.T) {
		query := `rate(http_requests_total[5m])`
		end := time.Now()
		start := end.Add(-1 * time.Hour)
		result, err := engine.RangeQuery(query, start, end, 5*time.Minute)
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.NotEmpty(t, result.Series)
	})

	// Test aggregation
	t.Run("AggregationQuery", func(t *testing.T) {
		query := `sum(http_requests_total) by (method)`
		result, err := engine.InstantQuery(query, time.Now())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.Len(t, result.Series, 2) // GET and POST methods
	})

	// Test binary operation
	t.Run("BinaryOperation", func(t *testing.T) {
		query := `http_requests_total{status="200"} / http_requests_total`
		result, err := engine.InstantQuery(query, time.Now())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.NotEmpty(t, result.Series)
	})

	// Test function evaluation
	t.Run("FunctionEvaluation", func(t *testing.T) {
		query := `increase(http_requests_total[5m])`
		result, err := engine.InstantQuery(query, time.Now())
		assert.NoError(t, err)
		assert.NotNil(t, result)
		assert.NotEmpty(t, result.Series)
	})
}

func TestPromQLErrorCases(t *testing.T) {
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	engine := NewEngine(store)

	// Test syntax error
	t.Run("SyntaxError", func(t *testing.T) {
		query := `invalid{query`
		_, err := engine.InstantQuery(query, time.Now())
		assert.Error(t, err)
	})

	// Test invalid metric name
	t.Run("InvalidMetric", func(t *testing.T) {
		query := `non_existent_metric`
		result, err := engine.InstantQuery(query, time.Now())
		assert.NoError(t, err)
		assert.Empty(t, result.Series)
	})

	// Test invalid function
	t.Run("InvalidFunction", func(t *testing.T) {
		query := `invalid_function(http_requests_total)`
		_, err := engine.InstantQuery(query, time.Now())
		assert.Error(t, err)
	})

	// Test invalid time range
	t.Run("InvalidTimeRange", func(t *testing.T) {
		query := `http_requests_total`
		end := time.Now()
		start := end.Add(1 * time.Hour) // Start after end
		_, err := engine.RangeQuery(query, start, end, 5*time.Minute)
		assert.Error(t, err)
	})
}

func TestPromQLConcurrency(t *testing.T) {
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	// Setup test data
	series := testutil.GenerateTestSeries(map[string]string{
		"__name__": "test_metric",
		"instance": "test-1",
	}, 1000)
	err = store.WriteSeries(series)
	require.NoError(t, err)

	engine := NewEngine(store)

	// Test concurrent queries
	t.Run("ConcurrentQueries", func(t *testing.T) {
		queries := []string{
			`test_metric`,
			`rate(test_metric[5m])`,
			`sum(test_metric)`,
			`avg(test_metric)`,
		}

		done := make(chan error, len(queries))
		for _, q := range queries {
			go func(query string) {
				_, err := engine.InstantQuery(query, time.Now())
				done <- err
			}(q)
		}

		for i := 0; i < len(queries); i++ {
			err := <-done
			assert.NoError(t, err)
		}
	})
}
