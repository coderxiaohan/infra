#include "observability_evidence_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::observability_evidence {
namespace {

constexpr const char* kArtifactType = "native_observability_evidence";
constexpr const char* kArtifactPath =
    "results/raw/native_observability_evidence.json";
constexpr const char* kTargetName = "inferlab_observability_evidence";
constexpr const char* kRefreshCommand =
    "./build/inferlab_observability_evidence --repo-root . --output results/raw/native_observability_evidence.json";
constexpr const char* kClaimScope =
    "compiled C++ analytical observability evidence covering SLO risk, OTel trace volume, sampling budget, and failure-drill planning; not metric collection, trace export, alert observation, fault injection, rollback execution, benchmark, product execute, or release readiness";

const std::vector<std::string>& product_alignment() {
  static const std::vector<std::string> products = {
      "vLLM", "SGLang", "TensorRT-LLM", "Dynamo", "KServe",
      "llm-d", "Ray Serve", "OpenTelemetry", "Prometheus"};
  return products;
}

template <typename Record, typename Renderer>
std::string record_array_json(const std::vector<Record>& records,
                              const int indent, Renderer renderer) {
  if (records.empty()) {
    throw std::invalid_argument(
        "observability evidence record array must not be empty");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0U; index < records.size(); ++index) {
    out << renderer(records[index], indent + 2)
        << (index + 1U == records.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string slo_record_json(const ObservabilitySloResult& result,
                            const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& scenario = result.scenario;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(scenario.request_qps) << ",\n";
  out << nested << "\"error_rate\": "
      << inferlab_plan::fixed6(scenario.error_rate) << ",\n";
  out << nested << "\"p99_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.p99_ttft_ms) << ",\n";
  out << nested << "\"p99_tpot_ms\": "
      << inferlab_plan::fixed6(scenario.p99_tpot_ms) << ",\n";
  out << nested << "\"ttft_slo_ms\": "
      << inferlab_plan::fixed6(scenario.ttft_slo_ms) << ",\n";
  out << nested << "\"tpot_slo_ms\": "
      << inferlab_plan::fixed6(scenario.tpot_slo_ms) << ",\n";
  out << nested << "\"error_budget_percent\": "
      << inferlab_plan::fixed6(scenario.error_budget_percent) << ",\n";
  out << nested << "\"observation_window_minutes\": "
      << inferlab_plan::fixed6(scenario.observation_window_minutes) << ",\n";
  out << nested << "\"scrape_interval_s\": "
      << inferlab_plan::fixed6(scenario.scrape_interval_s) << ",\n";
  out << nested << "\"samples_per_window\": "
      << result.samples_per_window << ",\n";
  out << nested << "\"ttft_violation_ratio\": "
      << inferlab_plan::fixed6(result.ttft_violation_ratio) << ",\n";
  out << nested << "\"tpot_violation_ratio\": "
      << inferlab_plan::fixed6(result.tpot_violation_ratio) << ",\n";
  out << nested << "\"error_budget_burn_rate\": "
      << inferlab_plan::fixed6(result.error_budget_burn_rate) << ",\n";
  out << nested << "\"composite_slo_risk\": "
      << inferlab_plan::fixed6(result.composite_slo_risk) << ",\n";
  out << nested << "\"page_oncall\": "
      << inferlab_plan::json_bool(result.page_oncall) << ",\n";
  out << nested << "\"ticket_required\": "
      << inferlab_plan::json_bool(result.ticket_required) << "\n";
  out << pad << "}";
  return out.str();
}

std::string trace_record_json(const OtelTraceResult& result,
                              const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& scenario = result.scenario;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(scenario.request_qps) << ",\n";
  out << nested << "\"sample_ratio\": "
      << inferlab_plan::fixed6(scenario.sample_ratio) << ",\n";
  out << nested << "\"spans_per_request\": "
      << scenario.spans_per_request << ",\n";
  out << nested << "\"attributes_per_span\": "
      << scenario.attributes_per_span << ",\n";
  out << nested << "\"avg_span_payload_bytes\": "
      << scenario.avg_span_payload_bytes << ",\n";
  out << nested << "\"exporter_batch_size\": "
      << scenario.exporter_batch_size << ",\n";
  out << nested << "\"exporter_flush_interval_ms\": "
      << inferlab_plan::fixed6(scenario.exporter_flush_interval_ms) << ",\n";
  out << nested << "\"collector_p99_latency_ms\": "
      << inferlab_plan::fixed6(scenario.collector_p99_latency_ms) << ",\n";
  out << nested << "\"storage_budget_mb_per_hour\": "
      << inferlab_plan::fixed6(scenario.storage_budget_mb_per_hour) << ",\n";
  out << nested << "\"sampled_traces_per_second\": "
      << inferlab_plan::fixed6(result.sampled_traces_per_second) << ",\n";
  out << nested << "\"sampled_spans_per_second\": "
      << inferlab_plan::fixed6(result.sampled_spans_per_second) << ",\n";
  out << nested << "\"export_batches_per_second\": "
      << inferlab_plan::fixed6(result.export_batches_per_second) << ",\n";
  out << nested << "\"telemetry_mb_per_hour\": "
      << inferlab_plan::fixed6(result.telemetry_mb_per_hour) << ",\n";
  out << nested << "\"exporter_cpu_percent\": "
      << inferlab_plan::fixed6(result.exporter_cpu_percent) << ",\n";
  out << nested << "\"collector_backpressure_risk\": "
      << inferlab_plan::fixed6(result.collector_backpressure_risk) << ",\n";
  out << nested << "\"storage_budget_fit\": "
      << inferlab_plan::json_bool(result.storage_budget_fit) << ",\n";
  out << nested << "\"sampling_recommendation\": "
      << inferlab_plan::json_string(result.sampling_recommendation) << "\n";
  out << pad << "}";
  return out.str();
}

std::string sampling_record_json(const OtelSamplingBudgetResult& result,
                                 const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& scenario = result.scenario;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(scenario.request_qps) << ",\n";
  out << nested << "\"spans_per_request\": "
      << scenario.spans_per_request << ",\n";
  out << nested << "\"avg_span_bytes\": "
      << inferlab_plan::fixed6(scenario.avg_span_bytes) << ",\n";
  out << nested << "\"sample_rate\": "
      << inferlab_plan::fixed6(scenario.sample_rate) << ",\n";
  out << nested << "\"collector_capacity_spans_per_sec\": "
      << inferlab_plan::fixed6(scenario.collector_capacity_spans_per_sec)
      << ",\n";
  out << nested << "\"storage_budget_gb_per_day\": "
      << inferlab_plan::fixed6(scenario.storage_budget_gb_per_day) << ",\n";
  out << nested << "\"error_trace_boost\": "
      << inferlab_plan::fixed6(scenario.error_trace_boost) << ",\n";
  out << nested << "\"tail_sampling_rate\": "
      << inferlab_plan::fixed6(scenario.tail_sampling_rate) << ",\n";
  out << nested << "\"generated_spans_per_sec\": "
      << inferlab_plan::fixed6(result.generated_spans_per_sec) << ",\n";
  out << nested << "\"sampled_spans_per_sec\": "
      << inferlab_plan::fixed6(result.sampled_spans_per_sec) << ",\n";
  out << nested << "\"collector_utilization\": "
      << inferlab_plan::fixed6(result.collector_utilization) << ",\n";
  out << nested << "\"daily_storage_gb\": "
      << inferlab_plan::fixed6(result.daily_storage_gb) << ",\n";
  out << nested << "\"storage_budget_utilization\": "
      << inferlab_plan::fixed6(result.storage_budget_utilization) << ",\n";
  out << nested << "\"recommended_sample_rate\": "
      << inferlab_plan::fixed6(result.recommended_sample_rate) << ",\n";
  out << nested << "\"backpressure_risk_score\": "
      << inferlab_plan::fixed6(result.backpressure_risk_score) << ",\n";
  out << nested << "\"sampling_policy\": "
      << inferlab_plan::json_string(result.sampling_policy) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation) << "\n";
  out << pad << "}";
  return out.str();
}

std::string drill_record_json(const failure_drill::Record& record,
                              const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(record.scenario.name) << ",\n";
  out << nested << "\"layer\": "
      << inferlab_plan::json_string(
             failure_drill::layer_name(record.scenario.layer))
      << ",\n";
  out << nested << "\"component\": "
      << inferlab_plan::json_string(record.scenario.component) << ",\n";
  out << nested << "\"trigger\": "
      << inferlab_plan::json_string(record.scenario.trigger) << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(record.scenario.request_qps) << ",\n";
  out << nested << "\"affected_replicas\": "
      << record.scenario.affected_replicas << ",\n";
  out << nested << "\"total_replicas\": "
      << record.scenario.total_replicas << ",\n";
  out << nested << "\"blast_radius_ratio\": "
      << inferlab_plan::fixed6(record.blast_radius_ratio) << ",\n";
  out << nested << "\"recovery_time_seconds\": "
      << inferlab_plan::fixed6(record.recovery_time_seconds) << ",\n";
  out << nested << "\"estimated_bad_requests\": "
      << inferlab_plan::fixed6(record.estimated_bad_requests) << ",\n";
  out << nested << "\"error_budget_burn_multiple\": "
      << inferlab_plan::fixed6(record.error_budget_burn_multiple) << ",\n";
  out << nested << "\"p99_degradation_ms\": "
      << inferlab_plan::fixed6(record.p99_degradation_ms) << ",\n";
  out << nested << "\"severity\": "
      << inferlab_plan::json_string(
             failure_drill::severity_name(record.severity))
      << ",\n";
  out << nested << "\"rollback_required\": "
      << inferlab_plan::json_bool(record.rollback_required) << ",\n";
  out << nested << "\"blast_radius_budget_exceeded\": "
      << inferlab_plan::json_bool(record.blast_radius_budget_exceeded)
      << ",\n";
  out << nested << "\"recovery_target_exceeded\": "
      << inferlab_plan::json_bool(record.recovery_target_exceeded) << ",\n";
  out << nested << "\"runbook_policy_id\": "
      << inferlab_plan::json_string(
             failure_drill::runbook_policy_id(record.runbook_policy))
      << ",\n";
  out << nested << "\"expected_signals\": "
      << inferlab_plan::json_string_array(record.expected_signals, indent + 2)
      << ",\n";
  out << nested << "\"runbook_steps\": "
      << inferlab_plan::json_string_array(record.runbook_steps, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string slo_section_json(const observability_slo_plan::Plan& plan,
                             const int indent) {
  observability_slo_plan::validate_plan(plan);
  const auto& max_burn = plan.records[plan.summary.max_burn_index];
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"summary\": {\n";
  out << nested << "  \"max_error_budget_burn_rate\": "
      << inferlab_plan::fixed6(max_burn.error_budget_burn_rate) << ",\n";
  out << nested << "  \"max_error_budget_burn_scenario\": "
      << inferlab_plan::json_string(max_burn.scenario.name) << ",\n";
  out << nested << "  \"page_oncall_count\": "
      << plan.summary.page_oncall_count << ",\n";
  out << nested << "  \"scenario_count\": " << plan.records.size()
      << "\n";
  out << nested << "},\n";
  out << nested << "\"records\": "
      << record_array_json(plan.records, indent + 2, slo_record_json) << "\n";
  out << pad << "}";
  return out.str();
}

std::string trace_section_json(const otel_trace_plan::Plan& plan,
                               const int indent) {
  otel_trace_plan::validate_plan(plan);
  const auto& max_telemetry =
      plan.records[plan.summary.max_telemetry_index];
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"schema\": {\n";
  out << nested << "  \"attribute_contract\": "
      << inferlab_plan::json_string_array(plan.schema.attribute_contract,
                                          indent + 4)
      << ",\n";
  out << nested << "  \"required_spans\": "
      << inferlab_plan::json_string_array(plan.schema.required_spans,
                                          indent + 4)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"summary\": {\n";
  out << nested << "  \"max_telemetry_mb_per_hour\": "
      << inferlab_plan::fixed6(max_telemetry.telemetry_mb_per_hour)
      << ",\n";
  out << nested << "  \"max_telemetry_scenario\": "
      << inferlab_plan::json_string(max_telemetry.scenario.name) << ",\n";
  out << nested << "  \"over_budget_count\": "
      << plan.summary.over_budget_count << ",\n";
  out << nested << "  \"scenario_count\": " << plan.records.size()
      << "\n";
  out << nested << "},\n";
  out << nested << "\"records\": "
      << record_array_json(plan.records, indent + 2, trace_record_json)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string sampling_section_json(
    const otel_sampling_budget_plan::Plan& plan, const int indent) {
  otel_sampling_budget_plan::validate_plan(plan);
  const auto& max_risk = plan.records[plan.summary.max_risk_index];
  const auto& max_storage = plan.records[plan.summary.max_storage_index];
  const auto& max_collector = plan.records[plan.summary.max_collector_index];
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"summary\": {\n";
  out << nested << "  \"max_backpressure_risk_scenario\": "
      << inferlab_plan::json_string(max_risk.scenario.name) << ",\n";
  out << nested << "  \"max_backpressure_risk_score\": "
      << inferlab_plan::fixed6(max_risk.backpressure_risk_score) << ",\n";
  out << nested << "  \"max_collector_utilization\": "
      << inferlab_plan::fixed6(max_collector.collector_utilization) << ",\n";
  out << nested << "  \"max_daily_storage_gb\": "
      << inferlab_plan::fixed6(max_storage.daily_storage_gb) << ",\n";
  out << nested << "  \"scenario_count\": " << plan.records.size()
      << "\n";
  out << nested << "},\n";
  out << nested << "\"records\": "
      << record_array_json(plan.records, indent + 2, sampling_record_json)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string drill_section_json(const failure_drill::Plan& plan,
                               const int indent) {
  failure_drill::validate_plan(plan);
  const auto& highest = plan.records[plan.highest_severity_record_index];
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"assumptions\": {\n";
  out << nested << "  \"profile_name\": "
      << inferlab_plan::json_string(plan.assumptions.profile_name) << ",\n";
  out << nested << "  \"availability_target\": "
      << inferlab_plan::fixed6(plan.assumptions.availability_target)
      << ",\n";
  out << nested << "  \"error_budget_window_seconds\": "
      << inferlab_plan::fixed6(
             plan.assumptions.error_budget_window_seconds)
      << ",\n";
  out << nested << "  \"max_planned_blast_radius_ratio\": "
      << inferlab_plan::fixed6(
             plan.assumptions.max_planned_blast_radius_ratio)
      << ",\n";
  out << nested << "  \"max_recovery_time_seconds\": "
      << inferlab_plan::fixed6(plan.assumptions.max_recovery_time_seconds)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"summary\": {\n";
  out << nested << "  \"decision\": "
      << inferlab_plan::json_string(
             failure_drill::plan_decision_name(plan.decision))
      << ",\n";
  out << nested << "  \"scenario_count\": " << plan.records.size()
      << ",\n";
  out << nested << "  \"critical_scenario_count\": "
      << plan.critical_scenario_count << ",\n";
  out << nested << "  \"rollback_required_count\": "
      << plan.rollback_required_count << ",\n";
  out << nested << "  \"blast_radius_budget_exceeded_count\": "
      << plan.blast_radius_budget_exceeded_count << ",\n";
  out << nested << "  \"recovery_target_exceeded_count\": "
      << plan.recovery_target_exceeded_count << ",\n";
  out << nested << "  \"highest_severity\": "
      << inferlab_plan::json_string(
             failure_drill::severity_name(highest.severity))
      << ",\n";
  out << nested << "  \"highest_severity_scenario\": "
      << inferlab_plan::json_string(highest.scenario.name) << ",\n";
  out << nested << "  \"average_recovery_time_seconds\": "
      << inferlab_plan::fixed6(plan.average_recovery_time_seconds) << ",\n";
  out << nested << "  \"max_bad_requests\": "
      << inferlab_plan::fixed6(plan.max_bad_requests) << ",\n";
  out << nested << "  \"max_blast_radius_ratio\": "
      << inferlab_plan::fixed6(plan.max_blast_radius_ratio) << ",\n";
  out << nested << "  \"max_error_budget_burn_multiple\": "
      << inferlab_plan::fixed6(plan.max_error_budget_burn_multiple) << "\n";
  out << nested << "},\n";
  out << nested << "\"scenarios\": "
      << record_array_json(plan.records, indent + 2, drill_record_json)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(
    const observability_slo_plan::Plan& slo,
    const otel_trace_plan::Plan& trace,
    const otel_sampling_budget_plan::Plan& sampling,
    const failure_drill::Plan& drill) {
  observability_slo_plan::validate_plan(slo);
  otel_trace_plan::validate_plan(trace);
  otel_sampling_budget_plan::validate_plan(sampling);
  failure_drill::validate_plan(drill);
  std::size_t scenario_count = inferlab::internal::checked_add_size(
      slo.records.size(), trace.records.size(),
      "observability evidence scenario count");
  scenario_count = inferlab::internal::checked_add_size(
      scenario_count, sampling.records.size(),
      "observability evidence scenario count");
  scenario_count = inferlab::internal::checked_add_size(
      scenario_count, drill.records.size(),
      "observability evidence scenario count");

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope, product_alignment(), kTargetName,
      kArtifactPath, kRefreshCommand);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"metric_evidence_level\": \"modeled_not_measured\",\n";
  out << "  \"collector_observed\": false,\n";
  out << "  \"fault_injection_executed\": false,\n";
  out << "  \"runbook_executed\": false,\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"section_count\": 4,\n";
  out << "    \"modeled_scenario_count\": " << scenario_count << ",\n";
  out << "    \"sections\": "
      << inferlab_plan::json_string_array(
             {"slo", "otel_trace", "sampling_budget", "failure_drill"}, 4)
      << "\n";
  out << "  },\n";
  out << "  \"slo\": " << slo_section_json(slo, 2) << ",\n";
  out << "  \"otel_trace\": " << trace_section_json(trace, 2) << ",\n";
  out << "  \"sampling_budget\": "
      << sampling_section_json(sampling, 2) << ",\n";
  out << "  \"failure_drill\": " << drill_section_json(drill, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::observability_evidence
