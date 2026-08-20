#include "scheduler_runtime_policy_sweep_model.hpp"

#include "checked_index.hpp"
#include "paged_kv_probe_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::scheduler_runtime_policy {
namespace {

using RequestIndex = std::unordered_map<RequestId, std::size_t>;

struct ResultAggregates {
  int total_preemptions = 0;
  int preempted_request_count = 0;
  int max_preemptions_per_request = 0;
  int total_recompute_tokens = 0;
  int total_backpressure_deferrals = 0;
  int append_pressure_events = 0;
  int trace_preemptions = 0;
};

int blocks_required(const int tokens, const int block_size_tokens) {
  return internal::checked_ceil_div_non_negative_int(
      tokens, block_size_tokens, "runtime policy blocks required");
}

int cached_tokens(const RuntimeRequest& request) {
  return internal::checked_add_int(
      request.request.prompt_tokens, request.generated_tokens,
      "runtime policy cached tokens");
}

int remaining_decode_tokens(const RuntimeRequest& request) {
  if (request.generated_tokens < 0 ||
      request.generated_tokens > request.request.max_decode_tokens) {
    throw std::logic_error(
        "runtime policy generated token state is invalid");
  }
  return request.request.max_decode_tokens - request.generated_tokens;
}

RequestLifecycleMetrics make_metrics(const RuntimeRequest& runtime) {
  return probe::make_scheduler_probe_lifecycle_metrics(
      runtime.request, runtime.state, runtime.submitted_step,
      runtime.generated_tokens, runtime.first_scheduled_step,
      runtime.first_decode_step, runtime.finished_step);
}

std::vector<RequestLifecycleMetrics> collect_request_metrics(
    const std::vector<RuntimeRequest>& requests) {
  std::vector<RequestLifecycleMetrics> metrics;
  metrics.reserve(requests.size());
  for (const auto& request : requests) {
    metrics.push_back(make_metrics(request));
  }
  return metrics;
}

std::vector<RequestLifecycleMetrics> sorted_metrics(
    std::vector<RequestLifecycleMetrics> metrics) {
  return probe::sorted_scheduler_probe_lifecycle_metrics(std::move(metrics));
}

std::vector<SchedulerStepStats> pressure_trace(
    const std::vector<RuntimeStepStats>& trace) {
  std::vector<SchedulerStepStats> pressure;
  pressure.reserve(trace.size());
  for (const auto& step : trace) {
    SchedulerStepStats stats;
    stats.step = step.step;
    stats.admitted = step.admitted;
    stats.decoded = step.decoded;
    stats.finished = step.finished;
    stats.rejected = step.rejected;
    stats.backpressure_deferred = step.backpressure_deferred;
    stats.cache_stats = step.cache_stats;
    pressure.push_back(stats);
  }
  return pressure;
}

void validate_cache_config(const PagedKVCacheConfig& config) {
  if (config.num_blocks <= 0 || config.block_size_tokens <= 0 ||
      config.bytes_per_token <= 0) {
    throw std::invalid_argument(
        "runtime scheduler cache resources must be positive");
  }
  (void)probe::require_paged_kv_probe_capacity(
      config.num_blocks, "runtime scheduler cache block count");
}

void validate_workload(const std::vector<InferenceRequest>& workload) {
  if (workload.empty()) {
    throw std::invalid_argument(
        "runtime scheduler workload must not be empty");
  }
  std::unordered_set<RequestId> request_ids;
  request_ids.reserve(workload.size());
  for (const auto& request : workload) {
    if (request.prompt_tokens < 0 || request.max_decode_tokens < 0) {
      throw std::invalid_argument(
          "runtime scheduler workload tokens must be non-negative");
    }
    if (!request_ids.insert(request.request_id).second) {
      throw std::invalid_argument(
          "runtime scheduler workload request ids must be unique");
    }
  }
}

void validate_policy(const PolicyConfig& policy,
                     const PagedKVCacheConfig& cache_config) {
  if (policy.name.empty() || policy.max_active_requests <= 0 ||
      policy.reserve_free_blocks < 0 ||
      policy.reserve_free_blocks > cache_config.num_blocks) {
    throw std::invalid_argument("runtime scheduler policy is invalid");
  }
}

std::optional<RequestId> choose_preemption_victim(
    const RequestId current_request_id,
    const std::vector<RequestId>& running,
    const std::vector<RuntimeRequest>& requests,
    const RequestIndex& index) {
  std::optional<RequestId> victim;
  for (const auto request_id : running) {
    if (request_id == current_request_id) {
      continue;
    }
    const auto& candidate = probe::scheduler_probe_request_at(
        requests, index, request_id);
    if (candidate.state != RequestState::kRunning) {
      continue;
    }
    if (!victim.has_value()) {
      victim = request_id;
      continue;
    }
    const auto& best = probe::scheduler_probe_request_at(
        requests, index, *victim);
    const auto candidate_remaining = remaining_decode_tokens(candidate);
    const auto best_remaining = remaining_decode_tokens(best);
    if (candidate_remaining > best_remaining ||
        (candidate_remaining == best_remaining &&
         cached_tokens(candidate) > cached_tokens(best))) {
      victim = request_id;
    }
  }
  return victim;
}

void erase_request_id(std::vector<RequestId>& requests,
                      const RequestId request_id) {
  requests.erase(std::remove(requests.begin(), requests.end(), request_id),
                 requests.end());
}

bool should_defer_admission(const PolicyConfig& policy,
                            const PagedKVCacheConfig& cache_config,
                            const AllocationStats& stats,
                            const RuntimeRequest& request) {
  if (!policy.enable_backpressure) {
    return false;
  }
  const int needed_blocks =
      blocks_required(cached_tokens(request), cache_config.block_size_tokens);
  if (needed_blocks > stats.total_blocks || stats.active_requests == 0) {
    return false;
  }
  const int available_without_reserve =
      stats.free_blocks > policy.reserve_free_blocks
          ? stats.free_blocks - policy.reserve_free_blocks
          : 0;
  return needed_blocks > available_without_reserve;
}

ResultAggregates derive_aggregates(
    const std::vector<RuntimeStepStats>& trace,
    const std::vector<RuntimeRequest>& requests) {
  ResultAggregates aggregates;
  int expected_step = 0;
  for (const auto& step : trace) {
    expected_step = internal::checked_add_int(
        expected_step, 1, "runtime policy expected trace step");
    if (step.step != expected_step || step.admitted < 0 || step.decoded < 0 ||
        step.finished < 0 || step.rejected < 0 || step.preempted < 0 ||
        step.backpressure_deferred < 0 ||
        step.append_pressure_events < 0) {
      throw std::logic_error(
          "runtime scheduler trace contains invalid counters");
    }
    probe::validate_paged_kv_probe_stats(
        step.cache_stats, "runtime scheduler trace cache");
    aggregates.total_backpressure_deferrals = internal::checked_add_int(
        aggregates.total_backpressure_deferrals,
        step.backpressure_deferred,
        "runtime policy total backpressure deferrals");
    aggregates.append_pressure_events = internal::checked_add_int(
        aggregates.append_pressure_events, step.append_pressure_events,
        "runtime policy append pressure events");
    aggregates.trace_preemptions = internal::checked_add_int(
        aggregates.trace_preemptions, step.preempted,
        "runtime policy trace preemptions");
  }
  for (const auto& request : requests) {
    if (request.preemptions < 0 || request.recompute_tokens < 0 ||
        request.generated_tokens < 0 ||
        request.generated_tokens > request.request.max_decode_tokens ||
        (request.state != RequestState::kFinished &&
         request.state != RequestState::kRejected)) {
      throw std::logic_error(
          "runtime scheduler request terminal accounting is invalid");
    }
    aggregates.total_preemptions = internal::checked_add_int(
        aggregates.total_preemptions, request.preemptions,
        "runtime policy total preemptions");
    aggregates.total_recompute_tokens = internal::checked_add_int(
        aggregates.total_recompute_tokens, request.recompute_tokens,
        "runtime policy total recompute tokens");
    aggregates.max_preemptions_per_request =
        std::max(aggregates.max_preemptions_per_request,
                 request.preemptions);
    if (request.preemptions > 0) {
      aggregates.preempted_request_count = internal::checked_add_int(
          aggregates.preempted_request_count, 1,
          "runtime policy preempted request count");
    }
  }
  if (aggregates.trace_preemptions >
      aggregates.append_pressure_events) {
    throw std::logic_error(
        "runtime scheduler preemptions exceed append pressure events");
  }
  return aggregates;
}

PolicyResult run_policy(const PolicyConfig& policy,
                        const PagedKVCacheConfig& cache_config,
                        const std::vector<InferenceRequest>& workload) {
  validate_cache_config(cache_config);
  validate_workload(workload);
  validate_policy(policy, cache_config);

  const auto step_budget = probe::scheduler_probe_step_budget(workload);
  PagedKVCacheManager cache(cache_config);
  std::vector<RuntimeRequest> requests;
  RequestIndex index;
  requests.reserve(workload.size());
  index.reserve(workload.size());
  std::deque<RequestId> waiting;
  for (const auto& request : workload) {
    RuntimeRequest runtime;
    runtime.request = request;
    requests.push_back(runtime);
    const auto inserted =
        index.emplace(request.request_id, requests.size() - 1U);
    if (!inserted.second) {
      throw std::logic_error(
          "validated runtime scheduler request id was not unique");
    }
    waiting.push_back(request.request_id);
  }

  std::vector<RequestId> running;
  std::vector<RuntimeStepStats> trace;
  running.reserve(std::min(
      workload.size(),
      static_cast<std::size_t>(policy.max_active_requests)));
  trace.reserve(step_budget);
  int step = 0;

  while (!waiting.empty() || !running.empty()) {
    if (trace.size() >= step_budget) {
      throw std::logic_error(
          "runtime scheduler exceeded derived progress step budget");
    }
    step = internal::checked_add_int(step, 1, "runtime policy step");
    RuntimeStepStats stats;
    stats.step = step;

    const auto max_active_requests =
        static_cast<std::size_t>(policy.max_active_requests);
    while (!waiting.empty() && running.size() < max_active_requests) {
      const auto request_id = waiting.front();
      auto& runtime =
          probe::scheduler_probe_request_at(requests, index, request_id);
      const auto cache_stats = cache.stats();
      const int needed_blocks =
          blocks_required(cached_tokens(runtime),
                          cache_config.block_size_tokens);
      if (needed_blocks > cache_config.num_blocks) {
        waiting.pop_front();
        runtime.state = RequestState::kRejected;
        runtime.finished_step = step;
        stats.rejected = internal::checked_add_int(
            stats.rejected, 1, "runtime policy rejected count");
        continue;
      }
      if (should_defer_admission(
              policy, cache_config, cache_stats, runtime)) {
        stats.backpressure_deferred = internal::checked_add_int(
            stats.backpressure_deferred, 1,
            "runtime policy backpressure count");
        break;
      }

      waiting.pop_front();
      if (!cache.add_request(request_id, cached_tokens(runtime))) {
        runtime.state = RequestState::kRejected;
        runtime.finished_step = step;
        stats.rejected = internal::checked_add_int(
            stats.rejected, 1, "runtime policy rejected count");
        continue;
      }
      runtime.state = RequestState::kRunning;
      if (runtime.first_scheduled_step < 0) {
        runtime.first_scheduled_step = step;
      }
      running.push_back(request_id);
      stats.admitted = internal::checked_add_int(
          stats.admitted, 1, "runtime policy admitted count");
    }

    std::vector<RequestId> still_running;
    still_running.reserve(running.size());
    for (const auto request_id : running) {
      auto& runtime =
          probe::scheduler_probe_request_at(requests, index, request_id);
      if (runtime.state != RequestState::kRunning) {
        continue;
      }
      if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
        probe::release_scheduler_probe_request(
            cache, request_id, "runtime scheduler completion");
        runtime.state = RequestState::kFinished;
        runtime.finished_step = step;
        stats.finished = internal::checked_add_int(
            stats.finished, 1, "runtime policy finished count");
        continue;
      }

      bool appended = cache.append_tokens(request_id, 1);
      if (!appended) {
        stats.append_pressure_events = internal::checked_add_int(
            stats.append_pressure_events, 1,
            "runtime policy append pressure count");
      }
      if (!appended && policy.preempt_on_append_pressure) {
        const auto victim =
            choose_preemption_victim(request_id, running, requests, index);
        if (victim.has_value()) {
          auto& victim_runtime = probe::scheduler_probe_request_at(
              requests, index, *victim);
          const int next_preemptions = internal::checked_add_int(
              victim_runtime.preemptions, 1,
              "runtime policy request preemptions");
          const int next_recompute_tokens = internal::checked_add_int(
              victim_runtime.recompute_tokens,
              cached_tokens(victim_runtime),
              "runtime policy recompute tokens");
          waiting.push_back(*victim);
          try {
            probe::release_scheduler_probe_request(
                cache, *victim, "runtime scheduler preemption");
          } catch (...) {
            waiting.pop_back();
            throw;
          }
          victim_runtime.state = RequestState::kWaiting;
          victim_runtime.preemptions = next_preemptions;
          victim_runtime.recompute_tokens = next_recompute_tokens;
          erase_request_id(still_running, *victim);
          stats.preempted = internal::checked_add_int(
              stats.preempted, 1, "runtime policy preempted count");
          appended = cache.append_tokens(request_id, 1);
        }
      }

      if (!appended) {
        probe::release_scheduler_probe_request(
            cache, request_id, "runtime scheduler append rejection");
        runtime.state = RequestState::kRejected;
        runtime.finished_step = step;
        stats.rejected = internal::checked_add_int(
            stats.rejected, 1, "runtime policy rejected count");
        continue;
      }

      runtime.generated_tokens = internal::checked_add_int(
          runtime.generated_tokens, 1,
          "runtime policy generated tokens");
      if (runtime.first_decode_step < 0) {
        runtime.first_decode_step = step;
      }
      stats.decoded = internal::checked_add_int(
          stats.decoded, 1, "runtime policy decoded count");

      if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
        probe::release_scheduler_probe_request(
            cache, request_id, "runtime scheduler decode completion");
        runtime.state = RequestState::kFinished;
        runtime.finished_step = step;
        stats.finished = internal::checked_add_int(
            stats.finished, 1, "runtime policy finished count");
      } else {
        still_running.push_back(request_id);
      }
    }
    running = std::move(still_running);
    stats.cache_stats = cache.stats();
    trace.push_back(stats);
  }

  probe::require_empty_paged_kv_probe_cache(
      cache.stats(), "runtime scheduler final cache");

  PolicyResult result;
  result.config = policy;
  result.trace = std::move(trace);
  result.requests = std::move(requests);
  result.request_metrics = collect_request_metrics(result.requests);
  result.profile = profile_scheduler_pressure(
      pressure_trace(result.trace),
      sorted_metrics(result.request_metrics));
  const auto aggregates = derive_aggregates(result.trace, result.requests);
  result.total_preemptions = aggregates.total_preemptions;
  result.preempted_request_count = aggregates.preempted_request_count;
  result.max_preemptions_per_request =
      aggregates.max_preemptions_per_request;
  result.total_recompute_tokens = aggregates.total_recompute_tokens;
  result.total_backpressure_deferrals =
      aggregates.total_backpressure_deferrals;
  result.append_pressure_events = aggregates.append_pressure_events;
  result.step_budget = step_budget;
  if (result.profile.total_backpressure_deferred !=
          result.total_backpressure_deferrals ||
      aggregates.trace_preemptions != result.total_preemptions ||
      result.profile.request_count < 0 ||
      static_cast<std::size_t>(result.profile.request_count) !=
          workload.size() ||
      result.trace.size() > result.step_budget) {
    throw std::logic_error(
        "runtime scheduler result disagrees with trace or workload");
  }
  return result;
}

std::size_t best_finished_index(const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (results[i].profile.finished_request_count >
            results[best].profile.finished_request_count ||
        (results[i].profile.finished_request_count ==
             results[best].profile.finished_request_count &&
         results[i].profile.rejected_request_count <
             results[best].profile.rejected_request_count)) {
      best = i;
    }
  }
  return best;
}

template <typename Metric>
std::size_t highest_index(const std::vector<PolicyResult>& results,
                          Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) > metric(results[best])) {
      best = i;
    }
  }
  return best;
}

template <typename Metric>
std::size_t lowest_index(const std::vector<PolicyResult>& results,
                         Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) < metric(results[best])) {
      best = i;
    }
  }
  return best;
}

TradeoffSummary make_tradeoff_summary(
    const std::vector<PolicyResult>& results) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "runtime policy results");
  TradeoffSummary summary;
  summary.best_finished_policy_index = best_finished_index(results);
  summary.best_tokens_per_step_index = highest_index(
      results, [](const PolicyResult& result) {
        return result.profile.tokens_per_step;
      });
  summary.lowest_average_ttft_index = lowest_index(
      results, [](const PolicyResult& result) {
        return result.profile.average_ttft_steps;
      });
  summary.highest_preemption_index = highest_index(
      results, [](const PolicyResult& result) {
        return result.total_preemptions;
      });
  summary.highest_backpressure_deferral_index = highest_index(
      results, [](const PolicyResult& result) {
        return result.total_backpressure_deferrals;
      });
  summary.total_preemptions =
      probe::checked_scheduler_result_metric_total(
          results,
          [](const PolicyResult& result) {
            return result.total_preemptions;
          },
          "runtime policy summary preemptions");
  summary.total_recompute_tokens =
      probe::checked_scheduler_result_metric_total(
          results,
          [](const PolicyResult& result) {
            return result.total_recompute_tokens;
          },
          "runtime policy summary recompute tokens");
  summary.total_backpressure_deferrals =
      probe::checked_scheduler_result_metric_total(
          results,
          [](const PolicyResult& result) {
            return result.total_backpressure_deferrals;
          },
          "runtime policy summary backpressure deferrals");
  return summary;
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) + " is outside result range");
  }
}

}  // namespace

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.cache_config = PagedKVCacheConfig{6, 4, 128};
  plan.workload = {
      {1, 12, 6},
      {2, 4, 2},
      {3, 8, 5},
      {4, 16, 3},
      {5, 4, 6},
      {6, 8, 2},
  };
  const std::vector<PolicyConfig> policies = {
      {"reject_on_pressure", false, 0, false, 3},
      {"one_block_backpressure", true, 1, false, 3},
      {"two_block_backpressure", true, 2, false, 3},
      {"preempt_longest_on_pressure", false, 0, true, 3},
      {"hybrid_backpressure_preempt", true, 1, true, 3},
  };

  validate_cache_config(plan.cache_config);
  validate_workload(plan.workload);
  plan.results.reserve(policies.size());
  for (const auto& policy : policies) {
    plan.results.push_back(
        run_policy(policy, plan.cache_config, plan.workload));
  }
  plan.tradeoff = make_tradeoff_summary(plan.results);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_cache_config(plan.cache_config);
  validate_workload(plan.workload);
  if (plan.results.empty()) {
    throw std::logic_error(
        "runtime scheduler plan results must not be empty");
  }

  std::unordered_set<std::string> policy_names;
  policy_names.reserve(plan.results.size());
  for (const auto& result : plan.results) {
    validate_policy(result.config, plan.cache_config);
    if (!policy_names.insert(result.config.name).second ||
        result.requests.size() != plan.workload.size() ||
        result.request_metrics.size() != plan.workload.size() ||
        result.trace.size() > result.step_budget ||
        result.profile.request_count < 0 ||
        static_cast<std::size_t>(result.profile.request_count) !=
            plan.workload.size()) {
      throw std::logic_error(
          "runtime scheduler policy result contract is inconsistent");
    }
    for (std::size_t i = 0U; i < result.requests.size(); ++i) {
      if (!probe::same_scheduler_probe_request(
              result.requests[i].request, plan.workload[i]) ||
          result.requests[i].submitted_step != 0) {
        throw std::logic_error(
            "runtime scheduler request does not match base workload");
      }
    }

    const auto expected_metrics =
        collect_request_metrics(result.requests);
    const auto expected_profile = profile_scheduler_pressure(
        pressure_trace(result.trace), sorted_metrics(expected_metrics));
    const auto expected_aggregates =
        derive_aggregates(result.trace, result.requests);
    if (result.trace.empty()) {
      throw std::logic_error(
          "runtime scheduler trace must not be empty");
    }
    probe::require_empty_paged_kv_probe_cache(
        result.trace.back().cache_stats,
        "runtime scheduler projected final cache");
    if (!probe::same_scheduler_probe_lifecycle_metrics(
            result.request_metrics, expected_metrics) ||
        !probe::same_scheduler_probe_pressure_profile(
            result.profile, expected_profile) ||
        result.total_preemptions !=
            expected_aggregates.total_preemptions ||
        result.preempted_request_count !=
            expected_aggregates.preempted_request_count ||
        result.max_preemptions_per_request !=
            expected_aggregates.max_preemptions_per_request ||
        result.total_recompute_tokens !=
            expected_aggregates.total_recompute_tokens ||
        result.total_backpressure_deferrals !=
            expected_aggregates.total_backpressure_deferrals ||
        result.append_pressure_events !=
            expected_aggregates.append_pressure_events ||
        expected_aggregates.trace_preemptions !=
            expected_aggregates.total_preemptions ||
        result.profile.total_backpressure_deferred !=
            result.total_backpressure_deferrals) {
      throw std::logic_error(
          "runtime scheduler derived result is not canonical");
    }
  }

  const auto count = plan.results.size();
  require_index(plan.tradeoff.best_finished_policy_index, count,
                "best finished policy index");
  require_index(plan.tradeoff.best_tokens_per_step_index, count,
                "best tokens-per-step index");
  require_index(plan.tradeoff.lowest_average_ttft_index, count,
                "lowest average TTFT index");
  require_index(plan.tradeoff.highest_preemption_index, count,
                "highest preemption index");
  require_index(plan.tradeoff.highest_backpressure_deferral_index, count,
                "highest backpressure deferral index");
  const auto expected = make_tradeoff_summary(plan.results);
  if (plan.tradeoff.best_finished_policy_index !=
          expected.best_finished_policy_index ||
      plan.tradeoff.best_tokens_per_step_index !=
          expected.best_tokens_per_step_index ||
      plan.tradeoff.lowest_average_ttft_index !=
          expected.lowest_average_ttft_index ||
      plan.tradeoff.highest_preemption_index !=
          expected.highest_preemption_index ||
      plan.tradeoff.highest_backpressure_deferral_index !=
          expected.highest_backpressure_deferral_index ||
      plan.tradeoff.total_backpressure_deferrals !=
          expected.total_backpressure_deferrals ||
      plan.tradeoff.total_preemptions != expected.total_preemptions ||
      plan.tradeoff.total_recompute_tokens !=
          expected.total_recompute_tokens) {
    throw std::logic_error(
        "runtime scheduler tradeoff summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_runtime_policy
