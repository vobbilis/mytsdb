#!/bin/bash
echo "Querying Server Metrics API..."

# 1. Get Total Query Duration (Seconds)
DURATION_JSON=$(curl -s 'http://localhost:9090/api/v1/query?query=mytsdb_query_duration_seconds_total')
DURATION=$(echo $DURATION_JSON | grep -o '"value":\[[0-9.]*,"[0-9.]*"]' | awk -F'"' '{print $4}')

# 2. Get Total Query Count
COUNT_JSON=$(curl -s 'http://localhost:9090/api/v1/query?query=mytsdb_query_count_total')
COUNT=$(echo $COUNT_JSON | grep -o '"value":\[[0-9.]*,"[0-9.]*"]' | awk -F'"' '{print $4}')

echo "Duration Total (s): $DURATION"
echo "Count Total: $COUNT"

if [ ! -z "$DURATION" ] && [ ! -z "$COUNT" ] && [ "$COUNT" != "0" ]; then
    AVG_LATENCY=$(echo "scale=6; $DURATION * 1000 / $COUNT" | bc)
    echo "Calculated Avg Read Latency: $AVG_LATENCY ms"
else
    echo "Could not calculate metrics (Server not running or no queries?)"
fi

# 3. Get Write Thread Wait Time (Mutex)
MUTEX_JSON=$(curl -s 'http://localhost:9090/api/v1/query?query=mytsdb_write_mutex_lock_seconds_total')
MUTEX=$(echo $MUTEX_JSON | grep -o '"value":\[[0-9.]*,"[0-9.]*"]' | awk -F'"' '{print $4}')

if [ ! -z "$MUTEX" ]; then
    echo "Total Mutex Wait (s): $MUTEX"
fi
