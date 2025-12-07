#!/bin/bash
# Use $7 for the duration value
grep "SLOW QUERY" benchmarks/server.log | awk '{print $7}' | sed 's/ms//' | sort -n > durations.txt
count=$(wc -l < durations.txt)
echo "Total Read Queries: $count"
if [ "$count" -gt 0 ]; then
    min=$(head -n 1 durations.txt)
    max=$(tail -n 1 durations.txt)
    p50_idx=$((count / 2))
    p99_idx=$((count * 99 / 100))
    p50=$(sed -n "${p50_idx}p" durations.txt)
    p99=$(sed -n "${p99_idx}p" durations.txt)
    echo "Read Performance (Server-Side):"
    echo "  Min: $min ms"
    echo "  Max: $max ms"
    echo "  P50: $p50 ms"
    echo "  P99: $p99 ms"
fi
rm durations.txt
