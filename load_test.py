import requests
import time
import random
import threading

def send_metrics(start_index, count):
    url = "http://localhost:9993/api/v1/write"
    for i in range(count):
        idx = start_index + i
        timestamp = int(time.time() * 1000)
        
        # Create a payload simulating Prometheus form data
        # Note: The server expects form-urlencoded data for simple writes based on previous curl examples
        # metric_name, labels (json), value, timestamp
        
        data = {
            "metric_name": f"load_test_metric_{idx}",
            "labels": f'{{"job":"load_test","instance":"server_{idx % 10}"}}',
            "value": str(random.randint(0, 1000)),
            "timestamp": str(timestamp)
        }
        
        try:
            requests.post(url, data=data, timeout=1)
        except Exception as e:
            pass # Ignore errors for load testing

threads = []
num_threads = 4
metrics_per_thread = 250

print(f"Starting load test with {num_threads} threads, {metrics_per_thread} metrics each...")
start_time = time.time()

for i in range(num_threads):
    t = threading.Thread(target=send_metrics, args=(i * metrics_per_thread, metrics_per_thread))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

duration = time.time() - start_time
print(f"Load test completed in {duration:.2f} seconds")
