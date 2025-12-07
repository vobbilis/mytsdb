#!/bin/bash

# Get absolute path to script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
LOG_FILE="$SCRIPT_DIR/server.log"
SERVER_BIN="$SCRIPT_DIR/../build/src/tsdb/tsdb_server"
TIMEOUT=600 # 10 minutes for large datasets

echo "Checking for existing server..."
if pgrep -f "tsdb_server" > /dev/null; then
    echo "Server is already running."
    exit 0
fi

echo "Starting server execution..."
nohup $SERVER_BIN > $LOG_FILE 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID. Logs in $LOG_FILE"

echo "Waiting for server readiness..."
start_time=$(date +%s)

while true; do
    if grep -q "Arrow Flight server listening" $LOG_FILE; then
        echo "Server is READY! (Listening on Arrow port)"
        exit 0
    fi
    
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    
    if [ $elapsed -gt $TIMEOUT ]; then
        echo "Timeout ($TIMEOUT s) reached waiting for server startup."
        tail -n 20 $LOG_FILE
        exit 1
    fi
    
    # Show progress if recovering blocks
    if grep -q "Recovering blocks" $LOG_FILE; then
        echo -ne "Recovering blocks... (${elapsed}s elapsed)\r"
    else
        echo -ne "Initializing... (${elapsed}s elapsed)\r"
    fi
    
    sleep 5
done
