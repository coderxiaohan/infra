#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::failure_drill {

enum class Layer {
  serving,
  kv_cache,
  engine_runtime,
  scheduler,
  observability,
  gateway,
};

enum class Severity {
  low,
  medium,
  high,
  critical,
};

enum class RunbookPolicy {
  record_evidence,
  limit_blast_radius,
  enable_remediation,
  drain_and_rollback,
  freeze_and_fail_over,
};

enum class PlanDecision {
  dry_run_plan_ready,
  target_drill_required,
  target_critical_drill_required,
};

struct Assumptions {
  std::string profile_name;
  double availability_target = 0.0;
  double error_budget_window_seconds = 0.0;
  double max_planned_blast_radius_ratio = 0.0;
  double max_recovery_time_seconds = 0.0;
  double rollback_data_loss_risk_threshold = 0.0;
  double rollback_error_rate_threshold = 0.0;
  double critical_error_budget_burn_threshold = 0.0;
  double critical_data_loss_risk_threshold = 0.0;
  double critical_recovery_seconds_threshold = 0.0;
  double high_error_budget_burn_threshold = 0.0;
  double high_error_rate_threshold = 0.0;
  double high_recovery_seconds_threshold = 0.0;
  double medium_error_budget_burn_threshold = 0.0;
  double medium_latency_degradation_ms_threshold = 0.0;
};

struct Scenario {
  std::string name;
  Layer layer = Layer::serving;
  std::string component;
  std::string trigger;
  double request_qps = 0.0;
  int affected_replicas = 0;
  int total_replicas = 0;
  double detection_seconds = 0.0;
  double mttr_seconds = 0.0;
  double rollback_seconds = 0.0;
  double error_rate_spike = 0.0;
  double latency_spike_ms = 0.0;
  double data_loss_risk = 0.0;
  bool auto_remediation = false;
};

struct Record {
  Scenario scenario;
  double blast_radius_ratio = 0.0;
  double recovery_time_seconds = 0.0;
  double estimated_bad_requests = 0.0;
  double hourly_error_budget_requests = 0.0;
  double error_budget_burn_multiple = 0.0;
  // Legacy normalized field retained for downstream compatibility.
  double error_budget_burn = 0.0;
  double p99_degradation_ms = 0.0;
  bool rollback_required = false;
  bool blast_radius_budget_exceeded = false;
  bool recovery_target_exceeded = false;
  Severity severity = Severity::low;
  RunbookPolicy runbook_policy = RunbookPolicy::record_evidence;
  std::vector<std::string> expected_signals;
  std::vector<std::string> runbook_steps;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Record> records;
  std::size_t comparable_record_count = 0;
  std::size_t critical_scenario_count = 0;
  std::size_t rollback_required_count = 0;
  std::size_t blast_radius_budget_exceeded_count = 0;
  std::size_t recovery_target_exceeded_count = 0;
  std::size_t highest_severity_record_index = 0;
  double average_recovery_time_seconds = 0.0;
  double max_bad_requests = 0.0;
  double max_blast_radius_ratio = 0.0;
  double max_error_budget_burn_multiple = 0.0;
  PlanDecision decision = PlanDecision::dry_run_plan_ready;
};

const char* layer_name(Layer layer);
const char* severity_name(Severity severity);
const char* runbook_policy_id(RunbookPolicy policy);
const char* runbook_policy_text(RunbookPolicy policy);
const char* plan_decision_name(PlanDecision decision);

Assumptions default_assumptions();
std::vector<Scenario> default_scenarios();
Record analyze_scenario(const Assumptions& assumptions,
                        const Scenario& scenario);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios);
void validate_plan(const Plan& plan);

}  // namespace inferlab::failure_drill
