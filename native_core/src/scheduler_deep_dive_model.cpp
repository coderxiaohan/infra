#include "scheduler_deep_dive_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"
#include "inferlab/advanced_serving_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::scheduler_deep_dive {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::finite_unit_interval;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_non_negative_int;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_positive_int;
using advanced_serving_internal::require_unit_interval;
using advanced_serving_internal::require_unique_nonempty_strings;

constexpr double kWeightTolerance = 1e-9;
constexpr int kMaximumVisibleTimelineChunks = 64;

double checked_weighted_sum(
    const std::initializer_list<std::pair<double, double>>& terms,
    const char* name) {
  double total = 0.0;
  for (const auto& term : terms) {
    const double weighted = finite_product(term.first, term.second, name);
    total = internal::checked_add_finite_double(total, weighted, name);
  }
  return total;
}

void require_weight_total(
    const std::initializer_list<double>& weights,
    const char* name) {
  double total = 0.0;
  for (const double weight : weights) {
    total = internal::checked_add_finite_double(
        total, require_unit_interval(weight, name), name);
  }
  if (std::abs(total - 1.0) > kWeightTolerance) {
    throw std::invalid_argument(std::string(name) +
                                " must sum to one");
  }
}

void validate_assumptions(const Assumptions& assumptions) {
  (void)artifact_role_name(assumptions.role);
  require_nonempty(assumptions.profile_name,
                   "scheduler deep-dive assumption profile");
  (void)fairness_policy_name(assumptions.fairness_policy);
  if ((assumptions.role == ArtifactRole::batching &&
       assumptions.fairness_policy !=
           FairnessPolicy::queue_tenant_reciprocal) ||
      (assumptions.role == ArtifactRole::root_cause &&
       assumptions.fairness_policy != FairnessPolicy::quota_active_linear)) {
    throw std::invalid_argument(
        "scheduler deep-dive role and fairness policy disagree");
  }

  require_non_negative_finite(
      assumptions.canonical_chunk_launch_overhead_ms,
      "scheduler deep-dive canonical launch overhead");
  require_unit_interval(
      assumptions.canonical_prefix_cache_hit_rate,
      "scheduler deep-dive canonical prefix cache hit rate");
  require_positive_finite(
      assumptions.canonical_kv_bytes_per_token,
      "scheduler deep-dive canonical KV bytes per token");
  require_positive_finite(
      assumptions.canonical_kv_budget_gb,
      "scheduler deep-dive canonical KV budget");
  require_non_negative_finite(
      assumptions.queue_base_factor,
      "scheduler deep-dive queue base factor");
  require_non_negative_finite(
      assumptions.queue_kv_pressure_weight,
      "scheduler deep-dive queue KV pressure weight");
  require_non_negative_finite(
      assumptions.queue_quota_pressure_weight,
      "scheduler deep-dive queue quota pressure weight");
  require_unit_interval(
      assumptions.tpot_decode_starvation_weight,
      "scheduler deep-dive TPOT starvation weight");
  require_non_negative_finite(
      assumptions.p99_tenant_tick_weight,
      "scheduler deep-dive P99 tenant tick weight");
  require_non_negative_finite(
      assumptions.p99_quota_penalty_ms,
      "scheduler deep-dive P99 quota penalty");
  require_non_negative_finite(
      assumptions.fairness_queue_depth_weight,
      "scheduler deep-dive fairness queue weight");
  require_non_negative_finite(
      assumptions.fairness_tenant_weight,
      "scheduler deep-dive fairness tenant weight");
  require_non_negative_finite(
      assumptions.fairness_quota_weight,
      "scheduler deep-dive fairness quota weight");
  require_non_negative_finite(
      assumptions.fairness_active_request_weight,
      "scheduler deep-dive fairness active-request weight");
  require_unit_interval(
      assumptions.fairness_active_request_penalty_cap,
      "scheduler deep-dive fairness active-request cap");

  require_weight_total(
      {assumptions.ttft_risk_weight, assumptions.p99_risk_weight},
      "scheduler deep-dive risk weights");
  require_weight_total(
      {assumptions.admission_weights.inverse_slo_risk,
       assumptions.admission_weights.fairness,
       assumptions.admission_weights.inverse_chunk_pressure,
       assumptions.admission_weights.inverse_kv_pressure,
       assumptions.admission_weights.decode_stall_policy},
      "scheduler deep-dive admission weights");
  require_weight_total(
      {assumptions.deep_dive_weights.admission,
       assumptions.deep_dive_weights.inverse_slo_risk,
       assumptions.deep_dive_weights.fairness,
       assumptions.deep_dive_weights.inverse_decode_stall,
       assumptions.deep_dive_weights.healthy_root_cause},
      "scheduler deep-dive score weights");

  require_positive_finite(
      assumptions.chunk_pressure_tick_window,
      "scheduler deep-dive chunk pressure tick window");
  require_non_negative_finite(
      assumptions.decode_stall_policy_threshold_ms,
      "scheduler deep-dive stall policy threshold");
  require_unit_interval(
      assumptions.high_stall_policy_credit,
      "scheduler deep-dive high-stall policy credit");
  require_unit_interval(
      assumptions.unhealthy_root_cause_credit,
      "scheduler deep-dive unhealthy root-cause credit");
  require_positive_finite(
      assumptions.decode_stall_normalization_ms,
      "scheduler deep-dive stall normalization");
  require_unit_interval(assumptions.admission_threshold,
                        "scheduler deep-dive admission threshold");
  require_unit_interval(assumptions.defer_risk_threshold,
                        "scheduler deep-dive defer-risk threshold");
  require_unit_interval(assumptions.defer_quota_threshold,
                        "scheduler deep-dive defer-quota threshold");
  require_unit_interval(assumptions.ready_score_threshold,
                        "scheduler deep-dive ready-score threshold");
  require_non_negative_finite(
      assumptions.recommendation_stall_threshold_ms,
      "scheduler deep-dive recommendation stall threshold");
  require_unit_interval(assumptions.root_quota_threshold,
                        "scheduler deep-dive root quota threshold");
  require_unit_interval(assumptions.root_kv_threshold,
                        "scheduler deep-dive root KV threshold");
  require_non_negative_finite(
      assumptions.root_decode_stall_threshold_ms,
      "scheduler deep-dive root stall threshold");
  require_unit_interval(
      assumptions.root_queue_ttft_share_threshold,
      "scheduler deep-dive root queue TTFT share threshold");
  require_unit_interval(assumptions.root_slo_risk_threshold,
                        "scheduler deep-dive root SLO threshold");
  if (assumptions.max_visible_prefill_chunks <= 0 ||
      assumptions.max_visible_prefill_chunks >
          kMaximumVisibleTimelineChunks) {
    throw std::invalid_argument(
        "scheduler deep-dive visible chunk limit is invalid");
  }
}

void validate_scenario(const Scenario& scenario) {
  require_nonempty(scenario.scenario_id,
                   "scheduler deep-dive scenario id");
  require_nonempty(scenario.tenant_id,
                   "scheduler deep-dive tenant id");
  require_nonempty(scenario.alignment,
                   "scheduler deep-dive alignment");
  require_non_negative_int(scenario.prompt_tokens,
                           "scheduler deep-dive prompt tokens");
  require_non_negative_int(scenario.output_tokens,
                           "scheduler deep-dive output tokens");
  require_positive_int(scenario.chunk_tokens,
                       "scheduler deep-dive chunk tokens");
  require_non_negative_int(scenario.queue_depth,
                           "scheduler deep-dive queue depth");
  require_non_negative_int(scenario.tenant_concurrency,
                           "scheduler deep-dive tenant concurrency");
  require_positive_int(scenario.max_batch_tokens,
                       "scheduler deep-dive batch token budget");
  if (scenario.max_batch_tokens < scenario.chunk_tokens) {
    throw std::invalid_argument(
        "scheduler deep-dive batch token budget must cover one chunk");
  }
  const bool active_work =
      scenario.prompt_tokens > 0 || scenario.output_tokens > 0;
  if (active_work && scenario.tenant_concurrency == 0) {
    throw std::invalid_argument(
        "scheduler deep-dive active work requires tenant concurrency");
  }
  require_non_negative_finite(scenario.arrival_ms,
                              "scheduler deep-dive arrival time");
  const double prefill_ms_per_token = require_non_negative_finite(
      scenario.prefill_ms_per_token,
      "scheduler deep-dive prefill milliseconds per token");
  const double decode_ms_per_token = require_non_negative_finite(
      scenario.decode_ms_per_token,
      "scheduler deep-dive decode milliseconds per token");
  const double scheduler_tick = require_non_negative_finite(
      scenario.scheduler_tick_ms,
      "scheduler deep-dive scheduler tick");
  if (scenario.prompt_tokens > 0 && prefill_ms_per_token == 0.0) {
    throw std::invalid_argument(
        "scheduler deep-dive prefill work requires positive throughput cost");
  }
  if (scenario.output_tokens > 0 && decode_ms_per_token == 0.0) {
    throw std::invalid_argument(
        "scheduler deep-dive decode work requires positive throughput cost");
  }
  if (active_work && scheduler_tick == 0.0) {
    throw std::invalid_argument(
        "scheduler deep-dive active work requires a positive scheduler tick");
  }
  require_unit_interval(scenario.kv_pressure_ratio,
                        "scheduler deep-dive KV pressure ratio");
  require_unit_interval(scenario.quota_pressure_ratio,
                        "scheduler deep-dive quota pressure ratio");
  if (!active_work &&
      (scenario.queue_depth != 0 || scenario.tenant_concurrency != 0 ||
       scenario.kv_pressure_ratio != 0.0 ||
       scenario.quota_pressure_ratio != 0.0)) {
    throw std::invalid_argument(
        "scheduler deep-dive inactive work cannot carry active pressure");
  }
  const double slo_ttft = require_non_negative_finite(
      scenario.slo_ttft_ms, "scheduler deep-dive TTFT SLO");
  const double slo_p99 = require_non_negative_finite(
      scenario.slo_p99_ms, "scheduler deep-dive P99 SLO");
  if (active_work && (slo_ttft == 0.0 || slo_p99 == 0.0)) {
    throw std::invalid_argument(
        "scheduler deep-dive active work requires positive SLO budgets");
  }
}

CanonicalChunkedPrefillProjection project_canonical_chunked_prefill(
    const Assumptions& assumptions, const Scenario& scenario) {
  ChunkedPrefillSchedulerScenario input;
  input.name = scenario.scenario_id;
  input.backend = scenario.alignment;
  input.prompt_tokens = scenario.prompt_tokens;
  input.output_tokens = scenario.output_tokens;
  input.chunk_size = scenario.chunk_tokens;
  input.max_prefill_batch_tokens = scenario.max_batch_tokens;
  input.concurrent_decode_streams =
      scenario.output_tokens > 0 ? scenario.tenant_concurrency : 0;
  input.prefix_cache_hit_rate =
      assumptions.canonical_prefix_cache_hit_rate;
  input.prefill_ms_per_token = scenario.prefill_ms_per_token;
  input.decode_ms_per_token = scenario.decode_ms_per_token;
  input.scheduler_tick_ms = scenario.scheduler_tick_ms;
  input.chunk_launch_overhead_ms =
      assumptions.canonical_chunk_launch_overhead_ms;
  input.kv_bytes_per_token = assumptions.canonical_kv_bytes_per_token;
  input.kv_budget_gb = assumptions.canonical_kv_budget_gb;

  const auto result = estimate_chunked_prefill_scheduler(input);
  CanonicalChunkedPrefillProjection projection;
  projection.effective_prompt_tokens = result.effective_prompt_tokens;
  projection.prefill_chunk_count = result.chunk_count;
  projection.prefill_wave_count = result.prefill_waves;
  projection.decode_maximal_batch_size =
      result.decode_maximal_batch_size;
  projection.first_chunk_ttft_ms = result.first_chunk_ttft_ms;
  projection.total_prefill_ms = result.total_prefill_ms;
  projection.scheduler_gap_ms = result.scheduler_gap_ms;
  projection.decode_stall_ms = result.decode_stall_ms;
  projection.decode_starvation_risk = result.decode_starvation_risk;
  projection.mechanism_fairness_estimate =
      result.jain_fairness_estimate;
  projection.mechanism_p99_risk = result.p99_risk_score;
  projection.chunk_policy = result.chunk_policy;
  projection.recommendation = result.recommendation;
  require_nonempty(projection.chunk_policy,
                   "scheduler deep-dive canonical chunk policy");
  require_nonempty(projection.recommendation,
                   "scheduler deep-dive canonical recommendation");
  return projection;
}

double calculate_queue_wait(const Assumptions& assumptions,
                            const Scenario& scenario) {
  double factor = assumptions.queue_base_factor;
  factor = internal::checked_add_finite_double(
      factor,
      finite_product(scenario.kv_pressure_ratio,
                     assumptions.queue_kv_pressure_weight,
                     "scheduler deep-dive queue KV pressure"),
      "scheduler deep-dive queue factor");
  factor = internal::checked_add_finite_double(
      factor,
      finite_product(scenario.quota_pressure_ratio,
                     assumptions.queue_quota_pressure_weight,
                     "scheduler deep-dive queue quota pressure"),
      "scheduler deep-dive queue factor");
  return finite_product(
      finite_product(static_cast<double>(scenario.queue_depth),
                     scenario.scheduler_tick_ms,
                     "scheduler deep-dive queue ticks"),
      factor, "scheduler deep-dive queue wait");
}

double calculate_prefill_chunk_ms(const Scenario& scenario) {
  return finite_product(
      static_cast<double>(
          std::min(scenario.prompt_tokens, scenario.chunk_tokens)),
      scenario.prefill_ms_per_token,
      "scheduler deep-dive first prefill chunk duration");
}

double calculate_fairness(const Assumptions& assumptions,
                          const Scenario& scenario,
                          const bool active_work) {
  if (!active_work) {
    return 1.0;
  }
  switch (assumptions.fairness_policy) {
    case FairnessPolicy::queue_tenant_reciprocal: {
      const double queue_penalty = finite_product(
          static_cast<double>(scenario.queue_depth),
          assumptions.fairness_queue_depth_weight,
          "scheduler deep-dive fairness queue penalty");
      const double tenant_penalty = finite_product(
          static_cast<double>(std::max(0, scenario.tenant_concurrency - 1)),
          assumptions.fairness_tenant_weight,
          "scheduler deep-dive fairness tenant penalty");
      const double denominator = internal::checked_add_finite_double(
          1.0,
          internal::checked_add_finite_double(
              queue_penalty, tenant_penalty,
              "scheduler deep-dive fairness denominator"),
          "scheduler deep-dive fairness denominator");
      return require_finite(1.0 / denominator,
                            "scheduler deep-dive fairness score");
    }
    case FairnessPolicy::quota_active_linear: {
      const double quota_penalty = finite_product(
          scenario.quota_pressure_ratio,
          assumptions.fairness_quota_weight,
          "scheduler deep-dive fairness quota penalty");
      const double active_penalty = std::min(
          assumptions.fairness_active_request_penalty_cap,
          finite_product(
              static_cast<double>(scenario.tenant_concurrency),
              assumptions.fairness_active_request_weight,
              "scheduler deep-dive fairness active penalty"));
      return finite_unit_interval(
          1.0 - quota_penalty - active_penalty,
          "scheduler deep-dive fairness score");
    }
  }
  throw std::invalid_argument(
      "scheduler deep-dive fairness policy is invalid");
}

double calculate_excess_risk(const double observed,
                             const double budget,
                             const char* name) {
  require_non_negative_finite(observed, name);
  require_positive_finite(budget, name);
  if (observed <= budget) {
    return 0.0;
  }
  return finite_unit_interval((observed - budget) / budget, name);
}

RootCause classify_root_cause(const Assumptions& assumptions,
                              const Record& record) {
  if (!record.active_work) {
    return RootCause::not_applicable;
  }
  if (record.scenario.quota_pressure_ratio >
      assumptions.root_quota_threshold) {
    return RootCause::tenant_quota_pressure;
  }
  if (record.scenario.kv_pressure_ratio > assumptions.root_kv_threshold) {
    return RootCause::kv_budget_pressure;
  }
  if (record.decode_stall_ms >
      assumptions.root_decode_stall_threshold_ms) {
    return RootCause::prefill_chunk_decode_stall;
  }
  const double queue_budget = finite_product(
      record.scenario.slo_ttft_ms,
      assumptions.root_queue_ttft_share_threshold,
      "scheduler deep-dive queue TTFT budget");
  if (record.queue_wait_ms > queue_budget) {
    return RootCause::queue_depth_admission_delay;
  }
  if (record.slo_violation_risk > assumptions.root_slo_risk_threshold) {
    return RootCause::slo_p99_tail_risk;
  }
  return RootCause::within_native_policy_budget;
}

AdmissionDecision select_admission_decision(
    const Assumptions& assumptions, const Record& record) {
  if (!record.active_work) {
    return AdmissionDecision::no_active_work;
  }
  if (record.admission_score >= assumptions.admission_threshold) {
    return assumptions.role == ArtifactRole::batching
               ? AdmissionDecision::admit_and_batch
               : AdmissionDecision::admit;
  }
  if (assumptions.role == ArtifactRole::batching) {
    return record.slo_violation_risk > assumptions.defer_risk_threshold
               ? AdmissionDecision::shed_or_defer_prefill
               : AdmissionDecision::admit_with_smaller_chunks;
  }
  return record.slo_violation_risk > assumptions.defer_risk_threshold ||
                 record.scenario.quota_pressure_ratio >
                     assumptions.defer_quota_threshold
             ? AdmissionDecision::defer_or_reject
             : AdmissionDecision::admit_with_smaller_chunks;
}

RecommendedAction action_for_root_cause(const RootCause root_cause) {
  switch (root_cause) {
    case RootCause::not_applicable:
      return RecommendedAction::no_active_work;
    case RootCause::tenant_quota_pressure:
      return RecommendedAction::
          apply_tenant_quota_or_fairness_guard_before_admission;
    case RootCause::kv_budget_pressure:
      return RecommendedAction::reduce_kv_pressure_or_route_to_warm_kv_tier;
    case RootCause::prefill_chunk_decode_stall:
      return RecommendedAction::
          reduce_prefill_chunk_or_split_prefill_decode_workers;
    case RootCause::queue_depth_admission_delay:
      return RecommendedAction::
          defer_low_priority_prefill_or_scale_decode_capacity;
    case RootCause::slo_p99_tail_risk:
      return RecommendedAction::
          enable_backpressure_or_replay_product_trace_for_tail_validation;
    case RootCause::within_native_policy_budget:
      return RecommendedAction::
          ready_to_compare_with_product_scheduler_replay;
  }
  throw std::invalid_argument("scheduler root cause is invalid");
}

RecommendedAction select_recommended_action(
    const Assumptions& assumptions, const Record& record) {
  if (!record.active_work) {
    return RecommendedAction::no_active_work;
  }
  if (assumptions.role == ArtifactRole::root_cause) {
    return action_for_root_cause(record.root_cause);
  }
  if (record.scheduler_deep_dive_score >=
      assumptions.ready_score_threshold) {
    return RecommendedAction::
        ready_to_compare_with_product_scheduler_replay;
  }
  if (record.decode_stall_ms >
      assumptions.recommendation_stall_threshold_ms) {
    return RecommendedAction::
        reduce_prefill_chunk_or_split_prefill_decode_workers;
  }
  return RecommendedAction::
      attach_real_ttft_tpot_p99_traces_after_migration;
}

std::vector<TimelineEvent> build_timeline(
    const Assumptions& assumptions, const Scenario& scenario,
    const CanonicalChunkedPrefillProjection& canonical,
    const double queue_wait_ms, const double decode_stall_ms) {
  std::vector<TimelineEvent> events;
  const int visible_chunks = std::min(
      canonical.prefill_chunk_count,
      assumptions.max_visible_prefill_chunks);
  events.reserve(static_cast<std::size_t>(visible_chunks) + 3U);
  events.push_back({"queue_wait", "admission", 0.0, queue_wait_ms});

  double cursor = queue_wait_ms;
  for (int index = 0; index < visible_chunks; ++index) {
    const int offset = internal::checked_multiply_int(
        index, scenario.chunk_tokens,
        "scheduler deep-dive timeline token offset");
    const int remaining = scenario.prompt_tokens - offset;
    if (remaining <= 0) {
      throw std::logic_error(
          "scheduler deep-dive timeline exceeds prompt tokens");
    }
    const int tokens = std::min(scenario.chunk_tokens, remaining);
    const double duration = finite_product(
        static_cast<double>(tokens), scenario.prefill_ms_per_token,
        "scheduler deep-dive timeline chunk duration");
    events.push_back({"prefill_chunk_" + std::to_string(index),
                      "prefill", cursor, duration});
    cursor = internal::checked_add_finite_double(
        cursor, duration, "scheduler deep-dive timeline cursor");
    cursor = internal::checked_add_finite_double(
        cursor, scenario.scheduler_tick_ms,
        "scheduler deep-dive timeline cursor");
  }

  const double stall_start = internal::checked_add_finite_double(
      queue_wait_ms,
      calculate_prefill_chunk_ms(scenario),
      "scheduler deep-dive stall start");
  events.push_back(
      {"decode_stall", "decode", stall_start, decode_stall_ms});
  const double decode_duration = finite_product(
      static_cast<double>(scenario.output_tokens),
      scenario.decode_ms_per_token,
      "scheduler deep-dive timeline decode duration");
  events.push_back(
      {"decode_tokens", "decode", cursor, decode_duration});
  return events;
}

Record analyze(const Assumptions& assumptions, const Scenario& scenario) {
  validate_scenario(scenario);
  Record record;
  record.scenario = scenario;
  record.active_work =
      scenario.prompt_tokens > 0 || scenario.output_tokens > 0;
  record.ranking_applicable = record.active_work;
  record.canonical_chunked_prefill =
      project_canonical_chunked_prefill(assumptions, scenario);
  record.queue_wait_ms = calculate_queue_wait(assumptions, scenario);
  record.prefill_chunk_ms = calculate_prefill_chunk_ms(scenario);
  record.decode_stall_ms =
      record.canonical_chunked_prefill.decode_stall_ms;

  if (!record.active_work) {
    record.fairness_score = 1.0;
    record.inverse_chunk_pressure = 1.0;
    record.inverse_kv_pressure = 1.0;
    record.decode_stall_policy_score = 1.0;
    record.admission_score = 1.0;
    record.scheduler_deep_dive_score = 1.0;
    record.admission_decision = AdmissionDecision::no_active_work;
    record.root_cause = RootCause::not_applicable;
    record.recommended_action = RecommendedAction::no_active_work;
    record.timeline = build_timeline(
        assumptions, scenario, record.canonical_chunked_prefill, 0.0, 0.0);
    return record;
  }

  record.ttft_ms = internal::checked_add_finite_double(
      record.queue_wait_ms,
      record.canonical_chunked_prefill.first_chunk_ttft_ms,
      "scheduler deep-dive TTFT");
  const double tpot_multiplier = internal::checked_add_finite_double(
      1.0,
      finite_product(
          record.canonical_chunked_prefill.decode_starvation_risk,
          assumptions.tpot_decode_starvation_weight,
          "scheduler deep-dive TPOT starvation adjustment"),
      "scheduler deep-dive TPOT multiplier");
  record.tpot_ms = scenario.output_tokens == 0
                       ? 0.0
                       : finite_product(scenario.decode_ms_per_token,
                                        tpot_multiplier,
                                        "scheduler deep-dive TPOT");
  const double decode_duration = finite_product(
      static_cast<double>(scenario.output_tokens), record.tpot_ms,
      "scheduler deep-dive decode duration");
  const double tenant_penalty = finite_product(
      finite_product(static_cast<double>(scenario.tenant_concurrency),
                     scenario.scheduler_tick_ms,
                     "scheduler deep-dive tenant tick penalty"),
      assumptions.p99_tenant_tick_weight,
      "scheduler deep-dive tenant tick penalty");
  const double quota_penalty = finite_product(
      scenario.quota_pressure_ratio,
      assumptions.p99_quota_penalty_ms,
      "scheduler deep-dive quota latency penalty");
  double p99 = internal::checked_add_finite_double(
      record.ttft_ms, decode_duration,
      "scheduler deep-dive P99 latency");
  p99 = internal::checked_add_finite_double(
      p99, record.decode_stall_ms,
      "scheduler deep-dive P99 latency");
  p99 = internal::checked_add_finite_double(
      p99, tenant_penalty, "scheduler deep-dive P99 latency");
  record.p99_latency_ms = internal::checked_add_finite_double(
      p99, quota_penalty, "scheduler deep-dive P99 latency");

  record.ttft_risk = calculate_excess_risk(
      record.ttft_ms, scenario.slo_ttft_ms,
      "scheduler deep-dive TTFT risk");
  record.p99_tail_risk = calculate_excess_risk(
      record.p99_latency_ms, scenario.slo_p99_ms,
      "scheduler deep-dive P99 risk");
  record.slo_violation_risk = finite_unit_interval(
      checked_weighted_sum(
          {{record.ttft_risk, assumptions.ttft_risk_weight},
           {record.p99_tail_risk, assumptions.p99_risk_weight}},
          "scheduler deep-dive combined SLO risk"),
      "scheduler deep-dive combined SLO risk");
  record.fairness_score =
      calculate_fairness(assumptions, scenario, record.active_work);

  const double chunk_window = finite_product(
      scenario.scheduler_tick_ms,
      assumptions.chunk_pressure_tick_window,
      "scheduler deep-dive chunk pressure window");
  record.inverse_chunk_pressure =
      record.prefill_chunk_ms == 0.0
          ? 1.0
          : finite_unit_interval(
                chunk_window / record.prefill_chunk_ms,
                "scheduler deep-dive inverse chunk pressure");
  record.inverse_kv_pressure = finite_unit_interval(
      1.0 - scenario.kv_pressure_ratio,
      "scheduler deep-dive inverse KV pressure");
  record.decode_stall_policy_score =
      record.decode_stall_ms <
              assumptions.decode_stall_policy_threshold_ms
          ? 1.0
          : assumptions.high_stall_policy_credit;
  record.admission_score = finite_unit_interval(
      checked_weighted_sum(
          {{1.0 - record.slo_violation_risk,
            assumptions.admission_weights.inverse_slo_risk},
           {record.fairness_score,
            assumptions.admission_weights.fairness},
           {record.inverse_chunk_pressure,
            assumptions.admission_weights.inverse_chunk_pressure},
           {record.inverse_kv_pressure,
            assumptions.admission_weights.inverse_kv_pressure},
           {record.decode_stall_policy_score,
            assumptions.admission_weights.decode_stall_policy}},
          "scheduler deep-dive admission score"),
      "scheduler deep-dive admission score");

  record.root_cause = assumptions.role == ArtifactRole::root_cause
                          ? classify_root_cause(assumptions, record)
                          : RootCause::not_applicable;
  const double inverse_stall = finite_unit_interval(
      1.0 - std::min(1.0, record.decode_stall_ms /
                              assumptions.decode_stall_normalization_ms),
      "scheduler deep-dive inverse stall");
  const double healthy_root_cause =
      record.root_cause == RootCause::within_native_policy_budget
          ? 1.0
          : assumptions.unhealthy_root_cause_credit;
  record.scheduler_deep_dive_score = finite_unit_interval(
      checked_weighted_sum(
          {{record.admission_score,
            assumptions.deep_dive_weights.admission},
           {1.0 - record.slo_violation_risk,
            assumptions.deep_dive_weights.inverse_slo_risk},
           {record.fairness_score,
            assumptions.deep_dive_weights.fairness},
           {inverse_stall,
            assumptions.deep_dive_weights.inverse_decode_stall},
           {healthy_root_cause,
            assumptions.deep_dive_weights.healthy_root_cause}},
          "scheduler deep-dive score"),
      "scheduler deep-dive score");
  record.admission_decision =
      select_admission_decision(assumptions, record);
  record.recommended_action =
      select_recommended_action(assumptions, record);
  record.timeline = build_timeline(
      assumptions, scenario, record.canonical_chunked_prefill,
      record.queue_wait_ms, record.decode_stall_ms);
  return record;
}

}  // namespace

const char* artifact_role_name(const ArtifactRole role) {
  switch (role) {
    case ArtifactRole::batching:
      return "scheduler_batching";
    case ArtifactRole::root_cause:
      return "scheduler_root_cause";
  }
  throw std::invalid_argument("scheduler deep-dive artifact role is invalid");
}

const char* fairness_policy_name(const FairnessPolicy policy) {
  switch (policy) {
    case FairnessPolicy::queue_tenant_reciprocal:
      return "queue_tenant_reciprocal";
    case FairnessPolicy::quota_active_linear:
      return "quota_active_linear";
  }
  throw std::invalid_argument("scheduler deep-dive fairness policy is invalid");
}

const char* admission_decision_name(const AdmissionDecision decision) {
  switch (decision) {
    case AdmissionDecision::no_active_work:
      return "no_active_work";
    case AdmissionDecision::admit_and_batch:
      return "admit_and_batch";
    case AdmissionDecision::admit:
      return "admit";
    case AdmissionDecision::admit_with_smaller_chunks:
      return "admit_with_smaller_chunks";
    case AdmissionDecision::shed_or_defer_prefill:
      return "shed_or_defer_prefill";
    case AdmissionDecision::defer_or_reject:
      return "defer_or_reject";
  }
  throw std::invalid_argument(
      "scheduler deep-dive admission decision is invalid");
}

const char* root_cause_name(const RootCause root_cause) {
  switch (root_cause) {
    case RootCause::not_applicable:
      return "not_applicable";
    case RootCause::tenant_quota_pressure:
      return "tenant_quota_pressure";
    case RootCause::kv_budget_pressure:
      return "kv_budget_pressure";
    case RootCause::prefill_chunk_decode_stall:
      return "prefill_chunk_decode_stall";
    case RootCause::queue_depth_admission_delay:
      return "queue_depth_admission_delay";
    case RootCause::slo_p99_tail_risk:
      return "slo_p99_tail_risk";
    case RootCause::within_native_policy_budget:
      return "within_native_policy_budget";
  }
  throw std::invalid_argument("scheduler deep-dive root cause is invalid");
}

const char* recommended_action_name(const RecommendedAction action) {
  switch (action) {
    case RecommendedAction::no_active_work:
      return "no_active_work";
    case RecommendedAction::ready_to_compare_with_product_scheduler_replay:
      return "ready_to_compare_with_product_scheduler_replay";
    case RecommendedAction::
        reduce_prefill_chunk_or_split_prefill_decode_workers:
      return "reduce_prefill_chunk_or_split_prefill_decode_workers";
    case RecommendedAction::
        attach_real_ttft_tpot_p99_traces_after_migration:
      return "attach_real_ttft_tpot_p99_traces_after_migration";
    case RecommendedAction::
        apply_tenant_quota_or_fairness_guard_before_admission:
      return "apply_tenant_quota_or_fairness_guard_before_admission";
    case RecommendedAction::reduce_kv_pressure_or_route_to_warm_kv_tier:
      return "reduce_kv_pressure_or_route_to_warm_kv_tier";
    case RecommendedAction::
        defer_low_priority_prefill_or_scale_decode_capacity:
      return "defer_low_priority_prefill_or_scale_decode_capacity";
    case RecommendedAction::
        enable_backpressure_or_replay_product_trace_for_tail_validation:
      return "enable_backpressure_or_replay_product_trace_for_tail_validation";
  }
  throw std::invalid_argument(
      "scheduler deep-dive recommended action is invalid");
}

Assumptions default_assumptions(const ArtifactRole role) {
  Assumptions assumptions;
  assumptions.role = role;
  assumptions.canonical_prefix_cache_hit_rate = 0.0;
  assumptions.canonical_chunk_launch_overhead_ms = 0.0;
  assumptions.canonical_kv_bytes_per_token = 32768.0;
  assumptions.canonical_kv_budget_gb = 1024.0;
  assumptions.chunk_pressure_tick_window = 12.0;
  assumptions.decode_stall_normalization_ms = 300.0;
  assumptions.high_stall_policy_credit = 0.25;
  assumptions.unhealthy_root_cause_credit = 1.0 / 3.0;
  assumptions.root_quota_threshold = 0.75;
  assumptions.root_kv_threshold = 0.80;
  assumptions.root_decode_stall_threshold_ms = 180.0;
  assumptions.root_queue_ttft_share_threshold = 0.45;
  assumptions.root_slo_risk_threshold = 0.35;
  assumptions.max_visible_prefill_chunks = 6;

  if (role == ArtifactRole::batching) {
    assumptions.profile_name = "scheduler_batching_compiled_fixture_v2";
    assumptions.fairness_policy =
        FairnessPolicy::queue_tenant_reciprocal;
    assumptions.queue_base_factor = 0.65;
    assumptions.queue_kv_pressure_weight = 0.0;
    assumptions.queue_quota_pressure_weight = 0.0;
    assumptions.tpot_decode_starvation_weight = 0.75;
    assumptions.p99_tenant_tick_weight = 1.0;
    assumptions.p99_quota_penalty_ms = 0.0;
    assumptions.fairness_queue_depth_weight = 0.015;
    assumptions.fairness_tenant_weight = 0.035;
    assumptions.fairness_quota_weight = 0.0;
    assumptions.fairness_active_request_weight = 0.0;
    assumptions.fairness_active_request_penalty_cap = 0.0;
    assumptions.ttft_risk_weight = 0.45;
    assumptions.p99_risk_weight = 0.55;
    assumptions.admission_weights = {0.45, 0.35, 0.20, 0.0, 0.0};
    assumptions.deep_dive_weights = {0.50, 0.0, 0.25, 0.25, 0.0};
    assumptions.decode_stall_policy_threshold_ms = 120.0;
    assumptions.admission_threshold = 0.70;
    assumptions.defer_risk_threshold = 0.45;
    assumptions.defer_quota_threshold = 0.82;
    assumptions.ready_score_threshold = 0.72;
    assumptions.recommendation_stall_threshold_ms = 160.0;
  } else if (role == ArtifactRole::root_cause) {
    assumptions.profile_name = "scheduler_root_cause_compiled_fixture_v2";
    assumptions.fairness_policy = FairnessPolicy::quota_active_linear;
    assumptions.queue_base_factor = 0.55;
    assumptions.queue_kv_pressure_weight = 0.35;
    assumptions.queue_quota_pressure_weight = 0.25;
    assumptions.tpot_decode_starvation_weight = 0.90;
    assumptions.p99_tenant_tick_weight = 0.0;
    assumptions.p99_quota_penalty_ms = 180.0;
    assumptions.fairness_queue_depth_weight = 0.0;
    assumptions.fairness_tenant_weight = 0.0;
    assumptions.fairness_quota_weight = 0.45;
    assumptions.fairness_active_request_weight = 0.012;
    assumptions.fairness_active_request_penalty_cap = 0.30;
    assumptions.ttft_risk_weight = 0.35;
    assumptions.p99_risk_weight = 0.65;
    assumptions.admission_weights = {0.38, 0.28, 0.0, 0.22, 0.12};
    assumptions.deep_dive_weights = {0.42, 0.25, 0.18, 0.0, 0.15};
    assumptions.decode_stall_policy_threshold_ms = 120.0;
    assumptions.admission_threshold = 0.72;
    assumptions.defer_risk_threshold = 0.50;
    assumptions.defer_quota_threshold = 0.82;
    assumptions.ready_score_threshold = 0.72;
    assumptions.recommendation_stall_threshold_ms = 160.0;
  } else {
    throw std::invalid_argument(
        "scheduler deep-dive artifact role is invalid");
  }
  validate_assumptions(assumptions);
  return assumptions;
}

std::vector<Scenario> default_scenarios(const ArtifactRole role) {
  if (role == ArtifactRole::batching) {
    return {
        {"continuous_batching_baseline", "aggregate",
         "native_continuous_batching", 4096, 256, 4096, 8, 2, 8192,
         0.0, 0.020, 1.40, 2.0, 0.0, 0.0, 150.0, 950.0},
        {"chunked_prefill_slo_rescue", "aggregate",
         "vllm_sglang_chunked_prefill", 32768, 384, 2048, 18, 4,
         8192, 0.0, 0.018, 1.55, 2.0, 0.0, 0.0, 220.0, 1400.0},
        {"tenant_quota_pressure", "aggregate",
         "kserve_ray_multi_tenant", 8192, 512, 1024, 42, 12, 4096,
         0.0, 0.022, 1.80, 4.0, 0.0, 0.0, 260.0, 1800.0},
        {"prefill_decode_disaggregation", "aggregate",
         "dynamo_pd_split", 65536, 256, 4096, 20, 5, 12288, 0.0,
         0.016, 1.35, 2.0, 0.0, 0.0, 240.0, 1550.0},
    };
  }
  if (role == ArtifactRole::root_cause) {
    return {
        {"req-short-001", "tenant-a",
         "vLLM/SGLang continuous batching", 2048, 256, 1024, 6, 4,
         8192, 0.0, 0.018, 1.35, 2.0, 0.32, 0.20, 150.0, 950.0},
        {"req-long-ctx-014", "tenant-b",
         "chunked prefill + prefix cache", 32768, 384, 2048, 22, 9,
         8192, 12.0, 0.019, 1.50, 2.0, 0.64, 0.35, 260.0,
         1500.0},
        {"req-kv-pressure-027", "tenant-c",
         "LMCache/Mooncake KV pressure", 49152, 256, 4096, 18, 7,
         6144, 18.0, 0.018, 1.42, 3.0, 0.88, 0.28, 320.0,
         1700.0},
        {"req-quota-tenant-103", "tenant-d",
         "KServe/Ray tenant quota", 8192, 512, 1024, 44, 18, 4096,
         22.0, 0.022, 1.80, 4.0, 0.52, 0.86, 300.0, 1800.0},
        {"req-pd-split-211", "tenant-a",
         "Dynamo/llm-d PD disaggregation", 65536, 768, 4096, 30, 11,
         12288, 25.0, 0.016, 1.33, 2.0, 0.70, 0.42, 500.0,
         2400.0},
    };
  }
  throw std::invalid_argument("scheduler deep-dive artifact role is invalid");
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios,
                inferlab_native_trace::Source trace_source) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "scheduler deep-dive scenarios must not be empty");
  }
  std::vector<std::string> scenario_ids;
  scenario_ids.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    validate_scenario(scenario);
    scenario_ids.push_back(scenario.scenario_id);
  }
  require_unique_nonempty_strings(
      scenario_ids, "scheduler deep-dive scenario ids");

  Plan plan;
  plan.role = assumptions.role;
  plan.assumptions = assumptions;
  plan.records.reserve(scenarios.size());
  bool selected = false;
  for (const auto& scenario : scenarios) {
    plan.records.push_back(analyze(assumptions, scenario));
    const std::size_t index = plan.records.size() - 1U;
    const auto& record = plan.records.back();
    if (record.active_work) {
      plan.active_record_count = internal::checked_add_size(
          plan.active_record_count, 1U,
          "scheduler deep-dive active record count");
    }
    if (!record.ranking_applicable) {
      continue;
    }
    plan.comparable_record_count = internal::checked_add_size(
        plan.comparable_record_count, 1U,
        "scheduler deep-dive comparable record count");
    if (!selected) {
      plan.best_record_index = index;
      plan.riskiest_record_index = index;
      selected = true;
      continue;
    }
    if (record.scheduler_deep_dive_score >
        plan.records[plan.best_record_index].scheduler_deep_dive_score) {
      plan.best_record_index = index;
    }
    if (record.slo_violation_risk >
        plan.records[plan.riskiest_record_index].slo_violation_risk) {
      plan.riskiest_record_index = index;
    }
  }
  if (!selected || plan.comparable_record_count == 0U) {
    throw std::invalid_argument(
        "scheduler deep-dive requires an active comparable scenario");
  }
  const inferlab_native_trace::Domain trace_domain =
      assumptions.role == ArtifactRole::batching
          ? inferlab_native_trace::Domain::scheduler_batching
          : inferlab_native_trace::Domain::scheduler_root_cause;
  plan.trace_context = inferlab_native_trace::bind_trace_context(
      trace_domain, scenario_ids, std::move(trace_source));
  return plan;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios) {
  return build_plan(assumptions, scenarios,
                    inferlab_native_trace::Source{});
}

}  // namespace inferlab::scheduler_deep_dive
