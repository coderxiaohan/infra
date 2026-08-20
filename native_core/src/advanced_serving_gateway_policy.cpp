#include "inferlab/advanced_serving_scheduler.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::finite_unit_interval;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

constexpr double kComparisonTolerance = 1e-9;

const std::array<GatewayRoutingPolicy, 6>& required_gateway_policies() {
  static const std::array<GatewayRoutingPolicy, 6> policies = {
      GatewayRoutingPolicy::round_robin,
      GatewayRoutingPolicy::least_loaded,
      GatewayRoutingPolicy::prefix_lora_aware,
      GatewayRoutingPolicy::slo_rescue_quota,
      GatewayRoutingPolicy::overload_shed,
      GatewayRoutingPolicy::canary_shadow,
  };
  return policies;
}

void require_weight_sum(const std::vector<double>& weights,
                        const char* name) {
  double total = 0.0;
  for (const double weight : weights) {
    require_unit_interval(weight, name);
    total = internal::checked_add_finite_double(total, weight, name);
  }
  if (std::fabs(total - 1.0) > kComparisonTolerance) {
    throw std::invalid_argument(std::string(name) + " must sum to one");
  }
}

void validate_gateway_assumptions(
    const GatewayPolicyMatrixAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "gateway policy assumption profile");
  require_positive_finite(assumptions.queue_slots_per_worker,
                          "gateway queue slots per worker");
  require_positive_finite(assumptions.adapter_slots_per_worker,
                          "gateway adapter slots per worker");
  require_positive_finite(assumptions.tenant_count_normalizer,
                          "gateway tenant-count normalizer");
  require_weight_sum(
      {assumptions.prefix_hit_weight,
       assumptions.prefix_policy_bias_weight},
      "gateway prefix score weights");
  require_weight_sum(
      {assumptions.lora_hit_weight,
       assumptions.lora_capacity_headroom_weight,
       assumptions.lora_policy_bias_weight},
      "gateway LoRA score weights");
  require_weight_sum(
      {assumptions.load_policy_bias_weight,
       assumptions.load_queue_headroom_weight,
       assumptions.load_kv_headroom_weight},
      "gateway load score weights");
  require_weight_sum(
      {assumptions.slo_overrun_risk_weight,
       assumptions.queue_pressure_risk_weight,
       assumptions.kv_pressure_risk_weight,
       assumptions.fairness_gap_risk_weight,
       assumptions.backend_gap_risk_weight,
       assumptions.admission_drop_risk_weight},
      "gateway P99 risk weights");
  require_unit_interval(assumptions.tenant_overage_penalty_weight,
                        "gateway tenant-overage penalty weight");
  require_unit_interval(assumptions.tenant_scale_penalty_weight,
                        "gateway tenant-scale penalty weight");
  require_unit_interval(assumptions.slo_overrun_rescue_weight,
                        "gateway SLO-overrun rescue weight");
  require_unit_interval(assumptions.backend_support_rescue_weight,
                        "gateway backend-support rescue weight");
  require_positive_finite(assumptions.overload_shed_divisor,
                          "gateway overload-shed divisor");
  require_unit_interval(assumptions.non_overload_quota_drop_weight,
                        "gateway non-overload drop weight");
  require_non_negative_finite(
      assumptions.prompt_locality_ttft_saving_ms_per_token,
      "gateway prompt-locality TTFT saving coefficient");
  require_non_negative_finite(
      assumptions.lora_affinity_ttft_saving_ms,
      "gateway LoRA-affinity TTFT saving coefficient");
  require_non_negative_finite(
      assumptions.kv_overpressure_ttft_penalty_ms,
      "gateway KV-overpressure TTFT penalty");
  require_non_negative_finite(
      assumptions.queue_imbalance_ttft_penalty_ms,
      "gateway queue TTFT penalty");
  require_non_negative_finite(
      assumptions.base_control_plane_overhead_ms,
      "gateway base control-plane overhead");
  require_non_negative_finite(
      assumptions.tenant_lookup_overhead_ms,
      "gateway tenant lookup overhead");
  require_non_negative_finite(
      assumptions.adapter_lookup_overhead_ms,
      "gateway adapter lookup overhead");
  require_unit_interval(assumptions.admission_drop_decision_threshold,
                        "gateway admission-drop decision threshold");
  require_unit_interval(assumptions.slo_rescue_decision_threshold,
                        "gateway SLO-rescue decision threshold");
  require_unit_interval(assumptions.prefix_locality_decision_threshold,
                        "gateway prefix decision threshold");
  require_unit_interval(assumptions.load_balance_decision_threshold,
                        "gateway load decision threshold");
  require_unit_interval(assumptions.p99_risk_recommendation_threshold,
                        "gateway P99 recommendation threshold");
  require_unit_interval(assumptions.prefix_locality_recommendation_floor,
                        "gateway prefix recommendation floor");
  require_unit_interval(assumptions.lora_affinity_recommendation_floor,
                        "gateway LoRA recommendation floor");
}

void validate_gateway_policy_profile(const GatewayPolicyProfile& profile) {
  gateway_routing_policy_name(profile.policy);
  require_unit_interval(profile.cache_bias,
                        "gateway policy cache bias");
  require_unit_interval(profile.load_bias,
                        "gateway policy load bias");
  require_unit_interval(profile.slo_rescue_base,
                        "gateway policy SLO-rescue base");
  require_unit_interval(profile.tenant_fairness_bonus,
                        "gateway policy fairness bonus");
  require_non_negative_finite(
      profile.extra_control_plane_overhead_ms,
      "gateway policy extra control-plane overhead");
}

const GatewayPolicyProfile& require_gateway_policy_profile(
    const GatewayRoutingPolicy policy) {
  const auto& profiles = gateway_policy_profiles();
  const auto found = std::find_if(
      profiles.begin(), profiles.end(),
      [policy](const GatewayPolicyProfile& profile) {
        return profile.policy == policy;
      });
  if (found == profiles.end()) {
    throw std::logic_error("gateway policy profile is missing");
  }
  return *found;
}

GatewayPolicyScenario validate_gateway_scenario(
    GatewayPolicyScenario scenario) {
  require_nonempty(scenario.name, "gateway scenario name");
  gateway_routing_policy_name(scenario.policy);
  if (scenario.tenant_count < 0 || scenario.worker_count < 0 ||
      scenario.prompt_tokens < 0 || scenario.output_tokens < 0 ||
      scenario.hot_adapter_count < 0) {
    throw std::invalid_argument(
        "gateway scenario counts and tokens must be non-negative");
  }
  internal::checked_add_int(scenario.prompt_tokens,
                            scenario.output_tokens,
                            "gateway request tokens");
  scenario.request_qps = require_non_negative_finite(
      scenario.request_qps, "gateway request QPS");
  scenario.prefix_hit_rate = require_unit_interval(
      scenario.prefix_hit_rate, "gateway prefix hit rate");
  scenario.lora_cache_hit_rate = require_unit_interval(
      scenario.lora_cache_hit_rate, "gateway LoRA cache hit rate");
  scenario.queue_depth = require_non_negative_finite(
      scenario.queue_depth, "gateway queue depth");
  scenario.kv_pressure = require_non_negative_finite(
      scenario.kv_pressure, "gateway KV pressure");
  scenario.tenant_quota_utilization = require_non_negative_finite(
      scenario.tenant_quota_utilization,
      "gateway tenant quota utilization");
  scenario.compiled_ttft_ms = require_non_negative_finite(
      scenario.compiled_ttft_ms, "gateway compiled TTFT input");
  scenario.p99_ttft_slo_ms = require_positive_finite(
      scenario.p99_ttft_slo_ms, "gateway P99 TTFT SLO");
  scenario.backend_support_score = require_unit_interval(
      scenario.backend_support_score,
      "gateway compiled backend-support score");

  const bool active_workload = scenario.request_qps > 0.0;
  if (active_workload &&
      (scenario.worker_count == 0 || scenario.tenant_count == 0)) {
    throw std::invalid_argument(
        "active gateway workload requires workers and tenants");
  }
  if (scenario.worker_count == 0 &&
      (scenario.queue_depth > 0.0 || scenario.hot_adapter_count > 0 ||
       scenario.kv_pressure > 0.0)) {
    throw std::invalid_argument(
        "gateway worker pressure requires worker inventory");
  }
  return scenario;
}

double bounded_derived_score(const double value, const char* name) {
  return finite_unit_interval(value, name);
}

std::vector<std::string> gateway_metrics(
    const GatewayRoutingPolicy policy) {
  std::vector<std::string> metrics = {
      "gateway_route_duration_ms",
      "router_prefix_overlap_tokens",
      "router_worker_queue_depth",
      "router_worker_kv_pressure",
      "tenant_quota_utilization",
      "request_ttft_ms",
      "request_tpot_ms",
  };
  switch (policy) {
    case GatewayRoutingPolicy::prefix_lora_aware:
      metrics.push_back("lora_adapter_cache_hit_rate");
      metrics.push_back("lora_adapter_load_ms");
      break;
    case GatewayRoutingPolicy::slo_rescue_quota:
    case GatewayRoutingPolicy::overload_shed:
      metrics.push_back("slo_violation_budget_burn_rate");
      metrics.push_back("admission_shed_qps");
      break;
    case GatewayRoutingPolicy::canary_shadow:
      metrics.push_back("shadow_mismatch_rate");
      metrics.push_back("canary_traffic_fraction");
      break;
    case GatewayRoutingPolicy::round_robin:
    case GatewayRoutingPolicy::least_loaded:
      break;
  }
  require_unique_nonempty_strings(metrics, "gateway required metrics");
  return metrics;
}

std::vector<std::string> gateway_commands(
    const GatewayRoutingPolicy policy) {
  const std::string policy_name = gateway_routing_policy_name(policy);
  std::vector<std::string> commands = {
      "kubectl apply --server-side --dry-run=server -f deploy/kserve_llm_d.example.yaml",
      "curl ${GATEWAY_URL}/v1/chat/completions -H 'x-inferlab-policy: " +
          policy_name + "'",
  };
  if (policy == GatewayRoutingPolicy::prefix_lora_aware) {
    commands.push_back(
        "curl ${GATEWAY_URL}/v1/models/${BASE_MODEL}/adapters --dry-run");
  }
  if (policy == GatewayRoutingPolicy::overload_shed) {
    commands.push_back(
        "promtool query instant ${PROM_URL} inferlab_admission_shed_qps");
  }
  require_unique_nonempty_strings(commands, "gateway migration commands");
  return commands;
}

GatewayRoutingDecision select_gateway_decision(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const GatewayPolicyResult& result) {
  if (result.admission_drop_rate >
      assumptions.admission_drop_decision_threshold) {
    return GatewayRoutingDecision::
        shed_or_defer_low_priority_requests;
  }
  if (result.slo_rescue_score >
      assumptions.slo_rescue_decision_threshold) {
    return GatewayRoutingDecision::route_slo_risk_to_rescue_pool;
  }
  if (result.prefix_locality_score >
      assumptions.prefix_locality_decision_threshold) {
    return GatewayRoutingDecision::prefer_prefix_lora_local_worker;
  }
  if (result.load_balance_score >
      assumptions.load_balance_decision_threshold) {
    return GatewayRoutingDecision::prefer_least_loaded_worker;
  }
  return GatewayRoutingDecision::route_with_weighted_policy_and_observe;
}

GatewayRoutingRecommendation select_gateway_recommendation(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const GatewayPolicyResult& result) {
  if (result.p99_risk_score >=
      assumptions.p99_risk_recommendation_threshold) {
    return GatewayRoutingRecommendation::
        enable_slo_rescue_pool_and_tenant_shed_guard;
  }
  if (result.prefix_locality_score <
      assumptions.prefix_locality_recommendation_floor) {
    return GatewayRoutingRecommendation::
        add_prefix_index_and_cache_aware_gateway_state;
  }
  if (result.lora_affinity_score <
      assumptions.lora_affinity_recommendation_floor) {
    return GatewayRoutingRecommendation::
        co_locate_hot_adapters_and_prefix_cache;
  }
  return GatewayRoutingRecommendation::
      keep_gateway_policy_and_collect_real_trace_artifacts;
}

}  // namespace

const char* gateway_routing_policy_name(
    const GatewayRoutingPolicy policy) {
  switch (policy) {
    case GatewayRoutingPolicy::round_robin:
      return "round_robin";
    case GatewayRoutingPolicy::least_loaded:
      return "least_loaded";
    case GatewayRoutingPolicy::prefix_lora_aware:
      return "prefix_lora_aware";
    case GatewayRoutingPolicy::slo_rescue_quota:
      return "slo_rescue_quota";
    case GatewayRoutingPolicy::overload_shed:
      return "overload_shed";
    case GatewayRoutingPolicy::canary_shadow:
      return "canary_shadow";
  }
  throw std::invalid_argument("unknown gateway routing policy");
}

const char* gateway_routing_decision_name(
    const GatewayRoutingDecision decision) {
  switch (decision) {
    case GatewayRoutingDecision::shed_or_defer_low_priority_requests:
      return "shed_or_defer_low_priority_requests";
    case GatewayRoutingDecision::route_slo_risk_to_rescue_pool:
      return "route_slo_risk_to_rescue_pool";
    case GatewayRoutingDecision::prefer_prefix_lora_local_worker:
      return "prefer_prefix_lora_local_worker";
    case GatewayRoutingDecision::prefer_least_loaded_worker:
      return "prefer_least_loaded_worker";
    case GatewayRoutingDecision::route_with_weighted_policy_and_observe:
      return "route_with_weighted_policy_and_observe";
  }
  throw std::invalid_argument("unknown gateway routing decision");
}

const char* gateway_routing_recommendation_text(
    const GatewayRoutingRecommendation recommendation) {
  switch (recommendation) {
    case GatewayRoutingRecommendation::
        enable_slo_rescue_pool_and_tenant_shed_guard:
      return "enable_slo_rescue_pool_and_tenant_shed_guard";
    case GatewayRoutingRecommendation::
        add_prefix_index_and_cache_aware_gateway_state:
      return "add_prefix_index_and_cache_aware_gateway_state";
    case GatewayRoutingRecommendation::
        co_locate_hot_adapters_and_prefix_cache:
      return "co_locate_hot_adapters_and_prefix_cache";
    case GatewayRoutingRecommendation::
        keep_gateway_policy_and_collect_real_trace_artifacts:
      return "keep_gateway_policy_and_collect_real_trace_artifacts";
  }
  throw std::invalid_argument("unknown gateway routing recommendation");
}

const std::vector<GatewayPolicyProfile>& gateway_policy_profiles() {
  static const std::vector<GatewayPolicyProfile> profiles = [] {
    std::vector<GatewayPolicyProfile> values = {
        {GatewayRoutingPolicy::round_robin, 0.12, 0.38, 0.16, 0.0,
         0.0, false},
        {GatewayRoutingPolicy::least_loaded, 0.28, 0.92, 0.16, 0.0,
         0.0, false},
        {GatewayRoutingPolicy::prefix_lora_aware, 1.00, 0.66, 0.16,
         0.0, 0.0, false},
        {GatewayRoutingPolicy::slo_rescue_quota, 0.78, 0.82, 0.62,
         0.15, 0.0, false},
        {GatewayRoutingPolicy::overload_shed, 0.12, 0.75, 0.16, 0.0,
         0.0, true},
        {GatewayRoutingPolicy::canary_shadow, 0.55, 0.38, 0.16, 0.0,
         0.55, false},
    };
    if (values.size() != required_gateway_policies().size()) {
      throw std::logic_error("gateway policy profile coverage is incomplete");
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
      validate_gateway_policy_profile(values[index]);
      if (values[index].policy != required_gateway_policies()[index]) {
        throw std::logic_error("gateway policy profile order drifted");
      }
      const auto duplicate = std::find_if(
          values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index),
          [&values, index](const GatewayPolicyProfile& candidate) {
            return candidate.policy == values[index].policy;
          });
      if (duplicate !=
          values.begin() + static_cast<std::ptrdiff_t>(index)) {
        throw std::logic_error("gateway policy profiles must be unique");
      }
    }
    return values;
  }();
  return profiles;
}

GatewayPolicyMatrixAssumptions
default_gateway_policy_matrix_assumptions() {
  GatewayPolicyMatrixAssumptions assumptions;
  assumptions.profile_name = "compiled_gateway_policy_fixture_v1";
  return assumptions;
}

std::vector<GatewayPolicyScenario> default_gateway_policy_scenarios() {
  return {
      {"baseline_round_robin", GatewayRoutingPolicy::round_robin,
       8, 8, 64.0, 1024, 256, 0.18, 0.20, 4, 18.0, 0.55, 0.62,
       780.0, 900.0, 0.72},
      {"least_loaded_no_cache", GatewayRoutingPolicy::least_loaded,
       24, 12, 160.0, 1536, 256, 0.20, 0.28, 12, 32.0, 0.68,
       0.74, 980.0, 900.0, 0.78},
      {"prefix_lora_aware", GatewayRoutingPolicy::prefix_lora_aware,
       48, 16, 220.0, 2048, 384, 0.66, 0.72, 48, 36.0, 0.74,
       0.70, 840.0, 900.0, 0.88},
      {"slo_rescue_quota", GatewayRoutingPolicy::slo_rescue_quota,
       64, 24, 420.0, 3072, 512, 0.54, 0.61, 96, 84.0, 0.92,
       0.96, 1420.0, 900.0, 0.91},
      {"overload_shed", GatewayRoutingPolicy::overload_shed,
       96, 32, 760.0, 4096, 768, 0.42, 0.50, 160, 156.0, 1.18,
       1.12, 2100.0, 900.0, 0.86},
      {"canary_shadow_gateway", GatewayRoutingPolicy::canary_shadow,
       32, 12, 120.0, 1280, 256, 0.48, 0.55, 32, 22.0, 0.58,
       0.58, 720.0, 900.0, 0.82},
  };
}

GatewayPolicyResult estimate_gateway_policy(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const GatewayPolicyScenario& input_scenario) {
  validate_gateway_assumptions(assumptions);
  const GatewayPolicyScenario scenario =
      validate_gateway_scenario(input_scenario);
  const GatewayPolicyProfile profile =
      require_gateway_policy_profile(scenario.policy);

  GatewayPolicyResult result;
  result.scenario = scenario;
  result.policy_profile = profile;
  result.active_workload = scenario.request_qps > 0.0;

  if (scenario.worker_count > 0) {
    const double queue_capacity = finite_product(
        static_cast<double>(scenario.worker_count),
        assumptions.queue_slots_per_worker,
        "gateway queue normalization capacity");
    const double adapter_capacity = finite_product(
        static_cast<double>(scenario.worker_count),
        assumptions.adapter_slots_per_worker,
        "gateway adapter normalization capacity");
    result.queue_pressure = bounded_derived_score(
        scenario.queue_depth / queue_capacity,
        "gateway queue pressure");
    result.adapter_pressure = bounded_derived_score(
        static_cast<double>(scenario.hot_adapter_count) / adapter_capacity,
        "gateway adapter pressure");
  }
  result.slo_overrun_ratio = require_non_negative_finite(
      std::max(0.0,
               scenario.compiled_ttft_ms / scenario.p99_ttft_slo_ms - 1.0),
      "gateway SLO overrun ratio");
  result.tenant_quota_overage = require_non_negative_finite(
      std::max(0.0, scenario.tenant_quota_utilization - 1.0),
      "gateway tenant quota overage");
  result.kv_overage = require_non_negative_finite(
      std::max(0.0, scenario.kv_pressure - 1.0),
      "gateway KV overage");

  result.prefix_locality_score = bounded_derived_score(
      finite_product(scenario.prefix_hit_rate,
                     assumptions.prefix_hit_weight,
                     "gateway prefix hit contribution") +
          finite_product(profile.cache_bias,
                         assumptions.prefix_policy_bias_weight,
                         "gateway prefix policy contribution"),
      "gateway prefix locality score");
  result.lora_affinity_score = bounded_derived_score(
      finite_product(scenario.lora_cache_hit_rate,
                     assumptions.lora_hit_weight,
                     "gateway LoRA hit contribution") +
          finite_product(1.0 - result.adapter_pressure,
                         assumptions.lora_capacity_headroom_weight,
                         "gateway LoRA headroom contribution") +
          finite_product(profile.cache_bias,
                         assumptions.lora_policy_bias_weight,
                         "gateway LoRA policy contribution"),
      "gateway LoRA affinity score");
  result.load_balance_score = bounded_derived_score(
      finite_product(profile.load_bias,
                     assumptions.load_policy_bias_weight,
                     "gateway load policy contribution") +
          finite_product(1.0 - result.queue_pressure,
                         assumptions.load_queue_headroom_weight,
                         "gateway load queue contribution") +
          finite_product(1.0 - std::min(1.0, scenario.kv_pressure),
                         assumptions.load_kv_headroom_weight,
                         "gateway load KV contribution"),
      "gateway load balance score");
  const double tenant_scale_pressure = bounded_derived_score(
      static_cast<double>(scenario.tenant_count) /
          assumptions.tenant_count_normalizer,
      "gateway tenant scale pressure");
  result.tenant_fairness_score = bounded_derived_score(
      1.0 - finite_product(result.tenant_quota_overage,
                           assumptions.tenant_overage_penalty_weight,
                           "gateway quota fairness penalty") -
          finite_product(tenant_scale_pressure,
                         assumptions.tenant_scale_penalty_weight,
                         "gateway scale fairness penalty") +
          profile.tenant_fairness_bonus,
      "gateway tenant fairness score");
  result.slo_rescue_score = bounded_derived_score(
      profile.slo_rescue_base +
          finite_product(std::min(1.0, result.slo_overrun_ratio),
                         assumptions.slo_overrun_rescue_weight,
                         "gateway SLO overrun rescue contribution") +
          finite_product(scenario.backend_support_score,
                         assumptions.backend_support_rescue_weight,
                         "gateway backend rescue contribution"),
      "gateway SLO rescue score");

  if (profile.overload_shedding_enabled) {
    double overload_pressure = internal::checked_add_finite_double(
        result.queue_pressure, result.kv_overage,
        "gateway overload pressure");
    overload_pressure = internal::checked_add_finite_double(
        overload_pressure, result.tenant_quota_overage,
        "gateway overload pressure");
    result.admission_drop_rate = bounded_derived_score(
        overload_pressure / assumptions.overload_shed_divisor,
        "gateway overload admission drop rate");
  } else {
    result.admission_drop_rate = bounded_derived_score(
        finite_product(result.tenant_quota_overage,
                       assumptions.non_overload_quota_drop_weight,
                       "gateway quota admission drop rate"),
        "gateway admission drop rate");
  }
  result.routed_qps = finite_product(
      scenario.request_qps, 1.0 - result.admission_drop_rate,
      "gateway routed QPS");
  result.shed_qps = require_non_negative_finite(
      scenario.request_qps - result.routed_qps,
      "gateway shed QPS");

  double ttft_delta = -finite_product(
      finite_product(static_cast<double>(scenario.prompt_tokens),
                     assumptions.prompt_locality_ttft_saving_ms_per_token,
                     "gateway prompt-locality TTFT coefficient"),
      result.prefix_locality_score,
      "gateway prompt-locality TTFT saving");
  ttft_delta = internal::checked_add_finite_double(
      ttft_delta,
      -finite_product(assumptions.lora_affinity_ttft_saving_ms,
                      result.lora_affinity_score,
                      "gateway LoRA-affinity TTFT saving"),
      "gateway expected TTFT delta");
  ttft_delta = internal::checked_add_finite_double(
      ttft_delta,
      finite_product(assumptions.kv_overpressure_ttft_penalty_ms,
                     result.kv_overage,
                     "gateway KV-overpressure TTFT penalty"),
      "gateway expected TTFT delta");
  result.expected_ttft_delta_ms = internal::checked_add_finite_double(
      ttft_delta,
      finite_product(
          finite_product(assumptions.queue_imbalance_ttft_penalty_ms,
                         scenario.queue_depth,
                         "gateway queue TTFT penalty"),
          1.0 - result.load_balance_score,
          "gateway residual queue TTFT penalty"),
      "gateway expected TTFT delta");

  const int tenant_log_input = internal::checked_add_int(
      scenario.tenant_count, 1, "gateway tenant log input");
  const int adapter_log_input = internal::checked_add_int(
      scenario.hot_adapter_count, 1, "gateway adapter log input");
  double overhead = internal::checked_add_finite_double(
      assumptions.base_control_plane_overhead_ms,
      finite_product(std::log2(static_cast<double>(tenant_log_input)),
                     assumptions.tenant_lookup_overhead_ms,
                     "gateway tenant lookup overhead"),
      "gateway control-plane overhead");
  overhead = internal::checked_add_finite_double(
      overhead,
      finite_product(std::log2(static_cast<double>(adapter_log_input)),
                     assumptions.adapter_lookup_overhead_ms,
                     "gateway adapter lookup overhead"),
      "gateway control-plane overhead");
  result.control_plane_overhead_ms =
      internal::checked_add_finite_double(
          overhead, profile.extra_control_plane_overhead_ms,
          "gateway control-plane overhead");

  double risk = finite_product(
      std::min(1.0, result.slo_overrun_ratio),
      assumptions.slo_overrun_risk_weight,
      "gateway SLO risk contribution");
  risk = internal::checked_add_finite_double(
      risk,
      finite_product(result.queue_pressure,
                     assumptions.queue_pressure_risk_weight,
                     "gateway queue risk contribution"),
      "gateway P99 risk score");
  risk = internal::checked_add_finite_double(
      risk,
      finite_product(std::min(1.0, scenario.kv_pressure),
                     assumptions.kv_pressure_risk_weight,
                     "gateway KV risk contribution"),
      "gateway P99 risk score");
  risk = internal::checked_add_finite_double(
      risk,
      finite_product(1.0 - result.tenant_fairness_score,
                     assumptions.fairness_gap_risk_weight,
                     "gateway fairness risk contribution"),
      "gateway P99 risk score");
  risk = internal::checked_add_finite_double(
      risk,
      finite_product(1.0 - scenario.backend_support_score,
                     assumptions.backend_gap_risk_weight,
                     "gateway backend risk contribution"),
      "gateway P99 risk score");
  result.p99_risk_score = bounded_derived_score(
      internal::checked_add_finite_double(
          risk,
          finite_product(result.admission_drop_rate,
                         assumptions.admission_drop_risk_weight,
                         "gateway admission-drop risk contribution"),
          "gateway P99 risk score"),
      "gateway P99 risk score");

  result.decision = select_gateway_decision(assumptions, result);
  result.required_metrics = gateway_metrics(scenario.policy);
  result.migration_commands = gateway_commands(scenario.policy);
  result.recommendation =
      select_gateway_recommendation(assumptions, result);
  return result;
}

GatewayPolicyMatrixPlan build_gateway_policy_matrix_plan(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const std::vector<GatewayPolicyScenario>& scenarios) {
  validate_gateway_assumptions(assumptions);
  const auto& profiles = gateway_policy_profiles();
  if (scenarios.size() != profiles.size()) {
    throw std::invalid_argument(
        "gateway policy matrix requires exact canonical policy coverage");
  }

  GatewayPolicyMatrixPlan plan;
  plan.assumptions = assumptions;
  plan.routing_key_fields = {
      "base_model_id", "adapter_id", "tokenizer_template_hash",
      "prefix_hash", "tenant_tier"};
  plan.routing_key_policy_expression =
      "base_model_id + adapter_id + tokenizer_template_hash + prefix_hash + tenant_tier";
  require_unique_nonempty_strings(plan.routing_key_fields,
                                  "gateway routing-key fields");
  plan.records.reserve(scenarios.size());
  bool has_comparable_record = false;

  for (const auto& scenario : scenarios) {
    const auto duplicate_name = std::find_if(
        plan.records.begin(), plan.records.end(),
        [&scenario](const GatewayPolicyResult& existing) {
          return existing.scenario.name == scenario.name;
        });
    const auto duplicate_policy = std::find_if(
        plan.records.begin(), plan.records.end(),
        [&scenario](const GatewayPolicyResult& existing) {
          return existing.scenario.policy == scenario.policy;
        });
    if (duplicate_name != plan.records.end() ||
        duplicate_policy != plan.records.end()) {
      throw std::invalid_argument(
          "gateway matrix scenario names and policies must be unique");
    }

    GatewayPolicyResult result = estimate_gateway_policy(assumptions, scenario);
    const std::size_t result_index = plan.records.size();
    plan.records.push_back(std::move(result));
    const auto& appended = plan.records.back();
    if (!appended.active_workload) {
      continue;
    }
    plan.active_record_count = internal::checked_add_size(
        plan.active_record_count, 1U,
        "gateway active record count");
    if (!has_comparable_record ||
        appended.p99_risk_score < plan.lowest_p99_risk_score) {
      plan.safest_record_index = result_index;
      plan.lowest_p99_risk_score = appended.p99_risk_score;
    }
    if (!has_comparable_record ||
        appended.prefix_locality_score >
            plan.records[plan.best_prefix_locality_record_index]
                .prefix_locality_score) {
      plan.best_prefix_locality_record_index = result_index;
    }
    has_comparable_record = true;
  }
  if (!has_comparable_record ||
      plan.active_record_count > plan.records.size()) {
    throw std::invalid_argument(
        "gateway policy matrix requires an active comparable scenario");
  }
  for (const auto policy : required_gateway_policies()) {
    const auto found = std::find_if(
        plan.records.begin(), plan.records.end(),
        [policy](const GatewayPolicyResult& result) {
          return result.scenario.policy == policy;
        });
    if (found == plan.records.end()) {
      throw std::logic_error("gateway policy coverage drifted");
    }
  }
  return plan;
}

}  // namespace inferlab
