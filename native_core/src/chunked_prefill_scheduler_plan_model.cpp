#include "chunked_prefill_scheduler_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace inferlab::chunked_prefill_scheduler_plan {
namespace {

bool same_scenario(const ChunkedPrefillSchedulerScenario& lhs,
                   const ChunkedPrefillSchedulerScenario& rhs) {
  return lhs.name == rhs.name && lhs.backend == rhs.backend &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.chunk_size == rhs.chunk_size &&
         lhs.max_prefill_batch_tokens == rhs.max_prefill_batch_tokens &&
         lhs.concurrent_decode_streams == rhs.concurrent_decode_streams &&
         lhs.prefix_cache_hit_rate == rhs.prefix_cache_hit_rate &&
         lhs.prefill_ms_per_token == rhs.prefill_ms_per_token &&
         lhs.decode_ms_per_token == rhs.decode_ms_per_token &&
         lhs.scheduler_tick_ms == rhs.scheduler_tick_ms &&
         lhs.chunk_launch_overhead_ms == rhs.chunk_launch_overhead_ms &&
         lhs.kv_bytes_per_token == rhs.kv_bytes_per_token &&
         lhs.kv_budget_gb == rhs.kv_budget_gb;
}

bool same_result(const ChunkedPrefillSchedulerResult& lhs,
                 const ChunkedPrefillSchedulerResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.effective_prompt_tokens == rhs.effective_prompt_tokens &&
         lhs.chunk_count == rhs.chunk_count &&
         lhs.prefill_waves == rhs.prefill_waves &&
         lhs.first_chunk_ttft_ms == rhs.first_chunk_ttft_ms &&
         lhs.total_prefill_ms == rhs.total_prefill_ms &&
         lhs.scheduler_gap_ms == rhs.scheduler_gap_ms &&
         lhs.decode_stall_ms == rhs.decode_stall_ms &&
         lhs.prefix_cache_saved_ms == rhs.prefix_cache_saved_ms &&
         lhs.kv_cache_gb == rhs.kv_cache_gb &&
         lhs.kv_budget_utilization == rhs.kv_budget_utilization &&
         lhs.throughput_efficiency == rhs.throughput_efficiency &&
         lhs.long_context_pressure_score ==
             rhs.long_context_pressure_score &&
         lhs.sarathi_stall_free_score == rhs.sarathi_stall_free_score &&
         lhs.decode_maximal_batch_size == rhs.decode_maximal_batch_size &&
         lhs.prefill_chunk_timeline == rhs.prefill_chunk_timeline &&
         lhs.decode_starvation_risk == rhs.decode_starvation_risk &&
         lhs.jain_fairness_estimate == rhs.jain_fairness_estimate &&
         lhs.p99_risk_score == rhs.p99_risk_score &&
         lhs.chunk_policy == rhs.chunk_policy &&
         lhs.recommendation == rhs.recommendation;
}

void validate_scenarios(
    const std::vector<ChunkedPrefillSchedulerScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "chunked prefill scheduler scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() || scenario.backend.empty() ||
        !names.insert(scenario.name).second) {
      throw std::logic_error(
          "chunked prefill scenario identity is invalid");
    }
    (void)estimate_chunked_prefill_scheduler(scenario);
  }
}

template <typename Metric>
std::size_t highest_index(
    const std::vector<ChunkedPrefillSchedulerResult>& records,
    Metric metric) {
  std::size_t best = 0U;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (metric(records[index]) > metric(records[best])) {
      best = index;
    }
  }
  return best;
}

template <typename Metric>
std::size_t lowest_index(
    const std::vector<ChunkedPrefillSchedulerResult>& records,
    Metric metric) {
  std::size_t best = 0U;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (metric(records[index]) < metric(records[best])) {
      best = index;
    }
  }
  return best;
}

Summary make_summary(
    const std::vector<ChunkedPrefillSchedulerResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "chunked prefill scheduler records must not be empty");
  }
  Summary summary;
  summary.best_ttft_index = lowest_index(
      records, [](const auto& record) {
        return record.first_chunk_ttft_ms;
      });
  summary.best_stall_free_index = highest_index(
      records, [](const auto& record) {
        return record.sarathi_stall_free_score;
      });
  summary.max_pressure_index = highest_index(
      records, [](const auto& record) {
        return record.long_context_pressure_score;
      });
  return summary;
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) +
                           " is outside record range");
  }
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_chunked_prefill_scheduler_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(
        estimate_chunked_prefill_scheduler(scenario));
  }
  plan.summary = make_summary(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "chunked prefill scenario and record counts disagree");
  }
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_chunked_prefill_scheduler(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       plan.scenarios[index]) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "chunked prefill record is not canonical");
    }
  }
  const std::size_t count = plan.records.size();
  require_index(plan.summary.best_ttft_index, count,
                "best TTFT index");
  require_index(plan.summary.best_stall_free_index, count,
                "best stall-free index");
  require_index(plan.summary.max_pressure_index, count,
                "maximum pressure index");
  const auto expected = make_summary(plan.records);
  if (plan.summary.best_ttft_index != expected.best_ttft_index ||
      plan.summary.best_stall_free_index !=
          expected.best_stall_free_index ||
      plan.summary.max_pressure_index != expected.max_pressure_index) {
    throw std::logic_error(
        "chunked prefill summary is not canonical");
  }
}

inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_kv_budget::Observation> observations;
  observations.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    const auto& scenario = record.scenario;
    const bool compatible = record.kv_budget_utilization <= 1.0;
    observations.push_back(
        {scenario.name, scenario.kv_budget_gb, scenario.prompt_tokens,
         scenario.output_tokens, scenario.chunk_size, compatible,
         compatible ? "fits_local_kv_budget"
                    : "exceeds_local_kv_budget"});
  }
  return inferlab_release_kv_budget::bind_profile(
      "chunked_prefill_scheduler", observations);
}

}  // namespace inferlab::chunked_prefill_scheduler_plan
