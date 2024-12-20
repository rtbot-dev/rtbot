# OpenTelemetry Local Setup Guide

## Quick Start

1. Install Docker and Docker Compose
2. Create `docker-compose.yml`:

```yaml
version: "3"
services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686" # Web UI
      - "4317:4317" # OTLP gRPC
      - "4318:4318" # OTLP HTTP
    environment:
      - COLLECTOR_OTLP_ENABLED=true

  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
```

3. Create `prometheus.yml`:

```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: "opentelemetry"
    static_configs:
      - targets: ["jaeger:14269"]
```

4. Start services:

```bash
docker-compose up -d
```

## Access UIs

- Jaeger UI: http://localhost:16686

## Configure Application

Initialize exporter:

```cpp
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"

void init_telemetry() {
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
    opts.url = "http://localhost:4318/v1/traces";

    auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(opts);
    auto processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    auto provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processor));

    opentelemetry::trace::Provider::SetTracerProvider(provider);
    RTBOT_INIT_TELEMETRY("rtbot");
}
```

## Data Types

- **Traces**: Request flows through your system

  - View in Jaeger
  - Search by service, operation, tags
  - View trace timeline and spans

- **Metrics**: Numeric measurements
  - View in Prometheus
  - Query using PromQL
  - Create graphs and alerts

## Troubleshooting

- Check collector logs:

```bash
docker-compose logs jaeger
```

- Verify endpoints:

```bash
curl localhost:4318/health
```

- Common ports:
  - 4317: OTLP gRPC
  - 4318: OTLP HTTP
  - 16686: Jaeger UI
  - 9090: Prometheus UI
