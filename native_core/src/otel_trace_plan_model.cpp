#include "otel_trace_plan_model.hpp"

#include "checked_index.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::otel_trace_plan {
namespace {

const std::vector<std::string>& canonical_attribute_contract() {
  static const std::vector<std::string> attributes = {
      "inferlab.request_id", "inferlab.backend", "inferlab.model",
      "inferlab.prompt_tokens", "inferlab.output_tokens",
      "inferlab.worker_id", "inferlab.cache_hit", "inferlab.ttft_ms",
      "inferlab.tpot_ms"};
  return attributes;
}

bool same_scenario(const OtelTraceScenario& lhs,
                   const OtelTraceScenario& rhs) {
  return lhs.name == rhs.name &&
         lhs.request_qps == rhs.request_qps &&
         lhs.sample_ratio == rhs.sample_ratio &&
         lhs.spans_per_request == rhs.spans_per_request &&
         lhs.attributes_per_span == rhs.attributes_per_span &&
         lhs.avg_span_payload_bytes == rhs.avg_span_payload_bytes &&
         lhs.exporter_batch_size == rhs.exporter_batch_size &&
         lhs.exporter_flush_interval_ms ==
             rhs.exporter_flush_interval_ms &&
         lhs.collector_p99_latency_ms ==
             rhs.collector_p99_latency_ms &&
         lhs.storage_budget_mb_per_hour ==
             rhs.storage_budget_mb_per_hour;
}

bool same_result(const OtelTraceResult& lhs,
                 const OtelTraceResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.sampled_traces_per_second ==
             rhs.sampled_traces_per_second &&
         lhs.sampled_spans_per_second ==
             rhs.sampled_spans_per_second &&
         lhs.export_batches_per_second ==
             rhs.export_batches_per_second &&
         lhs.telemetry_mb_per_hour == rhs.telemetry_mb_per_hour &&
         lhs.exporter_cpu_percent == rhs.exporter_cpu_percent &&
         lhs.collector_backpressure_risk ==
             rhs.collector_backpressure_risk &&
         lhs.storage_budget_fit == rhs.storage_budget_fit &&
         lhs.sampling_recommendation == rhs.sampling_recommendation &&
         lhs.required_spans == rhs.required_spans;
}

void validate_unique_nonempty_strings(
    const std::vector<std::string>& values,
    const char* label) {
  if (values.empty()) {
    throw std::logic_error(
        std::string("OTel trace ") + label + " must not be empty");
  }
  std::unordered_set<std::string> identities;
  identities.reserve(values.size());
  for (const auto& value : values) {
    if (value.empty() || !identities.insert(value).second) {
      throw std::logic_error(
          std::string("OTel trace ") + label +
          " must contain unique nonempty values");
    }
  }
}

void validate_scenarios(
    const std::vector<OtelTraceScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "OTel trace scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty()) {
      throw std::logic_error(
          "OTel trace scenario identity is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "OTel trace scenario names must be unique");
    }
  }
}

Summary summarize(const std::vector<OtelTraceResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "OTel trace records must not be empty");
  }
  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].telemetry_mb_per_hour >
        records[result.max_telemetry_index].telemetry_mb_per_hour) {
      result.max_telemetry_index = index;
    }
  }
  result.over_budget_count = internal::checked_count_if(
      records.begin(), records.end(),
      [](const auto& record) { return !record.storage_budget_fit; },
      "OTel trace over-budget count");
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_otel_trace_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_otel_trace(scenario));
  }
  if (plan.records.empty()) {
    throw std::logic_error(
        "OTel trace records must not be empty");
  }
  plan.schema.attribute_contract = canonical_attribute_contract();
  plan.schema.required_spans = plan.records.front().required_spans;
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "OTel trace scenario/record alignment is invalid");
  }
  if (plan.summary.max_telemetry_index >= plan.records.size()) {
    throw std::logic_error(
        "OTel trace summary index is out of range");
  }
  validate_unique_nonempty_strings(
      plan.schema.attribute_contract, "attribute contract");
  validate_unique_nonempty_strings(
      plan.schema.required_spans, "required spans");
  if (plan.schema.attribute_contract != canonical_attribute_contract()) {
    throw std::logic_error(
        "OTel trace attribute contract is not canonical");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected = estimate_otel_trace(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       expected.scenario) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "OTel trace record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "OTel trace normalized names must be unique");
    }
    if (expected.required_spans != plan.schema.required_spans) {
      throw std::logic_error(
          "OTel trace required-span schema drifted across records");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_telemetry_index !=
          expected_summary.max_telemetry_index ||
      plan.summary.over_budget_count !=
          expected_summary.over_budget_count) {
    throw std::logic_error(
        "OTel trace summary is not canonical");
  }
}

}  // namespace inferlab::otel_trace_plan
