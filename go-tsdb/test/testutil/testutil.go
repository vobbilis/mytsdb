package testutil

import (
	"fmt"
	"math/rand"
	"time"

	"github.com/vobbilis/go-tsdb/internal/types"
)

// GenerateTestSeries creates a series with random samples for testing
func GenerateTestSeries(labels map[string]string, numSamples int) *types.Series {
	samples := make([]*types.Sample, numSamples)
	now := time.Now().UnixNano() / int64(time.Millisecond)

	for i := 0; i < numSamples; i++ {
		samples[i] = &types.Sample{
			Timestamp: now - int64(i*1000), // 1 second intervals
			Value:     rand.Float64() * 100,
		}
	}

	labelPairs := make([]*types.Label, 0, len(labels))
	for k, v := range labels {
		labelPairs = append(labelPairs, &types.Label{
			Name:  k,
			Value: v,
		})
	}

	return &types.Series{
		Labels:  labelPairs,
		Samples: samples,
	}
}

// CompareFloat64 compares two float64 values with epsilon
func CompareFloat64(a, b, epsilon float64) bool {
	diff := a - b
	if diff < 0 {
		diff = -diff
	}
	return diff <= epsilon
}

// GenerateTestData generates a complete test dataset
func GenerateTestData(numSeries, samplesPerSeries int) []*types.Series {
	series := make([]*types.Series, numSeries)
	for i := 0; i < numSeries; i++ {
		labels := map[string]string{
			"__name__": fmt.Sprintf("test_metric_%d", i),
			"instance": fmt.Sprintf("instance-%d", i%3),
			"job":      fmt.Sprintf("job-%d", i%2),
		}
		series[i] = GenerateTestSeries(labels, samplesPerSeries)
	}
	return series
}
