package alert

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/vobbilis/go-tsdb/internal/promql"
	"github.com/vobbilis/go-tsdb/internal/storage"
	"github.com/vobbilis/go-tsdb/test/testutil"
)

func TestAlertBasicRules(t *testing.T) {
	// Setup storage and PromQL engine
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	engine := promql.NewEngine(store)
	manager := NewAlertManager(engine)

	// Setup test data
	series := testutil.GenerateTestSeries(map[string]string{
		"__name__": "error_rate",
		"service":  "api",
	}, 100)
	err = store.WriteSeries(series)
	require.NoError(t, err)

	// Test simple threshold rule
	t.Run("ThresholdRule", func(t *testing.T) {
		rule := &Rule{
			Name:     "high_error_rate",
			Query:    `error_rate > 0.5`,
			Duration: 5 * time.Minute,
			Labels: map[string]string{
				"severity": "critical",
			},
			Annotations: map[string]string{
				"summary": "High error rate detected",
			},
		}

		err := manager.AddRule(rule)
		assert.NoError(t, err)

		// Run evaluation
		ctx := context.Background()
		alerts, err := manager.Evaluate(ctx)
		assert.NoError(t, err)
		assert.NotEmpty(t, alerts)
	})

	// Test rate rule
	t.Run("RateRule", func(t *testing.T) {
		rule := &Rule{
			Name:     "error_spike",
			Query:    `rate(error_rate[5m]) > 0.1`,
			Duration: 5 * time.Minute,
			Labels: map[string]string{
				"severity": "warning",
			},
			Annotations: map[string]string{
				"summary": "Error rate is increasing rapidly",
			},
		}

		err := manager.AddRule(rule)
		assert.NoError(t, err)

		// Run evaluation
		ctx := context.Background()
		alerts, err := manager.Evaluate(ctx)
		assert.NoError(t, err)
		assert.NotEmpty(t, alerts)
	})
}

func TestAlertStatePersistence(t *testing.T) {
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	engine := promql.NewEngine(store)
	manager := NewAlertManager(engine)

	// Add test rule
	rule := &Rule{
		Name:     "test_alert",
		Query:    `error_rate > 0.5`,
		Duration: 5 * time.Minute,
		Labels: map[string]string{
			"severity": "warning",
		},
	}

	err = manager.AddRule(rule)
	require.NoError(t, err)

	// Test alert state transitions
	t.Run("StateTransitions", func(t *testing.T) {
		ctx := context.Background()

		// First evaluation
		alerts1, err := manager.Evaluate(ctx)
		assert.NoError(t, err)

		// Second evaluation
		alerts2, err := manager.Evaluate(ctx)
		assert.NoError(t, err)

		// Check if state is maintained
		assert.Equal(t, len(alerts1), len(alerts2))
		for i := range alerts1 {
			assert.Equal(t, alerts1[i].State, alerts2[i].State)
		}
	})
}

func TestAlertNotifications(t *testing.T) {
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	engine := promql.NewEngine(store)

	// Create notification channel
	notifications := make(chan *Alert, 100)
	manager := NewAlertManager(engine, WithNotificationChannel(notifications))

	// Add test rule
	rule := &Rule{
		Name:     "test_notification",
		Query:    `error_rate > 0.5`,
		Duration: 1 * time.Second,
		Labels: map[string]string{
			"severity": "critical",
		},
	}

	err = manager.AddRule(rule)
	require.NoError(t, err)

	// Test notification delivery
	t.Run("NotificationDelivery", func(t *testing.T) {
		ctx := context.Background()

		// Run evaluation
		_, err := manager.Evaluate(ctx)
		assert.NoError(t, err)

		// Check for notification
		select {
		case alert := <-notifications:
			assert.NotNil(t, alert)
			assert.Equal(t, "test_notification", alert.Name)
		case <-time.After(5 * time.Second):
			t.Fatal("Notification timeout")
		}
	})
}

func TestAlertConcurrency(t *testing.T) {
	store, err := storage.NewStorage(storage.StorageConfig{
		DataPath:     t.TempDir(),
		MaxBlockSize: 1024 * 1024,
	})
	require.NoError(t, err)
	defer store.Close()

	engine := promql.NewEngine(store)
	manager := NewAlertManager(engine)

	// Add multiple rules
	rules := []*Rule{
		{
			Name:     "rule1",
			Query:    `metric1 > 0.5`,
			Duration: 5 * time.Minute,
		},
		{
			Name:     "rule2",
			Query:    `metric2 > 0.5`,
			Duration: 5 * time.Minute,
		},
		{
			Name:     "rule3",
			Query:    `metric3 > 0.5`,
			Duration: 5 * time.Minute,
		},
	}

	for _, rule := range rules {
		err := manager.AddRule(rule)
		require.NoError(t, err)
	}

	// Test concurrent evaluations
	t.Run("ConcurrentEvaluations", func(t *testing.T) {
		ctx := context.Background()
		done := make(chan error, 10)

		for i := 0; i < 10; i++ {
			go func() {
				_, err := manager.Evaluate(ctx)
				done <- err
			}()
		}

		for i := 0; i < 10; i++ {
			err := <-done
			assert.NoError(t, err)
		}
	})
}
