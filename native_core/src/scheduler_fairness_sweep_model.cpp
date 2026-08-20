#include "scheduler_fairness_sweep_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::scheduler_fairness {
namespace {

using RequestIndex = std::unordered_map<RequestId, std::size_t>;

RequestLifecycleMetrics make_metrics(const RuntimeRequest& runtime) {
  return probe::make_scheduler_probe_lifecycle_metrics(
      runtime.request, runtime.state, runtime.submit_step,
      runtime.generated_tokens, runtime.first_scheduled_step,
      runtime.first_decode_step, runtime.finished_step);
}

std::vector<RequestLifecycleMetrics> collect_lifecycle_metrics(
    const std::vector<RuntimeRequest>& requests) {
  std::vector<RequestLifecycleMetrics> metrics;
  metrics.reserve(requests.size());
  for (const auto& request : requests) {
    metrics.push_back(make_metrics(request));
  }
  return probe::sorted_scheduler_probe_lifecycle_metrics(std::move(metrics));
}

void validate_workload(const std::vector<ArrivalRequest>& workload) {
  if (workload.empty()) {
    throw std::invalid_argument("fairness workload must not be empty");
  }
  std::unordered_set<RequestId> request_ids;
  request_ids.reserve(workload.size());
  for (const auto& arrival : workload) {
    if (arrival.submit_step < 0 || arrival.request.prompt_tokens < 0 ||
        arrival.request.max_decode_tokens < 0) {
      throw std::invalid_argument(
          "fairness workload tokens and submit steps must be non-negative");
    }
    if (!request_ids.insert(arrival.request.request_id).second) {
      throw std::invalid_argument(
          "fairness workload request ids must be unique");
    }
  }
}

int wait_steps(const RuntimeRequest& request, const int step) {
  return probe::scheduler_probe_step_delta(
      step, request.submit_step, "fairness sweep wait steps");
}

std::size_t select_waiting_position(
    const std::vector<RequestId>& waiting,
    const std::vector<RuntimeRequest>& requests, const RequestIndex& index,
    const Policy policy, const int step,
    const int starvation_guard_threshold_steps) {
  if (waiting.empty()) {
    throw std::logic_error("cannot select from empty waiting queue");
  }

  const auto oldest_wait_position = [&]() {
    return static_cast<std::size_t>(
        std::distance(waiting.begin(),
                      std::max_element(
                          waiting.begin(), waiting.end(),
                          [&](const auto left_id, const auto right_id) {
                            const auto& left =
                                probe::scheduler_probe_request_at(
                                    requests, index, left_id);
                            const auto& right =
                                probe::scheduler_probe_request_at(
                                    requests, index, right_id);
                            if (wait_steps(left, step) !=
                                wait_steps(right, step)) {
                              return wait_steps(left, step) <
                                     wait_steps(right, step);
                            }
                            return left.request.request_id >
                                   right.request.request_id;
                          })));
  };

  if (policy == Policy::kFcfs) {
    return 0U;
  }
  if (policy == Policy::kOldestWaitFirst) {
    return oldest_wait_position();
  }
  if (policy == Policy::kStarvationGuardedShortest) {
    const auto guarded = std::find_if(
        waiting.begin(), waiting.end(), [&](const auto request_id) {
          const auto& request = probe::scheduler_probe_request_at(
              requests, index, request_id);
          return wait_steps(request, step) >= starvation_guard_threshold_steps;
        });
    if (guarded != waiting.end()) {
      return oldest_wait_position();
    }
  }

  if (policy == Policy::kShortestDecodeFirst ||
      policy == Policy::kStarvationGuardedShortest) {
    return static_cast<std::size_t>(
        std::distance(waiting.begin(),
                      std::min_element(
                          waiting.begin(), waiting.end(),
                          [&](const auto left_id, const auto right_id) {
                            const auto& left =
                                probe::scheduler_probe_request_at(
                                    requests, index, left_id);
                            const auto& right =
                                probe::scheduler_probe_request_at(
                                    requests, index, right_id);
                            if (left.request.max_decode_tokens !=
                                right.request.max_decode_tokens) {
                              return left.request.max_decode_tokens <
                                     right.request.max_decode_tokens;
                            }
                            if (left.submit_step != right.submit_step) {
                              return left.submit_step < right.submit_step;
                            }
                            return left.request.request_id <
                                   right.request.request_id;
                          })));
  }

  return static_cast<std::size_t>(
      std::distance(waiting.begin(),
                    std::max_element(
                        waiting.begin(), waiting.end(),
                        [&](const auto left_id, const auto right_id) {
                          const auto& left =
                              probe::scheduler_probe_request_at(
                                  requests, index, left_id);
                          const auto& right =
                              probe::scheduler_probe_request_at(
                                  requests, index, right_id);
                          if (left.request.max_decode_tokens !=
                              right.request.max_decode_tokens) {
                            return left.request.max_decode_tokens <
                                   right.request.max_decode_tokens;
                          }
                          if (left.submit_step != right.submit_step) {
                            return left.submit_step > right.submit_step;
                          }
                          return left.request.request_id >
                                 right.request.request_id;
                        })));
}

bool has_pending_work(const std::vector<RuntimeRequest>& requests,
                      const std::vector<RequestId>& waiting,
                      const std::vector<RequestId>& running) {
  if (!waiting.empty() || !running.empty()) {
    return true;
  }
  for (const auto& request : requests) {
    if (!request.arrived || request.state == RequestState::kWaiting ||
        request.state == RequestState::kRunning) {
      return true;
    }
  }
  return false;
}

FairnessMetrics compute_fairness(
    const std::vector<RequestLifecycleMetrics>& metrics,
    const int starvation_threshold_steps) {
  FairnessMetrics fairness;
  fairness.starvation_threshold_steps = starvation_threshold_steps;

  std::vector<double> waits;
  std::vector<double> latencies;
  waits.reserve(metrics.size());
  latencies.reserve(metrics.size());
  double wait_total = 0.0;
  double wait_square_total = 0.0;
  for (const auto& request : metrics) {
    if (request.queue_wait_steps >= 0) {
      const auto wait = static_cast<double>(request.queue_wait_steps);
      waits.push_back(wait);
      probe::add_to_scheduler_probe_finite_total(
          wait_total, wait, "fairness wait total");
      probe::add_to_scheduler_probe_finite_total(
          wait_square_total,
          probe::checked_scheduler_probe_double_product(
              wait, wait, "fairness squared wait"),
          "fairness squared wait total");
      if (request.queue_wait_steps >= fairness.max_queue_wait_steps) {
        fairness.max_queue_wait_steps = request.queue_wait_steps;
        fairness.worst_queue_wait_request_id = request.request_id;
      }
      if (request.queue_wait_steps >= starvation_threshold_steps) {
        fairness.starved_request_count = internal::checked_add_int(
            fairness.starved_request_count, 1,
            "fairness starved request count");
      }
    }
    if (request.total_latency_steps >= 0) {
      const auto latency = static_cast<double>(request.total_latency_steps);
      latencies.push_back(latency);
      fairness.max_total_latency_steps =
          std::max(fairness.max_total_latency_steps, latency);
    }
  }

  if (!waits.empty()) {
    fairness.average_queue_wait_steps =
        wait_total / static_cast<double>(waits.size());
    double variance_total = 0.0;
    for (const auto wait : waits) {
      const auto delta = wait - fairness.average_queue_wait_steps;
      probe::add_to_scheduler_probe_finite_total(
          variance_total,
          probe::checked_scheduler_probe_double_product(
              delta, delta, "fairness squared wait delta"),
          "fairness variance total");
    }
    fairness.queue_wait_stddev_steps =
        std::sqrt(variance_total / static_cast<double>(waits.size()));
    if (wait_square_total > 0.0) {
      const auto numerator = probe::checked_scheduler_probe_double_product(
          wait_total, wait_total, "fairness Jain numerator");
      const auto denominator = probe::checked_scheduler_probe_double_product(
          static_cast<double>(waits.size()), wait_square_total,
          "fairness Jain denominator");
      fairness.jain_queue_wait_fairness =
          inferlab_plan::clamp01(numerator / denominator);
    }
  }
  if (!latencies.empty()) {
    double latency_total = 0.0;
    for (const auto latency : latencies) {
      probe::add_to_scheduler_probe_finite_total(
          latency_total, latency, "fairness latency total");
    }
    fairness.average_total_latency_steps =
        latency_total / static_cast<double>(latencies.size());
  }
  if (!std::isfinite(fairness.average_queue_wait_steps) ||
      !std::isfinite(fairness.queue_wait_stddev_steps) ||
      !std::isfinite(fairness.jain_queue_wait_fairness) ||
      !std::isfinite(fairness.average_total_latency_steps) ||
      !std::isfinite(fairness.max_total_latency_steps)) {
    throw std::overflow_error("fairness metrics must remain finite");
  }
  return fairness;
}

PolicyResult run_policy(
    const Policy policy, const std::vector<ArrivalRequest>& workload,
    const PagedKVCacheConfig& cache_config, const int max_active_requests,
    const int starvation_guard_threshold_steps,
    const int starvation_threshold_steps) {
  (void)policy_name(policy);
  if (max_active_requests <= 0 || starvation_guard_threshold_steps < 0 ||
      starvation_threshold_steps < 0) {
    throw std::invalid_argument(
        "fairness scheduler limits and thresholds are invalid");
  }
  validate_workload(workload);

  std::vector<InferenceRequest> request_specs;
  request_specs.reserve(workload.size());
  int max_submit_step = 0;
  for (const auto& arrival : workload) {
    request_specs.push_back(arrival.request);
    max_submit_step = std::max(max_submit_step, arrival.submit_step);
  }
  const auto step_budget = probe::scheduler_probe_step_budget_with_delay(
      request_specs, max_submit_step, "fairness scheduler step budget");

  PagedKVCacheManager cache(cache_config);
  std::vector<RuntimeRequest> requests;
  RequestIndex index;
  requests.reserve(workload.size());
  index.reserve(workload.size());
  for (const auto& arrival : workload) {
    RuntimeRequest runtime;
    runtime.request = arrival.request;
    runtime.submit_step = arrival.submit_step;
    requests.push_back(runtime);
    const auto inserted =
        index.emplace(arrival.request.request_id, requests.size() - 1U);
    if (!inserted.second) {
      throw std::logic_error(
          "validated fairness request id was not unique");
    }
  }

  std::vector<RequestId> waiting;
  std::vector<RequestId> running;
  std::vector<SchedulerStepStats> trace;
  waiting.reserve(workload.size());
  running.reserve(std::min(
      workload.size(), static_cast<std::size_t>(max_active_requests)));
  trace.reserve(step_budget);
  int step = 0;

  while (has_pending_work(requests, waiting, running)) {
    if (trace.size() >= step_budget) {
      throw std::logic_error(
          "fairness scheduler exceeded derived progress step budget");
    }
    step = internal::checked_add_int(step, 1, "fairness sweep step");
    for (auto& request : requests) {
      if (!request.arrived && request.submit_step <= step) {
        request.arrived = true;
        request.state = RequestState::kWaiting;
        waiting.push_back(request.request.request_id);
      }
    }

    SchedulerStepStats stats;
    stats.step = step;
    while (!waiting.empty() &&
           internal::checked_int_from_size(
               running.size(), "fairness running request count") <
               max_active_requests) {
      const auto position = select_waiting_position(
          waiting, requests, index, policy, step,
          starvation_guard_threshold_steps);
      const auto request_id = waiting.at(position);
      auto& runtime =
          probe::scheduler_probe_request_at(requests, index, request_id);
      const auto waiting_offset = internal::checked_ptrdiff_offset(
          position, "fairness waiting request offset");
      if (!cache.can_fit_request(runtime.request.prompt_tokens)) {
        waiting.erase(waiting.begin() + waiting_offset);
        runtime.state = RequestState::kRejected;
        runtime.finished_step = step;
        stats.rejected = internal::checked_add_int(
            stats.rejected, 1, "fairness rejected count");
        continue;
      }
      if (!cache.add_request(request_id, runtime.request.prompt_tokens)) {
        stats.backpressure_deferred = internal::checked_add_int(
            stats.backpressure_deferred, 1,
            "fairness backpressure count");
        break;
      }
      waiting.erase(waiting.begin() + waiting_offset);
      runtime.state = RequestState::kRunning;
      runtime.first_scheduled_step = step;
      running.push_back(request_id);
      stats.admitted = internal::checked_add_int(
          stats.admitted, 1, "fairness admitted count");
    }

    std::vector<RequestId> still_running;
    still_running.reserve(running.size());
    for (const auto request_id : running) {
      auto& runtime =
          probe::scheduler_probe_request_at(requests, index, request_id);
      if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
        probe::release_scheduler_probe_request(
            cache, request_id, "fairness completion");
        runtime.state = RequestState::kFinished;
        runtime.finished_step = step;
        stats.finished = internal::checked_add_int(
            stats.finished, 1, "fairness finished count");
        continue;
      }
      if (!cache.append_tokens(request_id, 1)) {
        probe::release_scheduler_probe_request(
            cache, request_id, "fairness append rejection");
        runtime.state = RequestState::kRejected;
        runtime.finished_step = step;
        stats.rejected = internal::checked_add_int(
            stats.rejected, 1, "fairness rejected count");
        continue;
      }
      runtime.generated_tokens = internal::checked_add_int(
          runtime.generated_tokens, 1, "fairness generated tokens");
      if (runtime.first_decode_step < 0) {
        runtime.first_decode_step = step;
      }
      stats.decoded = internal::checked_add_int(
          stats.decoded, 1, "fairness decoded count");
      if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
        probe::release_scheduler_probe_request(
            cache, request_id, "fairness decode completion");
        runtime.state = RequestState::kFinished;
        runtime.finished_step = step;
        stats.finished = internal::checked_add_int(
            stats.finished, 1, "fairness finished count");
      } else {
        still_running.push_back(request_id);
      }
    }
    running = std::move(still_running);
    stats.cache_stats = cache.stats();
    trace.push_back(stats);
  }

  const auto final_cache_stats = cache.stats();
  if (final_cache_stats.active_requests != 0 ||
      final_cache_stats.used_blocks != 0 ||
      final_cache_stats.total_tokens != 0) {
    throw std::logic_error(
        "fairness scheduler completed with live KV state");
  }

  PolicyResult result;
  result.policy = policy;
  result.trace = std::move(trace);
  result.requests = std::move(requests);
  result.lifecycle_metrics = collect_lifecycle_metrics(result.requests);
  result.profile =
      profile_scheduler_pressure(result.trace, result.lifecycle_metrics);
  result.fairness =
      compute_fairness(result.lifecycle_metrics, starvation_threshold_steps);
  result.step_budget = step_budget;
  if (result.profile.request_count < 0 ||
      static_cast<std::size_t>(result.profile.request_count) !=
          workload.size() ||
      result.fairness.starved_request_count > result.profile.request_count ||
      result.trace.size() > result.step_budget ||
      result.lifecycle_metrics.size() != workload.size()) {
    throw std::logic_error(
        "fairness scheduler result disagrees with workload or budget");
  }
  return result;
}

std::size_t best_tokens_per_step_index(
    const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (results[i].profile.tokens_per_step >
        results[best].profile.tokens_per_step) {
      best = i;
    }
  }
  return best;
}

std::size_t lowest_average_ttft_index(
    const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (results[i].profile.average_ttft_steps <
        results[best].profile.average_ttft_steps) {
      best = i;
    }
  }
  return best;
}

bool lower_starvation(const PolicyResult& left,
                      const PolicyResult& right) {
  if (left.fairness.starved_request_count !=
      right.fairness.starved_request_count) {
    return left.fairness.starved_request_count <
           right.fairness.starved_request_count;
  }
  return left.fairness.max_queue_wait_steps <
         right.fairness.max_queue_wait_steps;
}

std::size_t lowest_starvation_index(
    const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (lower_starvation(results[i], results[best])) {
      best = i;
    }
  }
  return best;
}

std::size_t worst_starvation_index(
    const std::vector<PolicyResult>& results) {
  std::size_t worst = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (lower_starvation(results[worst], results[i])) {
      worst = i;
    }
  }
  return worst;
}

std::size_t highest_jain_fairness_index(
    const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (results[i].fairness.jain_queue_wait_fairness >
        results[best].fairness.jain_queue_wait_fairness) {
      best = i;
    }
  }
  return best;
}

std::size_t balanced_policy_index(
    const std::vector<PolicyResult>& results, const double jain_floor,
    const std::size_t fallback_index) {
  std::size_t best = results.size();
  for (std::size_t i = 0U; i < results.size(); ++i) {
    if (results[i].fairness.jain_queue_wait_fairness < jain_floor) {
      continue;
    }
    if (best == results.size() ||
        results[i].fairness.average_queue_wait_steps <
            results[best].fairness.average_queue_wait_steps) {
      best = i;
    }
  }
  return best == results.size() ? fallback_index : best;
}

TradeoffSummary make_tradeoff_summary(
    const std::vector<PolicyResult>& results, const double jain_floor) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "fairness policy results");
  TradeoffSummary summary;
  summary.balanced_jain_floor = jain_floor;
  summary.best_tokens_per_step_index =
      best_tokens_per_step_index(results);
  summary.lowest_average_ttft_index =
      lowest_average_ttft_index(results);
  summary.lowest_starvation_index = lowest_starvation_index(results);
  summary.worst_starvation_index = worst_starvation_index(results);
  summary.highest_jain_fairness_index =
      highest_jain_fairness_index(results);
  summary.balanced_policy_index = balanced_policy_index(
      results, summary.balanced_jain_floor,
      summary.lowest_starvation_index);
  return summary;
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) + " is outside result range");
  }
}

bool same_lifecycle_metric(const RequestLifecycleMetrics& left,
                           const RequestLifecycleMetrics& right) {
  return left.request_id == right.request_id &&
         left.state == right.state &&
         left.prompt_tokens == right.prompt_tokens &&
         left.max_decode_tokens == right.max_decode_tokens &&
         left.submitted_step == right.submitted_step &&
         left.first_scheduled_step == right.first_scheduled_step &&
         left.first_decode_step == right.first_decode_step &&
         left.finished_step == right.finished_step &&
         left.generated_tokens == right.generated_tokens &&
         left.queue_wait_steps == right.queue_wait_steps &&
         left.time_to_first_token_steps ==
             right.time_to_first_token_steps &&
         left.total_latency_steps == right.total_latency_steps &&
         left.decode_steps_per_token == right.decode_steps_per_token;
}

bool same_lifecycle_metrics(
    const std::vector<RequestLifecycleMetrics>& left,
    const std::vector<RequestLifecycleMetrics>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0U; i < left.size(); ++i) {
    if (!same_lifecycle_metric(left[i], right[i])) {
      return false;
    }
  }
  return true;
}

bool same_pressure_profile(const SchedulerPressureProfile& left,
                           const SchedulerPressureProfile& right) {
  return left.steps == right.steps &&
         left.request_count == right.request_count &&
         left.finished_request_count == right.finished_request_count &&
         left.rejected_request_count == right.rejected_request_count &&
         left.total_admitted == right.total_admitted &&
         left.total_decoded_tokens == right.total_decoded_tokens &&
         left.total_finished == right.total_finished &&
         left.total_rejected == right.total_rejected &&
         left.total_backpressure_deferred ==
             right.total_backpressure_deferred &&
         left.peak_active_requests == right.peak_active_requests &&
         left.peak_used_blocks == right.peak_used_blocks &&
         left.peak_reserved_bytes == right.peak_reserved_bytes &&
         left.peak_total_tokens == right.peak_total_tokens &&
         left.peak_cache_utilization == right.peak_cache_utilization &&
         left.average_active_requests == right.average_active_requests &&
         left.average_used_blocks == right.average_used_blocks &&
         left.average_cache_utilization ==
             right.average_cache_utilization &&
         left.average_queue_wait_steps ==
             right.average_queue_wait_steps &&
         left.average_ttft_steps == right.average_ttft_steps &&
         left.average_decode_steps_per_token ==
             right.average_decode_steps_per_token &&
         left.tokens_per_step == right.tokens_per_step;
}

bool same_fairness_metrics(const FairnessMetrics& left,
                           const FairnessMetrics& right) {
  return left.starvation_threshold_steps ==
             right.starvation_threshold_steps &&
         left.starved_request_count == right.starved_request_count &&
         left.worst_queue_wait_request_id ==
             right.worst_queue_wait_request_id &&
         left.max_queue_wait_steps == right.max_queue_wait_steps &&
         left.average_queue_wait_steps == right.average_queue_wait_steps &&
         left.queue_wait_stddev_steps == right.queue_wait_stddev_steps &&
         left.jain_queue_wait_fairness ==
             right.jain_queue_wait_fairness &&
         left.average_total_latency_steps ==
             right.average_total_latency_steps &&
         left.max_total_latency_steps == right.max_total_latency_steps;
}

}  // namespace

const char* policy_name(const Policy policy) {
  switch (policy) {
    case Policy::kFcfs:
      return "fcfs";
    case Policy::kShortestDecodeFirst:
      return "shortest_decode_first";
    case Policy::kLongestDecodeFirst:
      return "longest_decode_first";
    case Policy::kOldestWaitFirst:
      return "oldest_wait_first";
    case Policy::kStarvationGuardedShortest:
      return "starvation_guarded_shortest";
  }
  throw std::invalid_argument("fairness policy is invalid");
}

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.cache_config = PagedKVCacheConfig{10, 4, 128};
  plan.max_active_requests = 2;
  plan.starvation_guard_threshold_steps = 5;
  plan.starvation_threshold_steps = 8;
  plan.workload = {
      {{1, 8, 12}, 0},
      {{2, 8, 10}, 0},
      {{3, 4, 2}, 0},
      {{4, 4, 2}, 1},
      {{5, 4, 2}, 2},
      {{6, 4, 2}, 3},
      {{7, 4, 2}, 4},
      {{8, 4, 2}, 5},
      {{9, 4, 2}, 6},
      {{10, 4, 2}, 7},
  };
  validate_workload(plan.workload);

  constexpr Policy kPolicies[] = {
      Policy::kFcfs,
      Policy::kShortestDecodeFirst,
      Policy::kLongestDecodeFirst,
      Policy::kOldestWaitFirst,
      Policy::kStarvationGuardedShortest,
  };
  plan.results.reserve(sizeof(kPolicies) / sizeof(kPolicies[0]));
  for (const auto policy : kPolicies) {
    plan.results.push_back(run_policy(
        policy, plan.workload, plan.cache_config, plan.max_active_requests,
        plan.starvation_guard_threshold_steps,
        plan.starvation_threshold_steps));
  }
  plan.tradeoff = make_tradeoff_summary(plan.results, 0.5);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_workload(plan.workload);
  if (plan.max_active_requests <= 0 ||
      plan.starvation_guard_threshold_steps < 0 ||
      plan.starvation_threshold_steps < 0 || plan.results.empty() ||
      !std::isfinite(plan.tradeoff.balanced_jain_floor) ||
      plan.tradeoff.balanced_jain_floor < 0.0 ||
      plan.tradeoff.balanced_jain_floor > 1.0) {
    throw std::logic_error("fairness sweep plan contract is invalid");
  }

  std::unordered_set<std::string> policies;
  policies.reserve(plan.results.size());
  for (const auto& result : plan.results) {
    if (!policies.insert(policy_name(result.policy)).second ||
        result.requests.size() != plan.workload.size() ||
        result.lifecycle_metrics.size() != plan.workload.size() ||
        result.trace.size() > result.step_budget ||
        result.profile.request_count < 0 ||
        static_cast<std::size_t>(result.profile.request_count) !=
            plan.workload.size() ||
        result.fairness.starvation_threshold_steps !=
            plan.starvation_threshold_steps) {
      throw std::logic_error(
          "fairness sweep policy result contract is inconsistent");
    }
    for (std::size_t i = 0U; i < result.requests.size(); ++i) {
      const auto& runtime = result.requests[i];
      const auto& arrival = plan.workload[i];
      if (runtime.request.request_id != arrival.request.request_id ||
          runtime.request.prompt_tokens != arrival.request.prompt_tokens ||
          runtime.request.max_decode_tokens !=
              arrival.request.max_decode_tokens ||
          runtime.submit_step != arrival.submit_step) {
        throw std::logic_error(
            "fairness sweep runtime request does not match workload");
      }
    }
    const auto expected_metrics =
        collect_lifecycle_metrics(result.requests);
    const auto expected_profile =
        profile_scheduler_pressure(result.trace, expected_metrics);
    const auto expected_fairness =
        compute_fairness(expected_metrics, plan.starvation_threshold_steps);
    if (!same_lifecycle_metrics(
            result.lifecycle_metrics, expected_metrics) ||
        !same_pressure_profile(result.profile, expected_profile) ||
        !same_fairness_metrics(result.fairness, expected_fairness)) {
      throw std::logic_error(
          "fairness sweep derived metrics are not canonical");
    }
  }

  const auto count = plan.results.size();
  require_index(plan.tradeoff.balanced_policy_index, count,
                "balanced policy index");
  require_index(plan.tradeoff.best_tokens_per_step_index, count,
                "best tokens-per-step index");
  require_index(plan.tradeoff.lowest_average_ttft_index, count,
                "lowest average TTFT index");
  require_index(plan.tradeoff.highest_jain_fairness_index, count,
                "highest Jain fairness index");
  require_index(plan.tradeoff.lowest_starvation_index, count,
                "lowest starvation index");
  require_index(plan.tradeoff.worst_starvation_index, count,
                "worst starvation index");

  const auto expected = make_tradeoff_summary(
      plan.results, plan.tradeoff.balanced_jain_floor);
  if (plan.tradeoff.balanced_policy_index !=
          expected.balanced_policy_index ||
      plan.tradeoff.best_tokens_per_step_index !=
          expected.best_tokens_per_step_index ||
      plan.tradeoff.lowest_average_ttft_index !=
          expected.lowest_average_ttft_index ||
      plan.tradeoff.highest_jain_fairness_index !=
          expected.highest_jain_fairness_index ||
      plan.tradeoff.lowest_starvation_index !=
          expected.lowest_starvation_index ||
      plan.tradeoff.worst_starvation_index !=
          expected.worst_starvation_index) {
    throw std::logic_error(
        "fairness sweep tradeoff summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_fairness
