#!/usr/bin/env python3
"""
Sample application that generates metrics and sends them via OTEL
"""
import time
import random
from opentelemetry import metrics
from opentelemetry.sdk.metrics import MeterProvider
from opentelemetry.sdk.metrics.export import PeriodicExportingMetricReader
from opentelemetry.exporter.otlp.proto.grpc.metric_exporter import OTLPMetricExporter
from opentelemetry.sdk.resources import Resource

# Configure OTEL
resource = Resource.create({"service.name": "sample-app", "service.version": "1.0.0"})

exporter = OTLPMetricExporter(
    endpoint="otel-collector.mytsdb.svc.cluster.local:4317",
    insecure=True
)

reader = PeriodicExportingMetricReader(exporter, export_interval_millis=10000)
provider = MeterProvider(resource=resource, metric_readers=[reader])
metrics.set_meter_provider(provider)

# Create meter
meter = metrics.get_meter("sample-app")

# Create metrics
request_counter = meter.create_counter(
    "http_requests_total",
    description="Total HTTP requests",
    unit="1"
)

request_duration = meter.create_histogram(
    "http_request_duration_seconds",
    description="HTTP request duration",
    unit="s"
)

active_requests = meter.create_up_down_counter(
    "http_requests_active",
    description="Active HTTP requests",
    unit="1"
)

# Simulate application
print("Sample application started, generating metrics...")
print("Sending metrics to: otel-collector.mytsdb.svc.cluster.local:4317")

methods = ["GET", "POST", "PUT", "DELETE"]
paths = ["/api/users", "/api/products", "/api/orders", "/health"]
status_codes = [200, 201, 400, 404, 500]

while True:
    # Simulate requests
    method = random.choice(methods)
    path = random.choice(paths)
    status = random.choices(status_codes, weights=[70, 10, 10, 5, 5])[0]
    duration = random.uniform(0.01, 2.0)
    
    # Record metrics
    request_counter.add(1, {"method": method, "path": path, "status": str(status)})
    request_duration.record(duration, {"method": method, "path": path})
    
    # Simulate active requests
    if random.random() > 0.5:
        active_requests.add(1)
    else:
        active_requests.add(-1)
    
    print(f"Generated: {method} {path} - {status} ({duration:.3f}s)")
    
    # Sleep between requests
    time.sleep(random.uniform(0.5, 3.0))
