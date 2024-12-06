#ifndef RTBOT_OPENTELEMETRY_INIT_H
#define RTBOT_OPENTELEMETRY_INIT_H

#include <memory>
#include <string>

#include "opentelemetry/exporters/otlp/otlp_http_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"

#ifdef RTBOT_INSTRUMENTATION
void init_telemetry() {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  opts.url = "http://localhost:4318/v1/traces";
  opts.console_debug = true;

  std::cout << "Initializing OpenTelemetry with OTLP exporter at: " << opts.url << std::endl;

  auto exporter = std::make_unique<opentelemetry::exporter::otlp::OtlpHttpExporter>(opts);
  auto processor = std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(std::move(exporter));

  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "rtbot"}, {"service.version", "1.0.0"}, {"deployment.environment", "development"}});

  auto provider = std::make_shared<opentelemetry::sdk::trace::TracerProvider>(std::move(processor), resource);

  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

#endif
#endif