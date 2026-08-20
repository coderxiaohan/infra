#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab {

struct ObservabilitySloScenario {
  std::string name;
  double request_qps = 1.0;
  double error_rate = 0.0;
  double p99_ttft_ms = 0.0;
  double p99_tpot_ms = 0.0;
  double ttft_slo_ms = 800.0;
  double tpot_slo_ms = 40.0;
  double error_budget_percent = 1.0;
  double observation_window_minutes = 5.0;
  double scrape_interval_s = 15.0;
};

struct ObservabilitySloResult {
  ObservabilitySloScenario scenario;
  int samples_per_window = 0;
  double ttft_violation_ratio = 0.0;
  double tpot_violation_ratio = 0.0;
  double error_budget_burn_rate = 0.0;
  double composite_slo_risk = 0.0;
  bool page_oncall = false;
  bool ticket_required = false;
};

ObservabilitySloResult estimate_observability_slo(
    const ObservabilitySloScenario& scenario);

struct OtelTraceScenario {
  std::string name;
  double request_qps = 1.0;
  double sample_ratio = 0.05;
  int spans_per_request = 8;
  int attributes_per_span = 10;
  int avg_span_payload_bytes = 512;
  int exporter_batch_size = 512;
  double exporter_flush_interval_ms = 5000.0;
  double collector_p99_latency_ms = 25.0;
  double storage_budget_mb_per_hour = 1024.0;
};

struct OtelTraceResult {
  OtelTraceScenario scenario;
  double sampled_traces_per_second = 0.0;
  double sampled_spans_per_second = 0.0;
  double export_batches_per_second = 0.0;
  double telemetry_mb_per_hour = 0.0;
  double exporter_cpu_percent = 0.0;
  double collector_backpressure_risk = 0.0;
  bool storage_budget_fit = true;
  std::string sampling_recommendation;
  std::vector<std::string> required_spans;
};

OtelTraceResult estimate_otel_trace(const OtelTraceScenario& scenario);

struct OtelSamplingBudgetScenario {
  std::string name;
  std::string backend;
  double request_qps = 10.0;
  int spans_per_request = 8;
  double avg_span_bytes = 900.0;
  double sample_rate = 0.10;
  double collector_capacity_spans_per_sec = 5000.0;
  double storage_budget_gb_per_day = 20.0;
  double error_trace_boost = 1.0;
  double tail_sampling_rate = 0.05;
};

struct OtelSamplingBudgetResult {
  OtelSamplingBudgetScenario scenario;
  double generated_spans_per_sec = 0.0;
  double sampled_spans_per_sec = 0.0;
  double collector_utilization = 0.0;
  double daily_storage_gb = 0.0;
  double storage_budget_utilization = 0.0;
  double recommended_sample_rate = 0.0;
  double backpressure_risk_score = 0.0;
  std::string sampling_policy;
  std::string recommendation;
};

OtelSamplingBudgetResult estimate_otel_sampling_budget(
    const OtelSamplingBudgetScenario& scenario);

std::vector<ObservabilitySloScenario> default_observability_slo_scenarios();
std::vector<OtelTraceScenario> default_otel_trace_scenarios();
std::vector<OtelSamplingBudgetScenario> default_otel_sampling_budget_scenarios();

}  // namespace inferlab
