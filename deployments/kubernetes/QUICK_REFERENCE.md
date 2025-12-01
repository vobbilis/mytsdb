# MyTSDB Kubernetes Quick Reference

## Installation

```bash
# Option 1: Using install script (recommended)
./install.sh

# Option 2: Using Makefile
make install

# Option 3: Force reinstall
make install-force

# Option 4: Skip pre-flight checks
make install-skip-preflight
```

## Pre-flight Check

```bash
# Run pre-flight checks manually
./preflight.sh

# Or via Makefile
make preflight
```

## Common Commands

### View Status
```bash
make status              # All pods, services, PVCs
make quick-status        # Quick pod status
make watch-pods          # Watch pods (live updates)
```

### View Logs
```bash
make logs-mytsdb         # MyTSDB logs (follow)
make logs-otel           # OTEL Collector logs
make logs-sample-app     # Sample app logs
make logs-grafana        # Grafana logs
make quick-logs          # All logs (last 10 lines)
```

### Restart Services
```bash
make restart-mytsdb      # Restart MyTSDB
make restart-otel        # Restart OTEL Collector
make restart-sample-app  # Restart sample app
make restart-grafana     # Restart Grafana
make restart-all         # Restart everything
```

### Update Services
```bash
make update-mytsdb       # Rebuild, load, restart MyTSDB
make update-otel         # Update OTEL config & restart
make update-sample-app   # Rebuild, load, restart sample app
make update-grafana      # Update dashboards & restart
```

### Scale Services
```bash
make scale-sample-app REPLICAS=3   # Scale sample app
make scale-mytsdb REPLICAS=2       # Scale MyTSDB
```

### Shell Access
```bash
make shell-mytsdb        # Open shell in MyTSDB pod
make shell-otel          # Open shell in OTEL pod
make shell-sample-app    # Open shell in sample app
make shell-grafana       # Open shell in Grafana
```

### Port Forwarding
```bash
make forward-mytsdb      # Forward MyTSDB to localhost:9090
make forward-grafana     # Forward Grafana to localhost:3000
make forward-otel        # Forward OTEL to localhost:4317,4318
```

### Debugging
```bash
make describe-mytsdb     # Describe MyTSDB pod
make describe-otel       # Describe OTEL pod
make events              # Show recent events
make test-mytsdb-health  # Test health endpoint
make test-connectivity   # Test service connectivity
```

### Cleanup
```bash
make clean-namespace     # Delete namespace only
make clean               # Full cleanup (delete cluster)
./cleanup.sh             # Alternative cleanup script
```

## Access URLs

- **Grafana**: http://localhost:3000 (admin/admin)
- **MyTSDB** (via port-forward): http://localhost:9090

## Makefile Help

```bash
make help                # Show all available targets
```

## Troubleshooting

### Pods not starting
```bash
make status              # Check pod status
make describe-mytsdb     # Get detailed info
make logs-mytsdb         # Check logs
```

### No metrics in Grafana
```bash
make logs-sample-app     # Verify app is generating metrics
make logs-otel           # Check OTEL is forwarding
make logs-mytsdb         # Check MyTSDB is receiving
```

### Port conflicts
```bash
./preflight.sh           # Check port availability
lsof -i :3000            # Find process using port
```

### Image issues
```bash
make build-all           # Rebuild all images
make load-all            # Load into Kind
make restart-all         # Restart all services
```

## File Structure

```
deployments/kubernetes/
├── Makefile             # All deployment targets
├── preflight.sh         # Pre-flight checks
├── install.sh           # Fresh install script
├── cleanup.sh           # Cleanup script
├── README.md            # Full documentation
├── QUICK_REFERENCE.md   # This file
├── kind/                # Kind cluster config
├── mytsdb/              # MyTSDB manifests
├── otel-collector/      # OTEL manifests
├── sample-app/          # Sample app
└── grafana/             # Grafana manifests
```

## Next Steps

1. Run `make install` to deploy
2. Open http://localhost:3000
3. View pre-configured dashboard
4. Explore metrics!
