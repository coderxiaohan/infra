#include "inferlab/advanced_serving_runtime_communication.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"
#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::finite_unit_interval;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_non_negative_int;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_positive_int;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

constexpr double kComparisonTolerance = 1e-9;

bool same_double(const double left, const double right) {
  return std::fabs(left - right) <=
         std::max(1.0, std::max(std::fabs(left), std::fabs(right))) *
             kComparisonTolerance;
}

void require_unit_sum(const double left,
                      const double middle,
                      const double right,
                      const char* name) {
  const double total = internal::checked_add_finite_double(
      internal::checked_add_finite_double(left, middle, name), right,
      name);
  if (!same_double(total, 1.0)) {
    throw std::invalid_argument(std::string(name) + " must sum to one");
  }
}

void validate_assumptions(
    const PdSplitCrossoverAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "PD crossover assumption profile");
  require_positive_finite(
      assumptions.monolithic_prefill_interference_reference_ms,
      "PD crossover monolithic interference reference");
  require_unit_interval(
      assumptions.monolithic_tpot_interference_cap,
      "PD crossover monolithic TPOT interference cap");
  require_positive_finite(
      assumptions.pd_decode_stall_reference_ms,
      "PD crossover decode-stall reference");
  require_unit_interval(
      assumptions.pd_tpot_stall_cap,
      "PD crossover PD TPOT stall cap");
  require_unit_interval(assumptions.queue_relief_weight,
                        "PD crossover queue-relief weight");
  require_unit_interval(assumptions.tpot_relief_weight,
                        "PD crossover TPOT-relief weight");
  require_unit_interval(assumptions.overlap_efficiency_weight,
                        "PD crossover overlap-efficiency weight");
  require_unit_sum(assumptions.queue_relief_weight,
                   assumptions.tpot_relief_weight,
                   assumptions.overlap_efficiency_weight,
                   "PD crossover capacity-relief weights");
  require_unit_interval(assumptions.capacity_relief_weight,
                        "PD crossover capacity-relief score weight");
  require_unit_interval(assumptions.ttft_headroom_weight,
                        "PD crossover TTFT-headroom weight");
  require_unit_interval(assumptions.tpot_improvement_bonus,
                        "PD crossover TPOT-improvement bonus");
  require_unit_interval(assumptions.no_tpot_improvement_bonus,
                        "PD crossover no-improvement bonus");
  require_unit_sum(assumptions.capacity_relief_weight,
                   assumptions.ttft_headroom_weight,
                   assumptions.tpot_improvement_bonus,
                   "PD crossover positive-TPOT score weights");
  if (assumptions.no_tpot_improvement_bonus >
      assumptions.tpot_improvement_bonus) {
    throw std::invalid_argument(
        "PD crossover no-improvement bonus exceeds improvement bonus");
  }
  require_unit_interval(assumptions.crossover_threshold,
                        "PD crossover decision threshold");
  require_non_negative_finite(
      assumptions.transfer_bottleneck_to_prefill_ratio_threshold,
      "PD crossover transfer bottleneck threshold");
  if (!assumptions.transfer.decode_requires_complete_prompt_kv) {
    throw std::invalid_argument(
        "PD crossover requires complete prompt KV before decode");
  }
}

PdSplitCrossoverScenario validate_scenario(
    PdSplitCrossoverScenario scenario) {
  require_nonempty(scenario.name, "PD crossover scenario name");
  require_unique_nonempty_strings(
      scenario.product_profile_ids,
      "PD crossover product profile identities");
  for (const auto& id : scenario.product_profile_ids) {
    (void)mature_product_profiles::require_product_profile(id);
  }
  scenario.concurrent_requests = require_non_negative_int(
      scenario.concurrent_requests,
      "PD crossover concurrent request count");
  scenario.monolithic_queue_ms = require_non_negative_finite(
      scenario.monolithic_queue_ms,
      "PD crossover monolithic queue time");
  scenario.pd_queue_ms = require_non_negative_finite(
      scenario.pd_queue_ms, "PD crossover PD queue time");
  scenario.slo_ttft_ms = require_positive_finite(
      scenario.slo_ttft_ms, "PD crossover TTFT SLO");
  scenario.slo_tpot_ms = require_positive_finite(
      scenario.slo_tpot_ms, "PD crossover TPOT SLO");
  if (scenario.transfer.name != scenario.name) {
    throw std::invalid_argument(
        "PD crossover and canonical transfer scenario identities differ");
  }
  if (scenario.transfer.topology !=
      PdTransferTopology::prefill_decode_split) {
    throw std::invalid_argument(
        "PD crossover scenarios must use prefill/decode split topology");
  }
  const bool has_prompt = scenario.transfer.prompt_tokens > 0;
  const bool has_output = scenario.transfer.output_tokens > 0;
  if (has_prompt != has_output) {
    throw std::invalid_argument(
        "PD crossover comparable work requires prompt and output tokens together");
  }
  if (has_prompt) {
    require_positive_int(scenario.concurrent_requests,
                         "PD crossover active concurrency");
  } else if (scenario.concurrent_requests != 0 ||
             scenario.monolithic_queue_ms != 0.0 ||
             scenario.pd_queue_ms != 0.0) {
    throw std::invalid_argument(
        "PD crossover no-work scenario must not declare queue or concurrency");
  }
  return scenario;
}

std::vector<std::string> product_alignment(
    const std::vector<std::string>& profile_ids) {
  std::vector<std::string> names;
  names.reserve(profile_ids.size());
  for (const auto& id : profile_ids) {
    names.push_back(
        mature_product_profiles::require_product_profile(id).display_name);
  }
  require_unique_nonempty_strings(
      names, "PD crossover product display names");
  return names;
}

double positive_difference(const double left,
                           const double right,
                           const char* name) {
  require_non_negative_finite(left, name);
  require_non_negative_finite(right, name);
  return left > right ? require_finite(left - right, name) : 0.0;
}

double non_negative_ratio(const double numerator,
                          const double denominator,
                          const char* name) {
  require_non_negative_finite(numerator, name);
  if (numerator == 0.0) return 0.0;
  require_positive_finite(denominator, name);
  return require_finite(numerator / denominator, name);
}

double multiply_three(const double first,
                      const double second,
                      const double third,
                      const char* name) {
  return finite_product(finite_product(first, second, name), third,
                        name);
}

bool same_transfer_scenario(const PdTransferOverlapScenario& left,
                            const PdTransferOverlapScenario& right) {
  return left.name == right.name && left.topology == right.topology &&
         left.prompt_tokens == right.prompt_tokens &&
         left.output_tokens == right.output_tokens &&
         left.prefill_chunk_tokens == right.prefill_chunk_tokens &&
         left.decode_chunk_tokens == right.decode_chunk_tokens &&
         left.prefill_workers == right.prefill_workers &&
         left.decode_workers == right.decode_workers &&
         same_double(left.prefill_ms_per_token,
                     right.prefill_ms_per_token) &&
         same_double(left.decode_ms_per_token,
                     right.decode_ms_per_token) &&
         same_double(left.kv_bytes_per_token,
                     right.kv_bytes_per_token) &&
         same_double(left.link_bandwidth_gbps,
                     right.link_bandwidth_gbps) &&
         same_double(left.serialization_us_per_chunk,
                     right.serialization_us_per_chunk) &&
         same_double(left.receiver_setup_us_per_chunk,
                     right.receiver_setup_us_per_chunk) &&
         same_double(left.overlap_efficiency,
                     right.overlap_efficiency);
}

bool same_scenario(const PdSplitCrossoverScenario& left,
                   const PdSplitCrossoverScenario& right) {
  return left.name == right.name &&
         left.product_profile_ids == right.product_profile_ids &&
         left.concurrent_requests == right.concurrent_requests &&
         same_transfer_scenario(left.transfer, right.transfer) &&
         same_double(left.monolithic_queue_ms,
                     right.monolithic_queue_ms) &&
         same_double(left.pd_queue_ms, right.pd_queue_ms) &&
         same_double(left.slo_ttft_ms, right.slo_ttft_ms) &&
         same_double(left.slo_tpot_ms, right.slo_tpot_ms);
}

bool same_stage(const PdTransferStage& left,
                const PdTransferStage& right) {
  return left.kind == right.kind && left.resource == right.resource &&
         left.chunk_index == right.chunk_index &&
         left.token_count == right.token_count &&
         same_double(left.start_ms, right.start_ms) &&
         same_double(left.duration_ms, right.duration_ms) &&
         same_double(left.end_ms, right.end_ms);
}

bool same_transfer_result(const PdTransferOverlapResult& left,
                          const PdTransferOverlapResult& right) {
  if (!same_transfer_scenario(left.scenario, right.scenario) ||
      left.active_workload != right.active_workload ||
      left.transfer_required != right.transfer_required ||
      left.ranking_applicable != right.ranking_applicable ||
      left.prefill_chunks != right.prefill_chunks ||
      left.decode_chunks != right.decode_chunks ||
      left.prompt_kv_token_count != right.prompt_kv_token_count ||
      left.recommendation != right.recommendation ||
      left.stages.size() != right.stages.size()) {
    return false;
  }
  const double left_values[] = {
      left.prompt_kv_bytes,
      left.prompt_kv_gib,
      left.prefill_compute_ms,
      left.decode_compute_ms,
      left.aggregated_baseline_ms,
      left.transfer_payload_serialization_ms,
      left.transfer_overhead_ms,
      left.kv_transfer_total_ms,
      left.transfer_pipeline_span_ms,
      left.transfer_link_busy_ratio,
      left.full_prompt_kv_ready_ms,
      left.disaggregated_no_overlap_ms,
      left.overlapped_end_to_end_ms,
      left.transfer_exposed_ms,
      left.transfer_hidden_ms,
      left.decode_stall_ms,
      left.ttft_ms,
      left.tpot_ms,
      left.overlap_gain_ms,
      left.speedup_vs_no_overlap,
      left.speedup_vs_aggregated,
      left.fabric_utilization_score,
  };
  const double right_values[] = {
      right.prompt_kv_bytes,
      right.prompt_kv_gib,
      right.prefill_compute_ms,
      right.decode_compute_ms,
      right.aggregated_baseline_ms,
      right.transfer_payload_serialization_ms,
      right.transfer_overhead_ms,
      right.kv_transfer_total_ms,
      right.transfer_pipeline_span_ms,
      right.transfer_link_busy_ratio,
      right.full_prompt_kv_ready_ms,
      right.disaggregated_no_overlap_ms,
      right.overlapped_end_to_end_ms,
      right.transfer_exposed_ms,
      right.transfer_hidden_ms,
      right.decode_stall_ms,
      right.ttft_ms,
      right.tpot_ms,
      right.overlap_gain_ms,
      right.speedup_vs_no_overlap,
      right.speedup_vs_aggregated,
      right.fabric_utilization_score,
  };
  for (std::size_t index = 0;
       index < sizeof(left_values) / sizeof(left_values[0]); ++index) {
    if (!same_double(left_values[index], right_values[index])) return false;
  }
  for (std::size_t index = 0; index < left.stages.size(); ++index) {
    if (!same_stage(left.stages[index], right.stages[index])) return false;
  }
  return true;
}

bool same_result(const PdSplitCrossoverResult& left,
                 const PdSplitCrossoverResult& right) {
  if (!same_scenario(left.scenario, right.scenario) ||
      left.product_alignment != right.product_alignment ||
      !same_transfer_result(left.transfer, right.transfer) ||
      left.active_workload != right.active_workload ||
      left.ranking_applicable != right.ranking_applicable ||
      left.ttft_slo_met != right.ttft_slo_met ||
      left.tpot_slo_met != right.tpot_slo_met ||
      left.bottleneck != right.bottleneck ||
      left.decision != right.decision ||
      left.recommendation != right.recommendation) {
    return false;
  }
  const double left_values[] = {
      left.transfer_setup_ms_per_chunk,
      left.monolithic_ttft_ms,
      left.pd_ttft_ms,
      left.monolithic_tpot_ms,
      left.pd_tpot_ms,
      left.ttft_delta_ms,
      left.tpot_delta_ms,
      left.monolithic_prefill_interference_uncapped,
      left.monolithic_prefill_interference,
      left.pd_decode_stall_pressure_uncapped,
      left.pd_decode_stall_pressure,
      left.queue_relief_ratio,
      left.tpot_relief_ratio,
      left.ttft_risk_uncapped,
      left.ttft_risk,
      left.capacity_relief_score_uncapped,
      left.capacity_relief_score,
      left.crossover_score_uncapped,
      left.crossover_score,
  };
  const double right_values[] = {
      right.transfer_setup_ms_per_chunk,
      right.monolithic_ttft_ms,
      right.pd_ttft_ms,
      right.monolithic_tpot_ms,
      right.pd_tpot_ms,
      right.ttft_delta_ms,
      right.tpot_delta_ms,
      right.monolithic_prefill_interference_uncapped,
      right.monolithic_prefill_interference,
      right.pd_decode_stall_pressure_uncapped,
      right.pd_decode_stall_pressure,
      right.queue_relief_ratio,
      right.tpot_relief_ratio,
      right.ttft_risk_uncapped,
      right.ttft_risk,
      right.capacity_relief_score_uncapped,
      right.capacity_relief_score,
      right.crossover_score_uncapped,
      right.crossover_score,
  };
  for (std::size_t index = 0;
       index < sizeof(left_values) / sizeof(left_values[0]); ++index) {
    if (!same_double(left_values[index], right_values[index])) return false;
  }
  return true;
}

bool same_assumptions(const PdSplitCrossoverAssumptions& left,
                      const PdSplitCrossoverAssumptions& right) {
  return left.profile_name == right.profile_name &&
         left.transfer.profile_name == right.transfer.profile_name &&
         same_double(left.transfer.bytes_per_gib,
                     right.transfer.bytes_per_gib) &&
         same_double(
             left.transfer.high_decode_stall_to_decode_ratio_threshold,
             right.transfer.high_decode_stall_to_decode_ratio_threshold) &&
         same_double(left.transfer.high_transfer_link_busy_ratio_threshold,
                     right.transfer.high_transfer_link_busy_ratio_threshold) &&
         same_double(left.transfer.pd_speedup_vs_aggregated_threshold,
                     right.transfer.pd_speedup_vs_aggregated_threshold) &&
         left.transfer.decode_requires_complete_prompt_kv ==
             right.transfer.decode_requires_complete_prompt_kv &&
         left.transfer.sender_receiver_overhead_is_per_chunk ==
             right.transfer.sender_receiver_overhead_is_per_chunk &&
         left.transfer.overlap_efficiency_is_streaming_readiness ==
             right.transfer.overlap_efficiency_is_streaming_readiness &&
         same_double(left.monolithic_prefill_interference_reference_ms,
                     right.monolithic_prefill_interference_reference_ms) &&
         same_double(left.monolithic_tpot_interference_cap,
                     right.monolithic_tpot_interference_cap) &&
         same_double(left.pd_decode_stall_reference_ms,
                     right.pd_decode_stall_reference_ms) &&
         same_double(left.pd_tpot_stall_cap,
                     right.pd_tpot_stall_cap) &&
         same_double(left.queue_relief_weight,
                     right.queue_relief_weight) &&
         same_double(left.tpot_relief_weight,
                     right.tpot_relief_weight) &&
         same_double(left.overlap_efficiency_weight,
                     right.overlap_efficiency_weight) &&
         same_double(left.capacity_relief_weight,
                     right.capacity_relief_weight) &&
         same_double(left.ttft_headroom_weight,
                     right.ttft_headroom_weight) &&
         same_double(left.tpot_improvement_bonus,
                     right.tpot_improvement_bonus) &&
         same_double(left.no_tpot_improvement_bonus,
                     right.no_tpot_improvement_bonus) &&
         same_double(left.crossover_threshold,
                     right.crossover_threshold) &&
         same_double(left.transfer_bottleneck_to_prefill_ratio_threshold,
                     right.transfer_bottleneck_to_prefill_ratio_threshold);
}

}  // namespace

const char* pd_split_crossover_bottleneck_name(
    const PdSplitCrossoverBottleneck bottleneck) {
  switch (bottleneck) {
    case PdSplitCrossoverBottleneck::no_active_workload:
      return "no_active_workload";
    case PdSplitCrossoverBottleneck::kv_transfer_path:
      return "kv_transfer_path";
    case PdSplitCrossoverBottleneck::prefill_compute_or_scheduler_queue:
      return "prefill_compute_or_scheduler_queue";
  }
  throw std::invalid_argument("unknown PD crossover bottleneck");
}

const char* pd_split_crossover_decision_name(
    const PdSplitCrossoverDecision decision) {
  switch (decision) {
    case PdSplitCrossoverDecision::no_active_workload:
      return "no_active_workload";
    case PdSplitCrossoverDecision::prefer_prefill_decode_disaggregation:
      return "prefer_prefill_decode_disaggregation";
    case PdSplitCrossoverDecision::keep_monolithic_or_improve_kv_transport:
      return "keep_monolithic_or_improve_kv_transport";
    case PdSplitCrossoverDecision::
        pd_split_needs_more_queue_pressure_or_overlap:
      return "pd_split_needs_more_queue_pressure_or_overlap";
  }
  throw std::invalid_argument("unknown PD crossover decision");
}

const char* pd_split_crossover_recommendation_name(
    const PdSplitCrossoverRecommendation recommendation) {
  switch (recommendation) {
    case PdSplitCrossoverRecommendation::no_active_workload:
      return "no_active_workload";
    case PdSplitCrossoverRecommendation::
        ready_to_compare_with_product_replay:
      return "ready_to_compare_with_dynamo_mooncake_llmd_replay";
    case PdSplitCrossoverRecommendation::
        collect_product_pd_trace_or_reduce_transfer_cost:
      return "collect_product_pd_trace_or_reduce_transfer_cost_before_claiming_pd_benefit";
  }
  throw std::invalid_argument("unknown PD crossover recommendation");
}

PdSplitCrossoverResult estimate_pd_split_crossover(
    const PdSplitCrossoverAssumptions& assumptions,
    const PdSplitCrossoverScenario& input_scenario) {
  validate_assumptions(assumptions);
  const PdSplitCrossoverScenario scenario =
      validate_scenario(input_scenario);

  PdSplitCrossoverResult result;
  result.scenario = scenario;
  result.product_alignment =
      product_alignment(scenario.product_profile_ids);
  result.transfer = estimate_pd_transfer_overlap(
      assumptions.transfer, scenario.transfer);
  if (result.transfer.prefill_chunks > 0) {
    result.transfer_setup_ms_per_chunk = non_negative_ratio(
        result.transfer.transfer_overhead_ms,
        static_cast<double>(result.transfer.prefill_chunks),
        "PD crossover transfer setup per chunk");
  }
  result.active_workload = result.transfer.active_workload;
  result.ranking_applicable =
      result.transfer.ranking_applicable && result.active_workload;
  if (!result.active_workload) {
    return result;
  }

  result.monolithic_ttft_ms = internal::checked_add_finite_double(
      scenario.monolithic_queue_ms, result.transfer.prefill_compute_ms,
      "PD crossover monolithic TTFT");
  result.pd_ttft_ms = internal::checked_add_finite_double(
      scenario.pd_queue_ms, result.transfer.ttft_ms,
      "PD crossover split TTFT");

  result.monolithic_prefill_interference_uncapped =
      multiply_three(
          result.transfer.prefill_compute_ms,
          static_cast<double>(scenario.concurrent_requests),
          1.0 / assumptions.monolithic_prefill_interference_reference_ms,
          "PD crossover monolithic prefill interference");
  result.monolithic_prefill_interference = std::min(
      assumptions.monolithic_tpot_interference_cap,
      result.monolithic_prefill_interference_uncapped);
  result.pd_decode_stall_pressure_uncapped = non_negative_ratio(
      result.transfer.decode_stall_ms,
      assumptions.pd_decode_stall_reference_ms,
      "PD crossover decode-stall pressure");
  result.pd_decode_stall_pressure = std::min(
      assumptions.pd_tpot_stall_cap,
      result.pd_decode_stall_pressure_uncapped);
  result.monolithic_tpot_ms = finite_product(
      result.transfer.tpot_ms,
      internal::checked_add_finite_double(
          1.0, result.monolithic_prefill_interference,
          "PD crossover monolithic TPOT multiplier"),
      "PD crossover monolithic TPOT");
  result.pd_tpot_ms = finite_product(
      result.transfer.tpot_ms,
      internal::checked_add_finite_double(
          1.0, result.pd_decode_stall_pressure,
          "PD crossover split TPOT multiplier"),
      "PD crossover split TPOT");
  result.ttft_delta_ms = require_finite(
      result.pd_ttft_ms - result.monolithic_ttft_ms,
      "PD crossover TTFT delta");
  result.tpot_delta_ms = require_finite(
      result.pd_tpot_ms - result.monolithic_tpot_ms,
      "PD crossover TPOT delta");

  result.queue_relief_ratio = non_negative_ratio(
      positive_difference(scenario.monolithic_queue_ms,
                          scenario.pd_queue_ms,
                          "PD crossover queue relief"),
      scenario.monolithic_queue_ms,
      "PD crossover queue-relief ratio");
  result.tpot_relief_ratio = non_negative_ratio(
      positive_difference(result.monolithic_tpot_ms,
                          result.pd_tpot_ms,
                          "PD crossover TPOT relief"),
      result.monolithic_tpot_ms,
      "PD crossover TPOT-relief ratio");
  result.ttft_risk_uncapped = non_negative_ratio(
      positive_difference(result.pd_ttft_ms, scenario.slo_ttft_ms,
                          "PD crossover TTFT excess"),
      scenario.slo_ttft_ms, "PD crossover TTFT risk");
  result.ttft_risk = finite_unit_interval(
      result.ttft_risk_uncapped, "PD crossover bounded TTFT risk");

  const double queue_component = finite_product(
      result.queue_relief_ratio, assumptions.queue_relief_weight,
      "PD crossover queue-relief score component");
  const double tpot_component = finite_product(
      result.tpot_relief_ratio, assumptions.tpot_relief_weight,
      "PD crossover TPOT-relief score component");
  const double overlap_component = finite_product(
      scenario.transfer.overlap_efficiency,
      assumptions.overlap_efficiency_weight,
      "PD crossover overlap score component");
  result.capacity_relief_score_uncapped =
      internal::checked_add_finite_double(
          internal::checked_add_finite_double(
              queue_component, tpot_component,
              "PD crossover capacity score subtotal"),
          overlap_component, "PD crossover capacity score");
  result.capacity_relief_score = finite_unit_interval(
      result.capacity_relief_score_uncapped,
      "PD crossover bounded capacity score");

  const double capacity_component = finite_product(
      result.capacity_relief_score, assumptions.capacity_relief_weight,
      "PD crossover capacity score component");
  const double headroom_component = finite_product(
      1.0 - result.ttft_risk, assumptions.ttft_headroom_weight,
      "PD crossover TTFT-headroom component");
  const double tpot_bonus = result.tpot_delta_ms < 0.0
      ? assumptions.tpot_improvement_bonus
      : assumptions.no_tpot_improvement_bonus;
  result.crossover_score_uncapped =
      internal::checked_add_finite_double(
          internal::checked_add_finite_double(
              capacity_component, headroom_component,
              "PD crossover score subtotal"),
          tpot_bonus, "PD crossover score");
  result.crossover_score = finite_unit_interval(
      result.crossover_score_uncapped,
      "PD crossover bounded score");

  const double transfer_bottleneck_ms = finite_product(
      result.transfer.prefill_compute_ms,
      assumptions.transfer_bottleneck_to_prefill_ratio_threshold,
      "PD crossover transfer bottleneck threshold");
  result.bottleneck =
      result.transfer.transfer_exposed_ms > transfer_bottleneck_ms
          ? PdSplitCrossoverBottleneck::kv_transfer_path
          : PdSplitCrossoverBottleneck::
                prefill_compute_or_scheduler_queue;
  result.decision = result.crossover_score >=
                            assumptions.crossover_threshold
      ? PdSplitCrossoverDecision::prefer_prefill_decode_disaggregation
      : (result.bottleneck == PdSplitCrossoverBottleneck::kv_transfer_path
             ? PdSplitCrossoverDecision::
                   keep_monolithic_or_improve_kv_transport
             : PdSplitCrossoverDecision::
                   pd_split_needs_more_queue_pressure_or_overlap);
  result.recommendation =
      result.decision ==
              PdSplitCrossoverDecision::
                  prefer_prefill_decode_disaggregation
          ? PdSplitCrossoverRecommendation::
                ready_to_compare_with_product_replay
          : PdSplitCrossoverRecommendation::
                collect_product_pd_trace_or_reduce_transfer_cost;
  result.ttft_slo_met = result.pd_ttft_ms <= scenario.slo_ttft_ms;
  result.tpot_slo_met = result.pd_tpot_ms <= scenario.slo_tpot_ms;
  return result;
}

PdSplitCrossoverPlan build_pd_split_crossover_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "PD crossover scenario inventory must not be empty");
  }

  PdSplitCrossoverPlan plan;
  plan.assumptions = assumptions;
  plan.scenarios.reserve(scenarios.size());
  plan.records.reserve(scenarios.size());
  std::vector<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& input : scenarios) {
    const PdSplitCrossoverScenario scenario = validate_scenario(input);
    names.push_back(scenario.name);
    plan.scenarios.push_back(scenario);
    const PdSplitCrossoverResult record =
        estimate_pd_split_crossover(assumptions, scenario);
    for (const auto& product : record.product_alignment) {
      if (std::find(plan.product_alignment.begin(),
                    plan.product_alignment.end(), product) ==
          plan.product_alignment.end()) {
        plan.product_alignment.push_back(product);
      }
    }
    plan.records.push_back(record);
  }
  require_unique_nonempty_strings(names,
                                  "PD crossover scenario identities");

  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    if (record.active_workload) {
      plan.active_record_count = internal::checked_add_size(
          plan.active_record_count, 1U,
          "PD crossover active record count");
    }
    if (!record.ranking_applicable) continue;
    plan.comparable_record_count = internal::checked_add_size(
        plan.comparable_record_count, 1U,
        "PD crossover comparable record count");
    if (record.decision ==
        PdSplitCrossoverDecision::prefer_prefill_decode_disaggregation) {
      plan.preferred_pd_record_count = internal::checked_add_size(
          plan.preferred_pd_record_count, 1U,
          "PD crossover preferred-PD record count");
    }
    if (!plan.best_crossover_record_index.has_value() ||
        record.crossover_score >
            plan.records[*plan.best_crossover_record_index]
                .crossover_score) {
      plan.best_crossover_record_index = index;
    }
    if (!plan.most_decode_blocked_record_index.has_value() ||
        record.transfer.decode_stall_ms >
            plan.records[*plan.most_decode_blocked_record_index]
                .transfer.decode_stall_ms) {
      plan.most_decode_blocked_record_index = index;
    }
  }
  if (plan.scenarios.size() != plan.records.size()) {
    throw std::logic_error(
        "PD crossover scenario/record cardinality drift");
  }
  return plan;
}

void validate_pd_split_crossover_plan(
    const PdSplitCrossoverPlan& plan) {
  const PdSplitCrossoverPlan expected =
      build_pd_split_crossover_plan(plan.assumptions, plan.scenarios);
  if (!same_assumptions(plan.assumptions, expected.assumptions) ||
      plan.scenarios.size() != expected.scenarios.size() ||
      plan.product_alignment != expected.product_alignment ||
      plan.records.size() != expected.records.size() ||
      plan.active_record_count != expected.active_record_count ||
      plan.comparable_record_count != expected.comparable_record_count ||
      plan.preferred_pd_record_count !=
          expected.preferred_pd_record_count ||
      plan.best_crossover_record_index !=
          expected.best_crossover_record_index ||
      plan.most_decode_blocked_record_index !=
          expected.most_decode_blocked_record_index) {
    throw std::logic_error("PD crossover plan summary drift");
  }
  for (std::size_t index = 0; index < plan.scenarios.size(); ++index) {
    if (!same_scenario(plan.scenarios[index], expected.scenarios[index]) ||
        !same_result(plan.records[index], expected.records[index])) {
      throw std::logic_error("PD crossover plan record drift");
    }
  }
}

}  // namespace inferlab
