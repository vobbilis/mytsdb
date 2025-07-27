package storage

import (
	"sync"
	"sync/atomic"
)

// Sample represents a single time series data point
type Sample struct {
	Timestamp int64
	Value     float64
}

// Labels represents time series labels
type Labels map[string]string

// Series represents a single time series
type Series struct {
	// Immutable fields
	labels Labels

	// Mutable fields protected by mutex
	mu      sync.RWMutex
	samples []Sample

	// Atomic fields
	lastTimestamp atomic.Int64
	lastValue     atomic.Float64

	// Memory-mapped block reference for historical data
	blockRef atomic.Pointer[Block]
}

// Block represents a compressed block of samples
type Block struct {
	StartTime int64
	EndTime   int64
	Data      []byte
}

// NewSeries creates a new time series
func NewSeries(labels Labels) *Series {
	s := &Series{
		labels:  labels,
		samples: make([]Sample, 0, 1024), // Pre-allocate space for efficiency
	}
	return s
}

// AppendSample adds a new sample to the series
func (s *Series) AppendSample(timestamp int64, value float64) {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Update atomic fields for quick access
	s.lastTimestamp.Store(timestamp)
	s.lastValue.Store(value)

	// Append to samples
	s.samples = append(s.samples, Sample{
		Timestamp: timestamp,
		Value:     value,
	})

	// TODO: Implement block rotation when samples slice grows too large
}

// GetSamples returns samples within the specified time range
func (s *Series) GetSamples(start, end int64) []Sample {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if len(s.samples) == 0 {
		return nil
	}

	// Binary search for start index - first sample with timestamp >= start
	startIdx := s.binarySearch(start)
	if startIdx == -1 {
		return nil
	}

	// Binary search for end index - first sample with timestamp > end
	// This creates a half-open interval [start, end]
	endIdx := s.binarySearchUpper(end + 1)
	if endIdx == -1 {
		endIdx = len(s.samples)
	}

	if startIdx >= endIdx {
		return nil
	}

	// Create a copy to return
	result := make([]Sample, endIdx-startIdx)
	copy(result, s.samples[startIdx:endIdx])
	return result
}

// binarySearch finds the index of the first sample with timestamp >= t
func (s *Series) binarySearch(t int64) int {
	left, right := 0, len(s.samples)
	for left < right {
		mid := (left + right) / 2
		if s.samples[mid].Timestamp < t {
			left = mid + 1
		} else {
			right = mid
		}
	}
	if left >= len(s.samples) {
		return -1
	}
	return left
}

// binarySearchUpper finds the index of the first sample with timestamp >= t
// Used for finding the upper bound in range queries
func (s *Series) binarySearchUpper(t int64) int {
	left, right := 0, len(s.samples)
	for left < right {
		mid := (left + right) / 2
		if s.samples[mid].Timestamp < t {
			left = mid + 1
		} else {
			right = mid
		}
	}
	return left
}
