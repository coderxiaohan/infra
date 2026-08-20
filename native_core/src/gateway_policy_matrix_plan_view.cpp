#include "gateway_policy_matrix_plan_view.hpp"

#include "advanced_serving_internal.hpp"
#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::gateway_policy_matrix_plan_view {
namespace {

const std::vector<std::string>& research_basis() {
  static const std::vector<std::string> products = {
      "AIBrix", "vLLM Production Stack", "Ray Serve LLM",
      "KServe llm-d", "NVIDIA Dynamo Router"};
  return products;
}

bool profile_matches(const GatewayPolicyProfile& lhs,
                     const GatewayPolicyProfile& rhs) {
  return lhs.policy == rhs.policy &&
         lhs.cache_bias == rhs.cache_bias &&
         lhs.load_bias == rhs.load_bias &&
         lhs.slo_rescue_base == rhs.slo_rescue_base &&
         lhs.tenant_fairness_bonus == rhs.tenant_fairness_bonus &&
         lhs.extra_control_plane_overhead_ms ==
             rhs.extra_control_plane_overhead_ms &&
         lhs.overload_shedding_enabled == rhs.overload_shedding_enabled;
}

const GatewayPolicyProfile& canonical_profile(
    const GatewayRoutingPolicy policy) {
  for (const auto& profile : gateway_policy_profiles()) {
    if (profile.policy == policy) {
      return profile;
    }
  }
  throw std::logic_error(
      "gateway-policy view canonical profile is missing");
}

template <typename Visitor>
void for_each_numeric_assumption(
    const GatewayPolicyMatrixAssumptions& assumptions,
    Visitor&& visitor) {
  visitor("adapter_lookup_overhead_ms",
          assumptions.adapter_lookup_overhead_ms);
  visitor("adapter_slots_per_worker",
          assumptions.adapter_slots_per_worker);
  visitor("admission_drop_decision_threshold",
          assumptions.admission_drop_decision_threshold);
  visitor("admission_drop_risk_weight",
          assumptions.admission_drop_risk_weight);
  visitor("backend_gap_risk_weight",
          assumptions.backend_gap_risk_weight);
  visitor("backend_support_rescue_weight",
          assumptions.backend_support_rescue_weight);
  visitor("base_control_plane_overhead_ms",
          assumptions.base_control_plane_overhead_ms);
  visitor("fairness_gap_risk_weight",
          assumptions.fairness_gap_risk_weight);
  visitor("kv_overpressure_ttft_penalty_ms",
          assumptions.kv_overpressure_ttft_penalty_ms);
  visitor("kv_pressure_risk_weight",
          assumptions.kv_pressure_risk_weight);
  visitor("load_balance_decision_threshold",
          assumptions.load_balance_decision_threshold);
  visitor("load_kv_headroom_weight",
          assumptions.load_kv_headroom_weight);
  visitor("load_policy_bias_weight",
          assumptions.load_policy_bias_weight);
  visitor("load_queue_headroom_weight",
          assumptions.load_queue_headroom_weight);
  visitor("lora_affinity_recommendation_floor",
          assumptions.lora_affinity_recommendation_floor);
  visitor("lora_affinity_ttft_saving_ms",
          assumptions.lora_affinity_ttft_saving_ms);
  visitor("lora_capacity_headroom_weight",
          assumptions.lora_capacity_headroom_weight);
  visitor("lora_hit_weight", assumptions.lora_hit_weight);
  visitor("lora_policy_bias_weight",
          assumptions.lora_policy_bias_weight);
  visitor("non_overload_quota_drop_weight",
          assumptions.non_overload_quota_drop_weight);
  visitor("overload_shed_divisor",
          assumptions.overload_shed_divisor);
  visitor("p99_risk_recommendation_threshold",
          assumptions.p99_risk_recommendation_threshold);
  visitor("prefix_hit_weight", assumptions.prefix_hit_weight);
  visitor("prefix_locality_decision_threshold",
          assumptions.prefix_locality_decision_threshold);
  visitor("prefix_locality_recommendation_floor",
          assumptions.prefix_locality_recommendation_floor);
  visitor("prefix_policy_bias_weight",
          assumptions.prefix_policy_bias_weight);
  visitor("prompt_locality_ttft_saving_ms_per_token",
          assumptions.prompt_locality_ttft_saving_ms_per_token);
  visitor("queue_imbalance_ttft_penalty_ms",
          assumptions.queue_imbalance_ttft_penalty_ms);
  visitor("queue_pressure_risk_weight",
          assumptions.queue_pressure_risk_weight);
  visitor("queue_slots_per_worker",
          assumptions.queue_slots_per_worker);
  visitor("slo_overrun_rescue_weight",
          assumptions.slo_overrun_rescue_weight);
  visitor("slo_overrun_risk_weight",
          assumptions.slo_overrun_risk_weight);
  visitor("slo_rescue_decision_threshold",
          assumptions.slo_rescue_decision_threshold);
  visitor("tenant_count_normalizer",
          assumptions.tenant_count_normalizer);
  visitor("tenant_lookup_overhead_ms",
          assumptions.tenant_lookup_overhead_ms);
  visitor("tenant_overage_penalty_weight",
          assumptions.tenant_overage_penalty_weight);
  visitor("tenant_scale_penalty_weight",
          assumptions.tenant_scale_penalty_weight);
}

void validate_scenario_for_projection(
    const GatewayPolicyScenario& scenario) {
  using advanced_serving_internal::require_non_negative_finite;
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_positive_finite;
  using advanced_serving_internal::require_unit_interval;

  require_nonempty(scenario.name, "gateway-policy view scenario name");
  gateway_routing_policy_name(scenario.policy);
  if (scenario.tenant_count < 0 || scenario.worker_count < 0 ||
      scenario.prompt_tokens < 0 || scenario.output_tokens < 0 ||
      scenario.hot_adapter_count < 0) {
    throw std::invalid_argument(
        "gateway-policy view requires non-negative scenario counts");
  }
  internal::checked_add_int(scenario.prompt_tokens,
                            scenario.output_tokens,
                            "gateway-policy view request tokens");
  require_non_negative_finite(scenario.request_qps,
                              "gateway-policy view request QPS");
  require_unit_interval(scenario.prefix_hit_rate,
                        "gateway-policy view prefix hit rate");
  require_unit_interval(scenario.lora_cache_hit_rate,
                        "gateway-policy view LoRA hit rate");
  require_non_negative_finite(scenario.queue_depth,
                              "gateway-policy view queue depth");
  require_non_negative_finite(scenario.kv_pressure,
                              "gateway-policy view KV pressure");
  require_non_negative_finite(
      scenario.tenant_quota_utilization,
      "gateway-policy view tenant quota utilization");
  require_non_negative_finite(scenario.compiled_ttft_ms,
                              "gateway-policy view compiled TTFT");
  require_positive_finite(scenario.p99_ttft_slo_ms,
                          "gateway-policy view TTFT SLO");
  require_unit_interval(scenario.backend_support_score,
                        "gateway-policy view backend support");
  if (scenario.request_qps > 0.0 &&
      (scenario.worker_count == 0 || scenario.tenant_count == 0)) {
    throw std::invalid_argument(
        "gateway-policy view active workload requires resources");
  }
}

void validate_result_for_projection(const GatewayPolicyResult& result) {
  using advanced_serving_internal::require_finite;
  using advanced_serving_internal::require_non_negative_finite;
  using advanced_serving_internal::require_unique_nonempty_strings;
  using advanced_serving_internal::require_unit_interval;

  validate_scenario_for_projection(result.scenario);
  const GatewayPolicyProfile& expected =
      canonical_profile(result.scenario.policy);
  if (!profile_matches(result.policy_profile, expected) ||
      result.active_workload != (result.scenario.request_qps > 0.0)) {
    throw std::invalid_argument(
        "gateway-policy view profile or workload parity failed");
  }
  require_unit_interval(result.queue_pressure,
                        "gateway-policy view queue pressure");
  require_unit_interval(result.adapter_pressure,
                        "gateway-policy view adapter pressure");
  require_non_negative_finite(result.slo_overrun_ratio,
                              "gateway-policy view SLO overrun");
  require_non_negative_finite(result.tenant_quota_overage,
                              "gateway-policy view quota overage");
  require_non_negative_finite(result.kv_overage,
                              "gateway-policy view KV overage");
  require_unit_interval(result.prefix_locality_score,
                        "gateway-policy view prefix score");
  require_unit_interval(result.lora_affinity_score,
                        "gateway-policy view LoRA score");
  require_unit_interval(result.load_balance_score,
                        "gateway-policy view load score");
  require_unit_interval(result.tenant_fairness_score,
                        "gateway-policy view fairness score");
  require_unit_interval(result.slo_rescue_score,
                        "gateway-policy view rescue score");
  require_unit_interval(result.admission_drop_rate,
                        "gateway-policy view drop rate");
  require_non_negative_finite(result.routed_qps,
                              "gateway-policy view routed QPS");
  require_non_negative_finite(result.shed_qps,
                              "gateway-policy view shed QPS");
  require_finite(result.expected_ttft_delta_ms,
                 "gateway-policy view TTFT delta");
  require_non_negative_finite(result.control_plane_overhead_ms,
                              "gateway-policy view control overhead");
  require_unit_interval(result.p99_risk_score,
                        "gateway-policy view P99 risk");
  gateway_routing_decision_name(result.decision);
  gateway_routing_recommendation_text(result.recommendation);
  require_unique_nonempty_strings(result.required_metrics,
                                  "gateway-policy view metrics");
  require_unique_nonempty_strings(result.migration_commands,
                                  "gateway-policy view commands");
}

void validate_plan_for_projection(const GatewayPolicyMatrixPlan& plan) {
  using advanced_serving_internal::require_non_negative_finite;
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_unique_nonempty_strings;

  require_nonempty(plan.assumptions.profile_name,
                   "gateway-policy view assumption profile");
  for_each_numeric_assumption(
      plan.assumptions,
      [](const char* field, const double value) {
        advanced_serving_internal::require_finite(value, field);
      });
  require_unique_nonempty_strings(plan.routing_key_fields,
                                  "gateway-policy view routing keys");
  require_nonempty(plan.routing_key_policy_expression,
                   "gateway-policy view routing expression");
  if (plan.records.empty() ||
      plan.records.size() != gateway_policy_profiles().size() ||
      plan.safest_record_index >= plan.records.size() ||
      plan.best_prefix_locality_record_index >= plan.records.size() ||
      plan.active_record_count == 0U ||
      plan.active_record_count > plan.records.size()) {
    throw std::invalid_argument(
        "gateway-policy view requires a complete canonical plan");
  }
  require_non_negative_finite(plan.lowest_p99_risk_score,
                              "gateway-policy view lowest risk");

  std::size_t active_count = 0U;
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& result = plan.records[index];
    validate_result_for_projection(result);
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (plan.records[prior].scenario.name == result.scenario.name ||
          plan.records[prior].scenario.policy == result.scenario.policy) {
        throw std::invalid_argument(
            "gateway-policy view requires unique names and policies");
      }
    }
    if (result.active_workload) {
      active_count = internal::checked_add_size(
          active_count, 1U, "gateway-policy view active count");
    }
  }
  if (active_count != plan.active_record_count ||
      !plan.records[plan.safest_record_index].active_workload ||
      !plan.records[plan.best_prefix_locality_record_index]
           .active_workload ||
      plan.lowest_p99_risk_score !=
          plan.records[plan.safest_record_index].p99_risk_score) {
    throw std::invalid_argument(
        "gateway-policy view summary projection parity failed");
  }
}

std::string profile_json(const GatewayPolicyProfile& profile,
                         const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cache_bias\": "
      << inferlab_plan::fixed6(profile.cache_bias) << ",\n";
  out << nested << "\"extra_control_plane_overhead_ms\": "
      << inferlab_plan::fixed6(
             profile.extra_control_plane_overhead_ms) << ",\n";
  out << nested << "\"load_bias\": "
      << inferlab_plan::fixed6(profile.load_bias) << ",\n";
  out << nested << "\"overload_shedding_enabled\": "
      << inferlab_plan::json_bool(profile.overload_shedding_enabled)
      << ",\n";
  out << nested << "\"policy\": "
      << inferlab_plan::json_string(
             gateway_routing_policy_name(profile.policy)) << ",\n";
  out << nested << "\"slo_rescue_base\": "
      << inferlab_plan::fixed6(profile.slo_rescue_base) << ",\n";
  out << nested << "\"tenant_fairness_bonus\": "
      << inferlab_plan::fixed6(profile.tenant_fairness_bonus) << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const GatewayPolicyScenario& scenario,
                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend_support_score\": "
      << inferlab_plan::fixed6(scenario.backend_support_score) << ",\n";
  out << nested << "\"compiled_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.compiled_ttft_ms) << ",\n";
  out << nested << "\"gateway_policy\": "
      << inferlab_plan::json_string(
             gateway_routing_policy_name(scenario.policy)) << ",\n";
  out << nested << "\"hot_adapter_count\": "
      << scenario.hot_adapter_count << ",\n";
  out << nested << "\"kv_pressure\": "
      << inferlab_plan::fixed6(scenario.kv_pressure) << ",\n";
  out << nested << "\"lora_cache_hit_rate\": "
      << inferlab_plan::fixed6(scenario.lora_cache_hit_rate) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"observed_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.compiled_ttft_ms) << ",\n";
  out << nested << "\"output_tokens\": "
      << scenario.output_tokens << ",\n";
  out << nested << "\"p99_ttft_slo_ms\": "
      << inferlab_plan::fixed6(scenario.p99_ttft_slo_ms) << ",\n";
  out << nested << "\"prefix_hit_rate\": "
      << inferlab_plan::fixed6(scenario.prefix_hit_rate) << ",\n";
  out << nested << "\"prompt_tokens\": "
      << scenario.prompt_tokens << ",\n";
  out << nested << "\"queue_depth\": "
      << inferlab_plan::fixed6(scenario.queue_depth) << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(scenario.request_qps) << ",\n";
  out << nested << "\"tenant_count\": "
      << scenario.tenant_count << ",\n";
  out << nested << "\"tenant_quota_utilization\": "
      << inferlab_plan::fixed6(
             scenario.tenant_quota_utilization) << ",\n";
  out << nested << "\"worker_count\": "
      << scenario.worker_count << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const GatewayPolicyResult& result,
                        const std::string& routing_expression,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"active_workload\": "
      << inferlab_plan::json_bool(result.active_workload) << ",\n";
  out << nested << "\"adapter_pressure\": "
      << inferlab_plan::fixed6(result.adapter_pressure) << ",\n";
  out << nested << "\"admission_drop_rate\": "
      << inferlab_plan::fixed6(result.admission_drop_rate) << ",\n";
  out << nested << "\"control_plane_overhead_ms\": "
      << inferlab_plan::fixed6(result.control_plane_overhead_ms)
      << ",\n";
  out << nested << "\"expected_ttft_delta_ms\": "
      << inferlab_plan::fixed6(result.expected_ttft_delta_ms) << ",\n";
  out << nested << "\"gateway_decision\": "
      << inferlab_plan::json_string(
             gateway_routing_decision_name(result.decision)) << ",\n";
  out << nested << "\"kv_overage\": "
      << inferlab_plan::fixed6(result.kv_overage) << ",\n";
  out << nested << "\"load_balance_score\": "
      << inferlab_plan::fixed6(result.load_balance_score) << ",\n";
  out << nested << "\"lora_affinity_score\": "
      << inferlab_plan::fixed6(result.lora_affinity_score) << ",\n";
  out << nested << "\"migration_commands\": "
      << inferlab_plan::json_string_array(result.migration_commands,
                                           indent + 2) << ",\n";
  out << nested << "\"p99_risk_score\": "
      << inferlab_plan::fixed6(result.p99_risk_score) << ",\n";
  out << nested << "\"policy_profile\": "
      << profile_json(result.policy_profile, indent + 2) << ",\n";
  out << nested << "\"prefix_locality_score\": "
      << inferlab_plan::fixed6(result.prefix_locality_score) << ",\n";
  out << nested << "\"queue_pressure\": "
      << inferlab_plan::fixed6(result.queue_pressure) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             gateway_routing_recommendation_text(
                 result.recommendation)) << ",\n";
  out << nested << "\"required_metrics\": "
      << inferlab_plan::json_string_array(result.required_metrics,
                                           indent + 2) << ",\n";
  out << nested << "\"routed_qps\": "
      << inferlab_plan::fixed6(result.routed_qps) << ",\n";
  out << nested << "\"routing_key_policy\": "
      << inferlab_plan::json_string(routing_expression) << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, indent + 2) << ",\n";
  out << nested << "\"shed_qps\": "
      << inferlab_plan::fixed6(result.shed_qps) << ",\n";
  out << nested << "\"slo_overrun_ratio\": "
      << inferlab_plan::fixed6(result.slo_overrun_ratio) << ",\n";
  out << nested << "\"slo_rescue_score\": "
      << inferlab_plan::fixed6(result.slo_rescue_score) << ",\n";
  out << nested << "\"tenant_fairness_score\": "
      << inferlab_plan::fixed6(result.tenant_fairness_score) << ",\n";
  out << nested << "\"tenant_quota_overage\": "
      << inferlab_plan::fixed6(result.tenant_quota_overage) << "\n";
  out << pad << "}";
  return out.str();
}

std::string assumptions_json(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  for_each_numeric_assumption(
      assumptions,
      [&out, &nested](const char* field, const double value) {
        out << nested << inferlab_plan::json_string(field) << ": "
            << inferlab_plan::fixed6(value) << ",\n";
      });
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const GatewayPolicyMatrixPlan& plan) {
  validate_plan_for_projection(plan);
  const auto& artifact = inferlab_high_value::require_managed_artifact(
      "gateway_policy_matrix_plan");
  const std::string artifact_type =
      inferlab_catalog::registry_artifact_type(artifact);
  const std::string artifact_path =
      inferlab_catalog::raw_artifact_path(artifact.artifact);
  const auto& safest = plan.records[plan.safest_record_index];
  const auto& best_prefix =
      plan.records[plan.best_prefix_locality_record_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_type) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, artifact_type, artifact.claim_boundary, research_basis(),
      artifact.target, artifact_path, artifact.command);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(artifact.claim_boundary) << ",\n";
  out << "  \"canonical_owner\": {\n";
  out << "    \"artifact\": "
      << inferlab_plan::json_string(artifact_path) << ",\n";
  out << "    \"model\": \"inferlab::build_gateway_policy_matrix_plan\",\n";
  out << "    \"refresh_command\": "
      << inferlab_plan::json_string(artifact.command) << ",\n";
  out << "    \"target\": "
      << inferlab_plan::json_string(artifact.target) << ",\n";
  out << "    \"view\": \"inferlab::gateway_policy_matrix_plan_view::artifact_json\"\n";
  out << "  },\n";
  out << "  \"fixture_scope\": {\n";
  out << "    \"backend_support\": \"compiled_assumption_not_observed\",\n";
  out << "    \"config_reference_consumed\": false,\n";
  out << "    \"scenario_source\": \"compiled_defaults\",\n";
  out << "    \"timing\": \"modeled_not_measured\"\n";
  out << "  },\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(research_basis(), 2)
      << ",\n";
  out << "  \"catalog_evidence_contract\": "
      << inferlab_plan::catalog_evidence_contract_json(
             "dry_run_ready", 1U, artifact.claim_boundary,
             artifact.command, 2)
      << ",\n";
  out << "  \"routing_key_contract\": {\n";
  out << "    \"fields\": "
      << inferlab_plan::json_string_array(plan.routing_key_fields, 4)
      << ",\n";
  out << "    \"policy_expression\": "
      << inferlab_plan::json_string(
             plan.routing_key_policy_expression) << "\n";
  out << "  },\n";
  out << "  \"metric_evidence\": {\n";
  out << "    \"expected_ttft_delta_ms\": \"analytical_estimate\",\n";
  out << "    \"legacy_observed_ttft_ms_alias\": \"compiled_ttft_ms; not observed evidence\",\n";
  out << "    \"policy_scores\": \"bounded_analytical_scores\",\n";
  out << "    \"required_metrics\": \"collection_contract_only\"\n";
  out << "  },\n";
  out << "  \"assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"active_scenario_count\": "
      << plan.active_record_count << ",\n";
  out << "    \"best_prefix_locality_scenario\": "
      << inferlab_plan::json_string(best_prefix.scenario.name) << ",\n";
  out << "    \"lowest_p99_risk_scenario\": "
      << inferlab_plan::json_string(safest.scenario.name) << ",\n";
  out << "    \"lowest_p99_risk_score\": "
      << inferlab_plan::fixed6(plan.lowest_p99_risk_score) << ",\n";
  out << "    \"ranking_scope\": \"active compiled scenarios only\",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index],
                       plan.routing_key_policy_expression, 4)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": [\n";
  out << "    \"gateway routing or quota enforcement\",\n";
  out << "    \"product replay or execute\",\n";
  out << "    \"TTFT or routing microbenchmark\",\n";
  out << "    \"migration commands\"\n";
  out << "  ],\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({artifact.command}, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::gateway_policy_matrix_plan_view
