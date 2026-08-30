#include "inferlab/advanced_serving_observability.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace inferlab {
namespace {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::non_negative_finite_or_zero;
using advanced_serving_internal::positive_finite_or_default;
using advanced_serving_internal::positive_or_one;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

double bounded_fraction(const double value, const char* name) {
  return std::clamp(require_finite(value, name), 0.0, 1.0);
}

double zero_capacity_utilization(const double demand,
                                 const double capacity,
                                 const char* name) {
  if (demand <= 0.0) {
    return 0.0;
  }
  if (capacity <= 0.0) {
    return 1.0;
  }
  return require_finite(demand / capacity, name);
}

}  // namespace

ObservabilitySloResult estimate_observability_slo(
    const ObservabilitySloScenario& scenario) {
  const double request_qps = non_negative_finite_or_zero(
      scenario.request_qps, "observability request QPS");
  const double error_rate =
      bounded_fraction(scenario.error_rate, "observability error rate");
  const double p99_ttft_ms = non_negative_finite_or_zero(
      scenario.p99_ttft_ms, "observability P99 TTFT");
  const double p99_tpot_ms = non_negative_finite_or_zero(
      scenario.p99_tpot_ms, "observability P99 TPOT");
  const double ttft_slo_ms = positive_finite_or_default(
      scenario.ttft_slo_ms, 800.0, "observability TTFT SLO");
  const double tpot_slo_ms = positive_finite_or_default(
      scenario.tpot_slo_ms, 40.0, "observability TPOT SLO");
  const double error_budget_percent = std::min(
      100.0, positive_finite_or_default(
                 scenario.error_budget_percent, 1.0,
                 "observability error budget percent"));
  const double observation_window_minutes = positive_finite_or_default(
      scenario.observation_window_minutes, 5.0,
      "observability window minutes");
  const double scrape_interval_s = positive_finite_or_default(
      scenario.scrape_interval_s, 15.0,
      "observability scrape interval seconds");
  const double samples_in_window = require_finite(
      finite_product(observation_window_minutes, 60.0,
                     "observability window seconds") /
          scrape_interval_s,
      "observability samples per window");
  const double ttft_violation_ratio =
      request_qps <= 0.0
          ? 0.0
          : require_finite(
                std::max(0.0, p99_ttft_ms / ttft_slo_ms - 1.0),
                "observability TTFT violation ratio");
  const double tpot_violation_ratio =
      request_qps <= 0.0
          ? 0.0
          : require_finite(
                std::max(0.0, p99_tpot_ms / tpot_slo_ms - 1.0),
                "observability TPOT violation ratio");
  const double error_budget_burn_rate =
      request_qps <= 0.0
          ? 0.0
          : require_finite(error_rate / (error_budget_percent / 100.0),
                           "observability error-budget burn rate");
  const double composite_risk =
      request_qps <= 0.0
          ? 0.0
          : clamp_unit_interval(require_finite(
                error_budget_burn_rate / 14.0 +
                    finite_product(ttft_violation_ratio, 0.35,
                                   "observability TTFT risk") +
                    finite_product(tpot_violation_ratio, 0.35,
                                   "observability TPOT risk"),
                "observability composite SLO risk"));

  ObservabilitySloResult result;
  result.scenario = scenario;
  result.scenario.request_qps = request_qps;
  result.scenario.error_rate = error_rate;
  result.scenario.p99_ttft_ms = p99_ttft_ms;
  result.scenario.p99_tpot_ms = p99_tpot_ms;
  result.scenario.ttft_slo_ms = ttft_slo_ms;
  result.scenario.tpot_slo_ms = tpot_slo_ms;
  result.scenario.error_budget_percent = error_budget_percent;
  result.scenario.observation_window_minutes = observation_window_minutes;
  result.scenario.scrape_interval_s = scrape_interval_s;
  result.samples_per_window = internal::checked_positive_ceil_double_to_int(
      samples_in_window, "observability samples per window");
  result.ttft_violation_ratio = ttft_violation_ratio;
  result.tpot_violation_ratio = tpot_violation_ratio;
  result.error_budget_burn_rate = error_budget_burn_rate;
  result.composite_slo_risk = composite_risk;
  result.page_oncall =
      error_budget_burn_rate >= 14.0 || composite_risk >= 0.85;
  result.ticket_required =
      error_budget_burn_rate >= 2.0 || composite_risk >= 0.35;
  return result;
}

OtelTraceResult estimate_otel_trace(const OtelTraceScenario& scenario) {
  const double request_qps = non_negative_finite_or_zero(
      scenario.request_qps, "OTel trace request QPS");
  const double sample_ratio =
      bounded_fraction(scenario.sample_ratio, "OTel trace sample ratio");
  const int spans_per_request = std::max(0, scenario.spans_per_request);
  const int attributes_per_span = std::max(0, scenario.attributes_per_span);
  const int attribute_payload = internal::checked_multiply_int(
      attributes_per_span, 48, "OTel trace attribute payload bytes");
  const int span_payload = std::max(
      128, internal::checked_add_int(
               std::max(0, scenario.avg_span_payload_bytes), attribute_payload,
               "OTel trace span payload bytes"));
  const int exporter_batch_size = positive_or_one(scenario.exporter_batch_size);
  const double exporter_flush_interval_ms = positive_finite_or_default(
      scenario.exporter_flush_interval_ms, 5000.0,
      "OTel trace exporter flush interval");
  const double collector_p99_latency_ms = non_negative_finite_or_zero(
      scenario.collector_p99_latency_ms, "OTel trace collector P99 latency");
  const double storage_budget_mb_per_hour = non_negative_finite_or_zero(
      scenario.storage_budget_mb_per_hour, "OTel trace storage budget");

  const double sampled_traces_per_second = finite_product(
      request_qps, sample_ratio, "OTel trace sampled traces per second");
  const double sampled_spans_per_second = finite_product(
      sampled_traces_per_second, static_cast<double>(spans_per_request),
      "OTel trace sampled spans per second");
  const double batch_by_size = require_finite(
      sampled_spans_per_second / static_cast<double>(exporter_batch_size),
      "OTel trace size-driven export batches");
  const double batch_by_time =
      sampled_spans_per_second <= 0.0
          ? 0.0
          : require_finite(1000.0 / exporter_flush_interval_ms,
                           "OTel trace time-driven export batches");
  const double export_batches_per_second =
      sampled_spans_per_second <= 0.0
          ? 0.0
          : std::min(sampled_spans_per_second,
                     std::max(batch_by_size, batch_by_time));
  const double telemetry_bytes_per_second = finite_product(
      sampled_spans_per_second, static_cast<double>(span_payload),
      "OTel trace telemetry bytes per second");
  const double telemetry_mb_per_hour = require_finite(
      finite_product(telemetry_bytes_per_second, 3600.0,
                     "OTel trace telemetry bytes per hour") /
          (1024.0 * 1024.0),
      "OTel trace telemetry MiB per hour");
  const double exporter_cpu_percent = std::min(
      100.0,
      require_finite(
          finite_product(sampled_spans_per_second, 0.0025,
                         "OTel trace span CPU cost") +
              finite_product(export_batches_per_second, 0.08,
                             "OTel trace batch CPU cost"),
          "OTel trace exporter CPU percent"));
  const bool storage_budget_exceeded =
      telemetry_mb_per_hour > 0.0 && storage_budget_mb_per_hour <= 0.0;
  const double budget_pressure =
      telemetry_mb_per_hour <= 0.0
          ? 0.0
          : (storage_budget_mb_per_hour <= 0.0
                 ? 1.0
                 : require_finite(
                       telemetry_mb_per_hour / storage_budget_mb_per_hour,
                       "OTel trace storage-budget pressure"));
  const double latency_pressure = require_finite(
      std::max(0.0, collector_p99_latency_ms / 100.0 - 1.0),
      "OTel trace collector-latency pressure");
  const double collector_backpressure_risk =
      sampled_spans_per_second <= 0.0
          ? 0.0
          : (storage_budget_exceeded
                 ? 1.0
                 : clamp_unit_interval(require_finite(
                       finite_product(
                           std::max(0.0, budget_pressure - 1.0), 0.55,
                           "OTel trace storage risk") +
                           finite_product(latency_pressure, 0.45,
                                          "OTel trace latency risk"),
                       "OTel trace collector backpressure risk")));

  OtelTraceResult result;
  result.scenario = scenario;
  result.scenario.request_qps = request_qps;
  result.scenario.sample_ratio = sample_ratio;
  result.scenario.spans_per_request = spans_per_request;
  result.scenario.attributes_per_span = attributes_per_span;
  result.scenario.avg_span_payload_bytes = span_payload;
  result.scenario.exporter_batch_size = exporter_batch_size;
  result.scenario.exporter_flush_interval_ms = exporter_flush_interval_ms;
  result.scenario.collector_p99_latency_ms = collector_p99_latency_ms;
  result.scenario.storage_budget_mb_per_hour = storage_budget_mb_per_hour;
  result.sampled_traces_per_second = sampled_traces_per_second;
  result.sampled_spans_per_second = sampled_spans_per_second;
  result.export_batches_per_second = export_batches_per_second;
  result.telemetry_mb_per_hour = telemetry_mb_per_hour;
  result.exporter_cpu_percent = exporter_cpu_percent;
  result.collector_backpressure_risk = collector_backpressure_risk;
  result.storage_budget_fit =
      telemetry_mb_per_hour <= storage_budget_mb_per_hour;
  result.sampling_recommendation =
      collector_backpressure_risk >= 0.75
          ? "reduce_sampling_or_add_collector"
          : (budget_pressure > 0.85 ? "watch_storage_budget" : "keep");
  result.required_spans = {"gateway.receive", "router.select_worker",
                           "queue.wait", "prefill.execute", "kv.transfer",
                           "decode.step", "stream.first_token",
                           "stream.complete"};
  return result;
}

OtelSamplingBudgetResult estimate_otel_sampling_budget(
    const OtelSamplingBudgetScenario& scenario) {
  const double request_qps = non_negative_finite_or_zero(
      scenario.request_qps, "OTel sampling request QPS");
  const int spans_per_request = std::max(0, scenario.spans_per_request);
  const double avg_span_bytes = positive_finite_or_default(
      scenario.avg_span_bytes, 900.0, "OTel sampling average span bytes");
  const double sample_rate =
      bounded_fraction(scenario.sample_rate, "OTel sampling head rate");
  const double collector_capacity = non_negative_finite_or_zero(
      scenario.collector_capacity_spans_per_sec,
      "OTel sampling collector capacity");
  const double storage_budget_gb_per_day = non_negative_finite_or_zero(
      scenario.storage_budget_gb_per_day, "OTel sampling storage budget");
  const double error_trace_boost = positive_finite_or_default(
      scenario.error_trace_boost, 1.0, "OTel sampling error-trace boost");
  const double tail_sampling_rate = bounded_fraction(
      scenario.tail_sampling_rate, "OTel sampling tail rate");

  const double generated_spans_per_second = finite_product(
      request_qps, static_cast<double>(spans_per_request),
      "OTel generated spans per second");
  const double base_effective_sample = clamp_unit_interval(require_finite(
      sample_rate + finite_product(tail_sampling_rate, 0.50,
                                   "OTel tail-sampling contribution"),
      "OTel base effective sample rate"));
  const double effective_sample_rate = clamp_unit_interval(finite_product(
      base_effective_sample, error_trace_boost,
      "OTel boosted effective sample rate"));
  const double sampled_spans_per_second = finite_product(
      generated_spans_per_second, effective_sample_rate,
      "OTel sampled spans per second");
  const double collector_utilization = zero_capacity_utilization(
      sampled_spans_per_second, collector_capacity,
      "OTel collector utilization");
  const double daily_storage_bytes = finite_product(
      finite_product(sampled_spans_per_second, avg_span_bytes,
                     "OTel sampled bytes per second"),
      86400.0, "OTel daily storage bytes");
  const double daily_storage_gb = require_finite(
      daily_storage_bytes / (1024.0 * 1024.0 * 1024.0),
      "OTel daily storage GiB");
  const double storage_budget_utilization = zero_capacity_utilization(
      daily_storage_gb, storage_budget_gb_per_day,
      "OTel storage-budget utilization");

  double collector_limited_rate = 1.0;
  double storage_limited_rate = 1.0;
  if (generated_spans_per_second > 0.0) {
    collector_limited_rate =
        collector_capacity <= 0.0
            ? 0.0
            : require_finite(
                  finite_product(collector_capacity, 0.75,
                                 "OTel collector target capacity") /
                      generated_spans_per_second,
                  "OTel collector-limited sample rate");
    const double generated_daily_storage_bytes = finite_product(
        finite_product(generated_spans_per_second, avg_span_bytes,
                       "OTel generated bytes per second"),
        86400.0, "OTel generated daily storage bytes");
    storage_limited_rate =
        storage_budget_gb_per_day <= 0.0
            ? 0.0
            : require_finite(
                  finite_product(storage_budget_gb_per_day,
                                 1024.0 * 1024.0 * 1024.0,
                                 "OTel storage-budget bytes") /
                      generated_daily_storage_bytes,
                  "OTel storage-limited sample rate");
  }
  const double recommended_sample_rate = clamp_unit_interval(
      std::min(collector_limited_rate, storage_limited_rate));

  double backpressure_risk = 0.0;
  if (sampled_spans_per_second > 0.0 &&
      (collector_capacity <= 0.0 || storage_budget_gb_per_day <= 0.0)) {
    backpressure_risk = 1.0;
  } else if (sampled_spans_per_second > 0.0) {
    const double span_complexity = finite_product(
        static_cast<double>(std::max(0, spans_per_request - 12)), 0.015,
        "OTel span-complexity risk");
    backpressure_risk = clamp_unit_interval(require_finite(
        finite_product(std::max(collector_utilization - 0.75, 0.0), 0.75,
                       "OTel collector pressure risk") +
            finite_product(
                std::max(storage_budget_utilization - 0.80, 0.0), 0.65,
                "OTel storage pressure risk") +
            span_complexity,
        "OTel backpressure risk"));
  }

  OtelSamplingBudgetResult result;
  result.scenario = scenario;
  result.scenario.request_qps = request_qps;
  result.scenario.spans_per_request = spans_per_request;
  result.scenario.avg_span_bytes = avg_span_bytes;
  result.scenario.sample_rate = sample_rate;
  result.scenario.collector_capacity_spans_per_sec = collector_capacity;
  result.scenario.storage_budget_gb_per_day = storage_budget_gb_per_day;
  result.scenario.error_trace_boost = error_trace_boost;
  result.scenario.tail_sampling_rate = tail_sampling_rate;
  result.generated_spans_per_sec = generated_spans_per_second;
  result.sampled_spans_per_sec = sampled_spans_per_second;
  result.collector_utilization = collector_utilization;
  result.daily_storage_gb = daily_storage_gb;
  result.storage_budget_utilization = storage_budget_utilization;
  result.recommended_sample_rate = recommended_sample_rate;
  result.backpressure_risk_score = backpressure_risk;
  result.sampling_policy =
      backpressure_risk >= 0.60
          ? "tail_sampling_with_rate_limit"
          : (tail_sampling_rate > 0.0 ? "head_plus_tail_sampling"
                                      : "head_sampling_only");
  result.recommendation =
      backpressure_risk >= 0.70
          ? "lower_sample_rate_or_scale_collectors"
          : (storage_budget_utilization >= 0.80
                 ? "reduce_span_payload_or_retention"
                 : "keep_sampling_budget_and_enable_error_boost");
  return result;
}

std::vector<ObservabilitySloScenario> default_observability_slo_scenarios() {
  return {
      {"healthy_window", 80.0, 0.001, 520.0, 24.0, 800.0, 40.0, 1.0, 5.0, 15.0},
      {"ttft_warning", 60.0, 0.004, 980.0, 31.0, 800.0, 40.0, 1.0, 5.0, 15.0},
      {"tpot_warning", 45.0, 0.006, 700.0, 58.0, 800.0, 40.0, 1.0, 5.0, 15.0},
      {"page_burn_rate", 120.0, 0.18, 1400.0, 72.0, 800.0, 40.0, 1.0, 5.0, 15.0},
  };
}

std::vector<OtelTraceScenario> default_otel_trace_scenarios() {
  return {
      {"healthy_chat_tracing", 80.0, 0.05, 9, 12, 384, 512, 5000.0, 25.0, 1024.0},
      {"debug_incident_full_sample", 60.0, 1.00, 12, 16, 512, 256, 2000.0, 45.0, 4096.0},
      {"high_qps_gateway", 420.0, 0.02, 7, 10, 320, 1024, 5000.0, 35.0, 768.0},
      {"multimodal_trace", 25.0, 0.20, 16, 18, 768, 256, 3000.0, 80.0, 1536.0},
  };
}

std::vector<OtelSamplingBudgetScenario> default_otel_sampling_budget_scenarios() {
  return {
      {"tgi_prometheus_otel", "tgi", 65.0, 9, 850.0, 0.10, 8000.0, 35.0, 1.25, 0.04},
      {"dynamo_gateway_router_trace", "nvidia_dynamo", 120.0, 14, 1100.0, 0.08, 12000.0, 70.0, 1.40, 0.08},
      {"kserve_llmd_full_pipeline", "kserve_llm_d", 90.0, 16, 1250.0, 0.12, 10000.0, 60.0, 1.30, 0.10},
      {"vllm_streaming_trace", "vllm", 80.0, 11, 950.0, 0.06, 7000.0, 28.0, 1.15, 0.06},
      {"native_verbose_dev_trace", "native", 20.0, 18, 1400.0, 0.50, 2500.0, 18.0, 1.00, 0.00},
  };
}

}  // namespace inferlab
