FROM python:3.11-slim

WORKDIR /app

# Install OTEL dependencies
RUN pip install --no-cache-dir \
    opentelemetry-api \
    opentelemetry-sdk \
    opentelemetry-exporter-otlp-proto-grpc

# Copy application
COPY app.py .

# Run application
CMD ["python", "app.py"]
