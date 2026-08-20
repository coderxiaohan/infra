#include "failure_drill_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::failure_drill {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "failure drill assumption profile");
  const double availability = require_unit_interval(
      assumptions.availability_target,
      "failure drill availability target");
  if (availability <= 0.0 || availability >= 1.0) {
    throw std::invalid_argument(
        "failure drill availability target must be in (0, 1)");
  }
  require_positive_finite(assumptions.error_budget_window_seconds,
                          "failure drill error-budget window");
  require_unit_interval(assumptions.max_planned_blast_radius_ratio,
                        "failure drill planned blast-radius limit");
  require_positive_finite(assumptions.max_recovery_time_seconds,
                          "failure drill maximum recovery time");
  require_unit_interval(assumptions.rollback_data_loss_risk_threshold,
                        "failure drill rollback data-loss threshold");
  require_unit_interval(assumptions.rollback_error_rate_threshold,
                        "failure drill rollback error-rate threshold");
  require_unit_interval(assumptions.critical_error_budget_burn_threshold,
                        "failure drill critical burn threshold");
  require_unit_interval(assumptions.critical_data_loss_risk_threshold,
                        "failure drill critical data-loss threshold");
  require_positive_finite(assumptions.critical_recovery_seconds_threshold,
                          "failure drill critical recovery threshold");
  require_unit_interval(assumptions.high_error_budget_burn_threshold,
                        "failure drill high burn threshold");
  require_unit_interval(assumptions.high_error_rate_threshold,
                        "failure drill high error-rate threshold");
  require_positive_finite(assumptions.high_recovery_seconds_threshold,
                          "failure drill high recovery threshold");
  require_unit_interval(assumptions.medium_error_budget_burn_threshold,
                        "failure drill medium burn threshold");
  require_positive_finite(
      assumptions.medium_latency_degradation_ms_threshold,
      "failure drill medium latency threshold");

  if (assumptions.critical_error_budget_burn_threshold <
          assumptions.high_error_budget_burn_threshold ||
      assumptions.high_error_budget_burn_threshold <
          assumptions.medium_error_budget_burn_threshold ||
      assumptions.critical_data_loss_risk_threshold <
          assumptions.rollback_data_loss_risk_threshold ||
      assumptions.critical_recovery_seconds_threshold <
          assumptions.high_recovery_seconds_threshold ||
      assumptions.high_error_rate_threshold <
          assumptions.rollback_error_rate_threshold) {
    throw std::invalid_argument(
        "failure drill severity and rollback thresholds are inconsistent");
  }
}

Scenario validated_scenario(Scenario scenario) {
  require_nonempty(scenario.name, "failure drill scenario name");
  layer_name(scenario.layer);
  require_nonempty(scenario.component, "failure drill component");
  require_nonempty(scenario.trigger, "failure drill trigger");
  scenario.request_qps = require_positive_finite(
      scenario.request_qps, "failure drill request QPS");
  if (scenario.total_replicas <= 0 || scenario.affected_replicas <= 0 ||
      scenario.affected_replicas > scenario.total_replicas) {
    throw std::invalid_argument(
        "failure drill affected replicas must be within positive inventory");
  }
  scenario.detection_seconds = require_non_negative_finite(
      scenario.detection_seconds, "failure drill detection time");
  scenario.mttr_seconds = require_non_negative_finite(
      scenario.mttr_seconds, "failure drill MTTR");
  scenario.rollback_seconds = require_non_negative_finite(
      scenario.rollback_seconds, "failure drill rollback time");
  scenario.error_rate_spike = require_unit_interval(
      scenario.error_rate_spike, "failure drill error-rate spike");
  scenario.latency_spike_ms = require_non_negative_finite(
      scenario.latency_spike_ms, "failure drill latency spike");
  scenario.data_loss_risk = require_unit_interval(
      scenario.data_loss_risk, "failure drill data-loss risk");
  return scenario;
}

std::vector<std::string> signals_for_layer(const Layer layer) {
  switch (layer) {
    case Layer::kv_cache:
      return {"kv_fetch_latency_ms", "kv_transfer_timeout_count",
              "prefix_cache_hit_ratio", "decode_stall_ms"};
    case Layer::engine_runtime:
      return {"gpu_memory_reserved_bytes", "kv_pages_free",
              "cuda_oom_count", "ttft_ms"};
    case Layer::scheduler:
      return {"queue_depth", "replica_inflight_tokens",
              "prefix_hit_ratio", "ttft_ms"};
    case Layer::observability:
      return {"otel_collector_queue_depth", "dropped_spans",
              "metrics_export_lag_ms", "trace_sample_ratio"};
    case Layer::gateway:
      return {"http_429_rate", "http_5xx_rate", "tenant_limit_hits",
              "route_error_count"};
    case Layer::serving:
      return {"http_5xx_rate", "p99_latency_ms", "active_streams",
              "inflight_requests"};
  }
  throw std::invalid_argument("unknown failure drill layer");
}

Severity severity_for(const Assumptions& assumptions,
                      const Scenario& scenario,
                      const double normalized_error_budget_burn,
                      const double recovery_time_seconds) {
  if (normalized_error_budget_burn >=
          assumptions.critical_error_budget_burn_threshold ||
      scenario.data_loss_risk >=
          assumptions.critical_data_loss_risk_threshold ||
      recovery_time_seconds >=
          assumptions.critical_recovery_seconds_threshold) {
    return Severity::critical;
  }
  if (normalized_error_budget_burn >=
          assumptions.high_error_budget_burn_threshold ||
      scenario.error_rate_spike >= assumptions.high_error_rate_threshold ||
      recovery_time_seconds >= assumptions.high_recovery_seconds_threshold) {
    return Severity::high;
  }
  if (normalized_error_budget_burn >=
          assumptions.medium_error_budget_burn_threshold ||
      scenario.latency_spike_ms >=
          assumptions.medium_latency_degradation_ms_threshold) {
    return Severity::medium;
  }
  return Severity::low;
}

RunbookPolicy policy_for(const Severity severity,
                         const bool rollback_required) {
  switch (severity) {
    case Severity::critical:
      return RunbookPolicy::freeze_and_fail_over;
    case Severity::high:
      return rollback_required ? RunbookPolicy::drain_and_rollback
                               : RunbookPolicy::enable_remediation;
    case Severity::medium:
      return RunbookPolicy::limit_blast_radius;
    case Severity::low:
      return RunbookPolicy::record_evidence;
  }
  throw std::invalid_argument("unknown failure drill severity");
}

std::vector<std::string> runbook_steps_for(const Scenario& scenario,
                                           const bool rollback_required) {
  std::vector<std::string> steps = {
      "announce drill window and pin baseline SLO metrics",
      "inject trigger: " + scenario.trigger,
      "capture expected metrics and trace exemplars for " +
          scenario.component,
      "verify traffic drain or load shedding keeps blast radius inside plan",
      "write artifact with recovery time, bad request estimate, and owner action",
  };
  if (rollback_required) {
    steps.insert(steps.begin() + 4,
                 "roll back or isolate " + scenario.component +
                     " before reopening traffic");
  }
  require_unique_nonempty_strings(steps, "failure drill runbook steps");
  return steps;
}

int severity_rank(const Severity severity) {
  switch (severity) {
    case Severity::low:
      return 0;
    case Severity::medium:
      return 1;
    case Severity::high:
      return 2;
    case Severity::critical:
      return 3;
  }
  throw std::invalid_argument("unknown failure drill severity");
}

}  // namespace

const char* layer_name(const Layer layer) {
  switch (layer) {
    case Layer::serving:
      return "serving";
    case Layer::kv_cache:
      return "kv_cache";
    case Layer::engine_runtime:
      return "engine_runtime";
    case Layer::scheduler:
      return "scheduler";
    case Layer::observability:
      return "observability";
    case Layer::gateway:
      return "gateway";
  }
  throw std::invalid_argument("unknown failure drill layer");
}

const char* severity_name(const Severity severity) {
  switch (severity) {
    case Severity::low:
      return "low";
    case Severity::medium:
      return "medium";
    case Severity::high:
      return "high";
    case Severity::critical:
      return "critical";
  }
  throw std::invalid_argument("unknown failure drill severity");
}

const char* runbook_policy_id(const RunbookPolicy policy) {
  switch (policy) {
    case RunbookPolicy::record_evidence:
      return "record_evidence";
    case RunbookPolicy::limit_blast_radius:
      return "limit_blast_radius";
    case RunbookPolicy::enable_remediation:
      return "enable_remediation";
    case RunbookPolicy::drain_and_rollback:
      return "drain_and_rollback";
    case RunbookPolicy::freeze_and_fail_over:
      return "freeze_and_fail_over";
  }
  throw std::invalid_argument("unknown failure drill runbook policy");
}

const char* runbook_policy_text(const RunbookPolicy policy) {
  switch (policy) {
    case RunbookPolicy::record_evidence:
      return "record drill evidence and keep automated remediation enabled";
    case RunbookPolicy::limit_blast_radius:
      return "rate-limit blast radius and watch SLO burn";
    case RunbookPolicy::enable_remediation:
      return "enable auto-remediation and keep incident commander assigned";
    case RunbookPolicy::drain_and_rollback:
      return "drain affected replicas and roll back release";
    case RunbookPolicy::freeze_and_fail_over:
      return "freeze rollout, page serving on-call, fail over traffic";
  }
  throw std::invalid_argument("unknown failure drill runbook policy");
}

const char* plan_decision_name(const PlanDecision decision) {
  switch (decision) {
    case PlanDecision::dry_run_plan_ready:
      return "dry_run_plan_ready";
    case PlanDecision::target_drill_required:
      return "target_drill_required";
    case PlanDecision::target_critical_drill_required:
      return "target_critical_drill_required";
  }
  throw std::invalid_argument("unknown failure drill plan decision");
}

Assumptions default_assumptions() {
  return {
      "compiled_failure_drill_analytical_fixture_v1",
      0.99,
      3600.0,
      0.35,
      300.0,
      0.10,
      0.40,
      0.70,
      0.20,
      900.0,
      0.35,
      0.45,
      300.0,
      0.15,
      300.0,
  };
}

std::vector<Scenario> default_scenarios() {
  return {
      {"decode_worker_crash", Layer::serving, "decode_worker_pool",
       "kill one decode worker while streaming requests are in flight", 420.0,
       1, 8, 8.0, 95.0, 45.0, 0.42, 380.0, 0.00, true},
      {"kv_remote_store_timeout", Layer::kv_cache, "remote_kv_store",
       "inject KV transfer timeout during prefill/decode disaggregation",
       360.0, 2, 8, 12.0, 180.0, 90.0, 0.36, 520.0, 0.04, false},
      {"gpu_oom_long_context", Layer::engine_runtime,
       "prefill_worker_pool",
       "send long-context traffic above configured KV and activation budget",
       210.0, 2, 6, 6.0, 240.0, 120.0, 0.55, 910.0, 0.02, false},
      {"router_hotspot_prefix_miss", Layer::scheduler, "kv_aware_router",
       "force prefix-hash skew so one replica loses locality and saturates",
       500.0, 3, 10, 10.0, 150.0, 60.0, 0.28, 430.0, 0.00, true},
      {"otel_collector_backpressure", Layer::observability,
       "otel_collector",
       "limit collector export bandwidth and verify sampling degradation",
       430.0, 10, 10, 20.0, 120.0, 0.0, 0.05, 80.0, 0.00, true},
      {"gateway_rate_limit_misconfig", Layer::gateway,
       "openai_api_gateway",
       "lower tenant rate limit and verify fail-open/fail-closed policy",
       620.0, 4, 12, 5.0, 75.0, 50.0, 0.33, 220.0, 0.00, false},
  };
}

Record analyze_scenario(const Assumptions& assumptions,
                        const Scenario& scenario) {
  validate_assumptions(assumptions);
  const Scenario validated = validated_scenario(scenario);
  const double blast_radius_ratio =
      static_cast<double>(validated.affected_replicas) /
      static_cast<double>(validated.total_replicas);
  require_unit_interval(blast_radius_ratio,
                        "failure drill blast-radius ratio");

  const bool rollback_required =
      !validated.auto_remediation ||
      validated.data_loss_risk >
          assumptions.rollback_data_loss_risk_threshold ||
      validated.error_rate_spike >
          assumptions.rollback_error_rate_threshold;
  double recovery_time_seconds = internal::checked_add_finite_double(
      validated.detection_seconds, validated.mttr_seconds,
      "failure drill detection and MTTR duration");
  if (rollback_required) {
    recovery_time_seconds = internal::checked_add_finite_double(
        recovery_time_seconds, validated.rollback_seconds,
        "failure drill recovery and rollback duration");
  }

  double estimated_bad_requests = finite_product(
      validated.request_qps, recovery_time_seconds,
      "failure drill requests during recovery");
  estimated_bad_requests = finite_product(
      estimated_bad_requests, blast_radius_ratio,
      "failure drill requests inside blast radius");
  estimated_bad_requests = finite_product(
      estimated_bad_requests, validated.error_rate_spike,
      "failure drill bad-request estimate");

  const double error_budget_fraction =
      1.0 - assumptions.availability_target;
  require_positive_finite(error_budget_fraction,
                          "failure drill error-budget fraction");
  double hourly_error_budget_requests = finite_product(
      validated.request_qps, assumptions.error_budget_window_seconds,
      "failure drill requests in error-budget window");
  hourly_error_budget_requests = finite_product(
      hourly_error_budget_requests, error_budget_fraction,
      "failure drill error-budget requests");
  require_positive_finite(hourly_error_budget_requests,
                          "failure drill error-budget requests");
  const double error_budget_burn_multiple =
      estimated_bad_requests / hourly_error_budget_requests;
  require_non_negative_finite(error_budget_burn_multiple,
                              "failure drill error-budget burn multiple");
  const double normalized_error_budget_burn =
      std::min(1.0, error_budget_burn_multiple);
  const double p99_degradation_ms = finite_product(
      validated.latency_spike_ms, blast_radius_ratio,
      "failure drill P99 degradation");

  Record result;
  result.scenario = validated;
  result.blast_radius_ratio = blast_radius_ratio;
  result.recovery_time_seconds = recovery_time_seconds;
  result.estimated_bad_requests = estimated_bad_requests;
  result.hourly_error_budget_requests = hourly_error_budget_requests;
  result.error_budget_burn_multiple = error_budget_burn_multiple;
  result.error_budget_burn = normalized_error_budget_burn;
  result.p99_degradation_ms = p99_degradation_ms;
  result.rollback_required = rollback_required;
  result.blast_radius_budget_exceeded =
      blast_radius_ratio > assumptions.max_planned_blast_radius_ratio;
  result.recovery_target_exceeded =
      recovery_time_seconds > assumptions.max_recovery_time_seconds;
  result.severity = severity_for(
      assumptions, validated, normalized_error_budget_burn,
      recovery_time_seconds);
  result.runbook_policy = policy_for(result.severity, rollback_required);
  result.expected_signals = signals_for_layer(validated.layer);
  require_unique_nonempty_strings(result.expected_signals,
                                  "failure drill expected signals");
  result.runbook_steps =
      runbook_steps_for(validated, rollback_required);
  return result;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "failure drill plan requires at least one scenario");
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.records.reserve(scenarios.size());
  double recovery_sum = 0.0;
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (scenarios[index].name == scenarios[prior].name) {
        throw std::invalid_argument(
            "failure drill scenario names must be unique");
      }
    }
    Record record = analyze_scenario(assumptions, scenarios[index]);
    recovery_sum = internal::checked_add_finite_double(
        recovery_sum, record.recovery_time_seconds,
        "failure drill recovery-time sum");
    plan.max_bad_requests =
        std::max(plan.max_bad_requests, record.estimated_bad_requests);
    plan.max_blast_radius_ratio =
        std::max(plan.max_blast_radius_ratio, record.blast_radius_ratio);
    plan.max_error_budget_burn_multiple = std::max(
        plan.max_error_budget_burn_multiple,
        record.error_budget_burn_multiple);
    if (record.severity == Severity::critical) {
      plan.critical_scenario_count = internal::checked_add_size(
          plan.critical_scenario_count, 1U,
          "failure drill critical scenario count");
    }
    if (record.rollback_required) {
      plan.rollback_required_count = internal::checked_add_size(
          plan.rollback_required_count, 1U,
          "failure drill rollback count");
    }
    if (record.blast_radius_budget_exceeded) {
      plan.blast_radius_budget_exceeded_count = internal::checked_add_size(
          plan.blast_radius_budget_exceeded_count, 1U,
          "failure drill blast-radius violation count");
    }
    if (record.recovery_target_exceeded) {
      plan.recovery_target_exceeded_count = internal::checked_add_size(
          plan.recovery_target_exceeded_count, 1U,
          "failure drill recovery-target violation count");
    }
    if (!plan.records.empty() &&
        severity_rank(record.severity) >
            severity_rank(
                plan.records[plan.highest_severity_record_index].severity)) {
      plan.highest_severity_record_index = plan.records.size();
    }
    plan.records.push_back(std::move(record));
  }

  plan.comparable_record_count = plan.records.size();
  const int scenario_count = internal::checked_int_from_size(
      plan.records.size(), "failure drill scenario count");
  plan.average_recovery_time_seconds =
      recovery_sum / static_cast<double>(scenario_count);
  require_non_negative_finite(plan.average_recovery_time_seconds,
                              "failure drill average recovery time");
  if (plan.critical_scenario_count > 0U) {
    plan.decision = PlanDecision::target_critical_drill_required;
  } else if (plan.rollback_required_count > 0U ||
             plan.blast_radius_budget_exceeded_count > 0U ||
             plan.recovery_target_exceeded_count > 0U) {
    plan.decision = PlanDecision::target_drill_required;
  }
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_assumptions(plan.assumptions);
  if (plan.records.empty() ||
      plan.comparable_record_count != plan.records.size() ||
      plan.critical_scenario_count > plan.records.size() ||
      plan.rollback_required_count > plan.records.size() ||
      plan.blast_radius_budget_exceeded_count > plan.records.size() ||
      plan.recovery_target_exceeded_count > plan.records.size() ||
      plan.highest_severity_record_index >= plan.records.size()) {
    throw std::invalid_argument(
        "failure drill plan accounting is invalid");
  }
  require_non_negative_finite(plan.average_recovery_time_seconds,
                              "failure drill average recovery time");
  require_non_negative_finite(plan.max_bad_requests,
                              "failure drill maximum bad requests");
  require_unit_interval(plan.max_blast_radius_ratio,
                        "failure drill maximum blast radius");
  require_non_negative_finite(plan.max_error_budget_burn_multiple,
                              "failure drill maximum error budget burn");
  plan_decision_name(plan.decision);
  std::size_t critical_count = 0U;
  std::size_t rollback_count = 0U;
  std::size_t blast_radius_exceeded_count = 0U;
  std::size_t recovery_exceeded_count = 0U;
  std::size_t highest_severity_index = 0U;
  double recovery_sum = 0.0;
  double max_bad_requests = 0.0;
  double max_blast_radius_ratio = 0.0;
  double max_error_budget_burn_multiple = 0.0;
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto& stored = plan.records[index];
    const Record expected = analyze_scenario(plan.assumptions, stored.scenario);
    if (stored.scenario.name.empty() || stored.scenario.component.empty() ||
        stored.scenario.trigger.empty() || stored.expected_signals.empty() ||
        stored.runbook_steps.empty() ||
        stored.blast_radius_ratio != expected.blast_radius_ratio ||
        stored.recovery_time_seconds != expected.recovery_time_seconds ||
        stored.estimated_bad_requests != expected.estimated_bad_requests ||
        stored.hourly_error_budget_requests !=
            expected.hourly_error_budget_requests ||
        stored.error_budget_burn_multiple !=
            expected.error_budget_burn_multiple ||
        stored.error_budget_burn != expected.error_budget_burn ||
        stored.p99_degradation_ms != expected.p99_degradation_ms ||
        stored.rollback_required != expected.rollback_required ||
        stored.blast_radius_budget_exceeded !=
            expected.blast_radius_budget_exceeded ||
        stored.recovery_target_exceeded !=
            expected.recovery_target_exceeded ||
        stored.severity != expected.severity ||
        stored.runbook_policy != expected.runbook_policy ||
        stored.expected_signals != expected.expected_signals ||
        stored.runbook_steps != expected.runbook_steps) {
      throw std::invalid_argument(
          "failure drill record is not the canonical analysis");
    }
    recovery_sum = internal::checked_add_finite_double(
        recovery_sum, expected.recovery_time_seconds,
        "failure drill validation recovery sum");
    max_bad_requests =
        std::max(max_bad_requests, expected.estimated_bad_requests);
    max_blast_radius_ratio =
        std::max(max_blast_radius_ratio, expected.blast_radius_ratio);
    max_error_budget_burn_multiple = std::max(
        max_error_budget_burn_multiple,
        expected.error_budget_burn_multiple);
    if (expected.severity == Severity::critical) {
      critical_count = internal::checked_add_size(
          critical_count, 1U, "failure drill validation critical count");
    }
    if (expected.rollback_required) {
      rollback_count = internal::checked_add_size(
          rollback_count, 1U, "failure drill validation rollback count");
    }
    if (expected.blast_radius_budget_exceeded) {
      blast_radius_exceeded_count = internal::checked_add_size(
          blast_radius_exceeded_count, 1U,
          "failure drill validation blast-radius count");
    }
    if (expected.recovery_target_exceeded) {
      recovery_exceeded_count = internal::checked_add_size(
          recovery_exceeded_count, 1U,
          "failure drill validation recovery-target count");
    }
    if (index > 0U &&
        severity_rank(expected.severity) >
            severity_rank(
                plan.records[highest_severity_index].severity)) {
      highest_severity_index = index;
    }
  }
  const int scenario_count = internal::checked_int_from_size(
      plan.records.size(), "failure drill validation scenario count");
  const double average_recovery =
      recovery_sum / static_cast<double>(scenario_count);
  PlanDecision expected_decision = PlanDecision::dry_run_plan_ready;
  if (critical_count > 0U) {
    expected_decision = PlanDecision::target_critical_drill_required;
  } else if (rollback_count > 0U || blast_radius_exceeded_count > 0U ||
             recovery_exceeded_count > 0U) {
    expected_decision = PlanDecision::target_drill_required;
  }
  if (plan.critical_scenario_count != critical_count ||
      plan.rollback_required_count != rollback_count ||
      plan.blast_radius_budget_exceeded_count !=
          blast_radius_exceeded_count ||
      plan.recovery_target_exceeded_count != recovery_exceeded_count ||
      plan.highest_severity_record_index != highest_severity_index ||
      plan.average_recovery_time_seconds != average_recovery ||
      plan.max_bad_requests != max_bad_requests ||
      plan.max_blast_radius_ratio != max_blast_radius_ratio ||
      plan.max_error_budget_burn_multiple !=
          max_error_budget_burn_multiple ||
      plan.decision != expected_decision) {
    throw std::invalid_argument(
        "failure drill summary is not the canonical aggregation");
  }
}

}  // namespace inferlab::failure_drill
