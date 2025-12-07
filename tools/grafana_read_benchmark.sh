#!/bin/bash
# Grafana Dashboard Read Performance Benchmark
# Simulates 25 panels with complex PromQL queries

HOST="localhost:9090"
ITERATIONS=100  # Run each query type multiple times

# Complex PromQL queries simulating real Grafana dashboard panels
QUERIES=(
  # CPU/Memory gauges
  'sum(rate(container_cpu_usage_seconds_total[5m])) by (pod)'
  'sum(container_memory_working_set_bytes) by (namespace)'
  'avg(rate(http_requests_total[1m])) by (service)'
  
  # Rate calculations
  'rate(http_requests_total{status="500"}[5m])'
  'irate(network_receive_bytes_total[1m])'
  'increase(http_requests_total[1h])'
  
  # Aggregations
  'sum by (namespace) (kube_pod_container_resource_requests)'
  'max by (node) (node_memory_MemTotal_bytes - node_memory_MemAvailable_bytes)'
  'min(container_cpu_usage_seconds_total) by (container)'
  'count(up{job="kubernetes-pods"}) by (namespace)'
  
  # Histograms
  'histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))'
  'histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[1m]))'
  'histogram_quantile(0.50, http_request_duration_seconds_bucket)'
  
  # Complex expressions
  'sum(rate(container_cpu_usage_seconds_total[5m])) / sum(kube_pod_container_resource_limits) * 100'
  '(node_memory_MemTotal_bytes - node_memory_MemAvailable_bytes) / node_memory_MemTotal_bytes * 100'
  'rate(http_requests_total{status=~"5.."}[5m]) / rate(http_requests_total[5m]) * 100'
  
  # Label matching
  'up{job=~".*kubernetes.*"}'
  'container_memory_usage_bytes{namespace!="kube-system"}'
  'http_requests_total{method="POST", status="200"}'
  
  # Subqueries
  'max_over_time(rate(http_requests_total[5m])[1h:5m])'
  'avg_over_time(container_cpu_usage_seconds_total[30m])'
  
  # Offset
  'rate(http_requests_total[5m]) - rate(http_requests_total[5m] offset 1h)'
  
  # Binary operations
  'sum(rate(http_requests_total[5m])) > 100'
  'container_memory_usage_bytes / 1024 / 1024'
)

# Also use some actual metrics we have in the database
ACTUAL_QUERIES=(
  'benchmark_metric_0'
  'benchmark_metric_0{namespace="default"}'
  'sum(benchmark_metric_0) by (pod)'
  'rate(benchmark_metric_0[5m])'
  'avg(benchmark_metric_0)'
)

echo "=== Grafana Dashboard Read Performance Benchmark ==="
echo "Host: $HOST"
echo "Queries: ${#QUERIES[@]} complex + ${#ACTUAL_QUERIES[@]} actual = $((${#QUERIES[@]} + ${#ACTUAL_QUERIES[@]})) total"
echo "Iterations per query: $ITERATIONS"
echo ""

# Collect all latencies
declare -a ALL_LATENCIES

# Run benchmark
run_query() {
  local query="$1"
  local start_time=$(python3 -c "import time; print(int(time.time()*1000))")
  curl -s "http://${HOST}/api/v1/query?query=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$query'))")" > /dev/null 2>&1
  local end_time=$(python3 -c "import time; print(int(time.time()*1000))")
  echo $((end_time - start_time))
}

echo "Running queries..."

# Run complex queries (will mostly return empty but test parsing/execution)
for query in "${QUERIES[@]}"; do
  for ((i=0; i<$ITERATIONS; i++)); do
    latency=$(run_query "$query")
    ALL_LATENCIES+=($latency)
  done
done

# Run actual queries (will return data)
for query in "${ACTUAL_QUERIES[@]}"; do
  for ((i=0; i<$ITERATIONS; i++)); do
    latency=$(run_query "$query")
    ALL_LATENCIES+=($latency)
  done
done

# Calculate percentiles
echo ""
echo "=== Query Latency Results ==="
echo "Total queries executed: ${#ALL_LATENCIES[@]}"

# Sort latencies
IFS=$'\n' SORTED_LATENCIES=($(sort -n <<<"${ALL_LATENCIES[*]}")); unset IFS

TOTAL=${#SORTED_LATENCIES[@]}

get_percentile() {
  local p=$1
  local idx=$(echo "scale=0; $TOTAL * $p / 100" | bc)
  if [ $idx -ge $TOTAL ]; then idx=$((TOTAL - 1)); fi
  echo "${SORTED_LATENCIES[$idx]}"
}

echo ""
echo "Percentile | Latency (ms)"
echo "-----------+-------------"
echo "     p10   | $(get_percentile 10)"
echo "     p20   | $(get_percentile 20)"
echo "     p30   | $(get_percentile 30)"
echo "     p40   | $(get_percentile 40)"
echo "     p50   | $(get_percentile 50)"
echo "     p60   | $(get_percentile 60)"
echo "     p70   | $(get_percentile 70)"
echo "     p80   | $(get_percentile 80)"
echo "     p90   | $(get_percentile 90)"
echo "     p99   | $(get_percentile 99)"
echo ""

# Calculate min/max/avg
MIN=${SORTED_LATENCIES[0]}
MAX=${SORTED_LATENCIES[$((TOTAL-1))]}
SUM=0
for lat in "${SORTED_LATENCIES[@]}"; do
  SUM=$((SUM + lat))
done
AVG=$((SUM / TOTAL))

echo "Min: ${MIN}ms | Max: ${MAX}ms | Avg: ${AVG}ms"
