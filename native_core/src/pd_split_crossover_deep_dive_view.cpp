#include "pd_split_crossover_deep_dive_view.hpp"

#include "checked_index.hpp"
#include "native_trace_context_view.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::pd_split_crossover_deep_dive_view {
namespace {

constexpr const char* kArtifactType =
    "native_pd_split_crossover_deep_dive";
constexpr const char* kArtifactPath =
    "results/raw/native_pd_split_crossover_deep_dive.json";
constexpr const char* kTargetName =
    "inferlab_pd_split_crossover_deep_dive";
constexpr const char* kRefreshCommand =
    "./build/inferlab_pd_split_crossover_deep_dive --repo-root . --output results/raw/native_pd_split_crossover_deep_dive.json";
constexpr const char* kClaimScope =
    "compiled native prefill/decode crossover analysis with optional explicitly supplied trace, clock, and span-lifecycle projection; not measured distributed serving, inferred causality, product replay/execute, benchmark, or release evidence";

const PdSplitCrossoverResult* selected_record(
    const PdSplitCrossoverPlan& plan,
    const std::optional<std::size_t>& index,
    const char* name) {
  if (!index.has_value()) return nullptr;
  if (*index >= plan.records.size()) {
    throw std::logic_error(std::string("PD crossover ") + name +
                           " index is out of range");
  }
  return &plan.records[*index];
}

std::string joined_names(const std::vector<std::string>& values) {
  std::ostringstream out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0U) out << " / ";
    out << values[index];
  }
  return out.str();
}

std::string assumptions_json(
    const PdSplitCrossoverAssumptions& assumptions,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "PD crossover assumptions JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"capacity_relief_weight\": "
      << inferlab_plan::fixed6(assumptions.capacity_relief_weight)
      << ",\n";
  out << nested << "\"crossover_threshold\": "
      << inferlab_plan::fixed6(assumptions.crossover_threshold)
      << ",\n";
  out << nested << "\"decode_requires_complete_prompt_kv\": "
      << inferlab_plan::json_bool(
             assumptions.transfer.decode_requires_complete_prompt_kv)
      << ",\n";
  out << nested << "\"monolithic_prefill_interference_reference_ms\": "
      << inferlab_plan::fixed6(
             assumptions.monolithic_prefill_interference_reference_ms)
      << ",\n";
  out << nested << "\"monolithic_tpot_interference_cap\": "
      << inferlab_plan::fixed6(
             assumptions.monolithic_tpot_interference_cap)
      << ",\n";
  out << nested << "\"no_tpot_improvement_bonus\": "
      << inferlab_plan::fixed6(
             assumptions.no_tpot_improvement_bonus)
      << ",\n";
  out << nested << "\"overlap_efficiency_is_streaming_readiness\": "
      << inferlab_plan::json_bool(
             assumptions.transfer.overlap_efficiency_is_streaming_readiness)
      << ",\n";
  out << nested << "\"overlap_efficiency_weight\": "
      << inferlab_plan::fixed6(
             assumptions.overlap_efficiency_weight)
      << ",\n";
  out << nested << "\"pd_decode_stall_reference_ms\": "
      << inferlab_plan::fixed6(
             assumptions.pd_decode_stall_reference_ms)
      << ",\n";
  out << nested << "\"pd_tpot_stall_cap\": "
      << inferlab_plan::fixed6(assumptions.pd_tpot_stall_cap)
      << ",\n";
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << ",\n";
  out << nested << "\"queue_relief_weight\": "
      << inferlab_plan::fixed6(assumptions.queue_relief_weight)
      << ",\n";
  out << nested << "\"sender_receiver_overhead_is_per_chunk\": "
      << inferlab_plan::json_bool(
             assumptions.transfer.sender_receiver_overhead_is_per_chunk)
      << ",\n";
  out << nested << "\"tpot_improvement_bonus\": "
      << inferlab_plan::fixed6(assumptions.tpot_improvement_bonus)
      << ",\n";
  out << nested << "\"tpot_relief_weight\": "
      << inferlab_plan::fixed6(assumptions.tpot_relief_weight)
      << ",\n";
  out << nested << "\"transfer_assumption_profile\": "
      << inferlab_plan::json_string(assumptions.transfer.profile_name)
      << ",\n";
  out << nested << "\"transfer_bottleneck_to_prefill_ratio_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.transfer_bottleneck_to_prefill_ratio_threshold)
      << ",\n";
  out << nested << "\"ttft_headroom_weight\": "
      << inferlab_plan::fixed6(assumptions.ttft_headroom_weight)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const PdSplitCrossoverResult& result,
                          const int indent) {
  const auto& scenario = result.scenario;
  const auto& transfer = scenario.transfer;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "PD crossover scenario JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"concurrent_requests\": "
      << scenario.concurrent_requests << ",\n";
  out << nested << "\"decode_ms_per_token\": "
      << inferlab_plan::fixed6(transfer.decode_ms_per_token) << ",\n";
  out << nested << "\"decode_tokens\": "
      << transfer.output_tokens << ",\n";
  out << nested << "\"kv_bytes_per_token\": "
      << inferlab_plan::fixed6(transfer.kv_bytes_per_token) << ",\n";
  out << nested << "\"monolithic_queue_ms\": "
      << inferlab_plan::fixed6(scenario.monolithic_queue_ms) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"overlap_ratio\": "
      << inferlab_plan::fixed6(transfer.overlap_efficiency) << ",\n";
  out << nested << "\"overlap_ratio_scope\": "
      << inferlab_plan::json_string(
             "legacy alias of canonical streaming overlap efficiency")
      << ",\n";
  out << nested << "\"pd_queue_ms\": "
      << inferlab_plan::fixed6(scenario.pd_queue_ms) << ",\n";
  out << nested << "\"prefill_ms_per_token\": "
      << inferlab_plan::fixed6(transfer.prefill_ms_per_token) << ",\n";
  out << nested << "\"prefill_tokens\": "
      << transfer.prompt_tokens << ",\n";
  out << nested << "\"product_alignment\": "
      << inferlab_plan::json_string(joined_names(result.product_alignment))
      << ",\n";
  out << nested << "\"product_alignment_names\": "
      << inferlab_plan::json_string_array(result.product_alignment,
                                          nested_indent)
      << ",\n";
  out << nested << "\"product_profile_ids\": "
      << inferlab_plan::json_string_array(scenario.product_profile_ids,
                                          nested_indent)
      << ",\n";
  out << nested << "\"slo_tpot_ms\": "
      << inferlab_plan::fixed6(scenario.slo_tpot_ms) << ",\n";
  out << nested << "\"slo_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.slo_ttft_ms) << ",\n";
  out << nested << "\"transfer_bandwidth_gbps\": "
      << inferlab_plan::fixed6(transfer.link_bandwidth_gbps) << ",\n";
  out << nested << "\"transfer_setup_ms\": "
      << inferlab_plan::fixed6(result.transfer_setup_ms_per_chunk)
      << ",\n";
  out << nested << "\"transfer_setup_scope\": "
      << inferlab_plan::json_string(
             "single compiled chunk sender/receiver overhead") << "\n";
  out << pad << "}";
  return out.str();
}

std::string stage_json(const PdTransferStage& stage,
                       const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "PD crossover stage JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"chunk_index\": " << stage.chunk_index << ",\n";
  out << nested << "\"duration_ms\": "
      << inferlab_plan::fixed6(stage.duration_ms) << ",\n";
  out << nested << "\"end_ms\": "
      << inferlab_plan::fixed6(stage.end_ms) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(
             pd_transfer_stage_kind_name(stage.kind)) << ",\n";
  out << nested << "\"resource\": "
      << inferlab_plan::json_string(
             pd_transfer_resource_kind_name(stage.resource)) << ",\n";
  out << nested << "\"start_ms\": "
      << inferlab_plan::fixed6(stage.start_ms) << ",\n";
  out << nested << "\"token_count\": " << stage.token_count << "\n";
  out << pad << "}";
  return out.str();
}

std::string stages_json(const std::vector<PdTransferStage>& stages,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int item_indent = internal::checked_add_int(
      indent, 2, "PD crossover stage-list JSON indent");
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < stages.size(); ++index) {
    out << stage_json(stages[index], item_indent)
        << (index + 1U == stages.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string legacy_event_json(const std::string& lane,
                              const std::string& name,
                              const double start_ms,
                              const double duration_ms,
                              const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "PD crossover legacy event JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"duration_ms\": "
      << inferlab_plan::fixed6(duration_ms) << ",\n";
  out << nested << "\"lane\": "
      << inferlab_plan::json_string(lane) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(name) << ",\n";
  out << nested << "\"start_ms\": "
      << inferlab_plan::fixed6(start_ms) << "\n";
  out << pad << "}";
  return out.str();
}

const PdTransferStage* first_transfer_stage(
    const PdTransferOverlapResult& transfer) {
  for (const auto& stage : transfer.stages) {
    if (stage.kind == PdTransferStageKind::kv_send_recv) return &stage;
  }
  if (transfer.transfer_required) {
    throw std::logic_error(
        "PD crossover transfer-required record has no transfer stage");
  }
  return nullptr;
}

std::string legacy_timeline_json(
    const PdSplitCrossoverResult& result,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int item_indent = internal::checked_add_int(
      indent, 2, "PD crossover legacy timeline JSON indent");
  const PdTransferStage* transfer_stage =
      first_transfer_stage(result.transfer);
  std::ostringstream out;
  out << "[\n";
  out << legacy_event_json(
             "prefill_worker", "prefill_compute", 0.0,
             result.transfer.prefill_compute_ms, item_indent)
      << ",\n";
  out << legacy_event_json(
             "kv_transfer", "kv_send_recv",
             transfer_stage == nullptr ? 0.0 : transfer_stage->start_ms,
             result.transfer.transfer_pipeline_span_ms, item_indent)
      << ",\n";
  out << legacy_event_json(
             "decode_worker", "decode_wait_for_kv",
             result.transfer.prefill_compute_ms,
             result.transfer.decode_stall_ms, item_indent)
      << ",\n";
  out << legacy_event_json(
             "decode_worker", "decode_tokens",
             result.transfer.full_prompt_kv_ready_ms,
             result.transfer.decode_compute_ms, item_indent)
      << "\n";
  out << pad << "]";
  return out.str();
}

std::string result_json(const PdSplitCrossoverResult& result,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "PD crossover result JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"active_workload\": "
      << inferlab_plan::json_bool(result.active_workload) << ",\n";
  out << nested << "\"capacity_relief_score\": "
      << inferlab_plan::fixed6(result.capacity_relief_score) << ",\n";
  out << nested << "\"capacity_relief_score_uncapped\": "
      << inferlab_plan::fixed6(result.capacity_relief_score_uncapped)
      << ",\n";
  out << nested << "\"canonical_transfer_stages\": "
      << stages_json(result.transfer.stages, nested_indent) << ",\n";
  out << nested << "\"crossover_decision\": "
      << inferlab_plan::json_string(
             pd_split_crossover_decision_name(result.decision))
      << ",\n";
  out << nested << "\"crossover_score\": "
      << inferlab_plan::fixed6(result.crossover_score) << ",\n";
  out << nested << "\"crossover_score_uncapped\": "
      << inferlab_plan::fixed6(result.crossover_score_uncapped)
      << ",\n";
  out << nested << "\"decode_blocked_ms\": "
      << inferlab_plan::fixed6(result.transfer.decode_stall_ms) << ",\n";
  out << nested << "\"decode_blocked_scope\": "
      << inferlab_plan::json_string(
             "canonical exposed transfer tail after prefill completion")
      << ",\n";
  out << nested << "\"decode_gpu_busy_ms\": "
      << inferlab_plan::fixed6(result.transfer.decode_compute_ms) << ",\n";
  out << nested << "\"full_prompt_kv_ready_ms\": "
      << inferlab_plan::fixed6(result.transfer.full_prompt_kv_ready_ms)
      << ",\n";
  out << nested << "\"kv_bytes_to_transfer\": "
      << inferlab_plan::fixed6(result.transfer.prompt_kv_bytes) << ",\n";
  out << nested << "\"kv_transfer_ms\": "
      << inferlab_plan::fixed6(result.transfer.kv_transfer_total_ms)
      << ",\n";
  out << nested << "\"monolithic_prefill_interference\": "
      << inferlab_plan::fixed6(
             result.monolithic_prefill_interference) << ",\n";
  out << nested << "\"monolithic_prefill_interference_uncapped\": "
      << inferlab_plan::fixed6(
             result.monolithic_prefill_interference_uncapped) << ",\n";
  out << nested << "\"monolithic_tpot_ms\": "
      << inferlab_plan::fixed6(result.monolithic_tpot_ms) << ",\n";
  out << nested << "\"monolithic_ttft_ms\": "
      << inferlab_plan::fixed6(result.monolithic_ttft_ms) << ",\n";
  out << nested << "\"network_or_storage_bottleneck\": "
      << inferlab_plan::json_string(
             pd_split_crossover_bottleneck_name(result.bottleneck))
      << ",\n";
  out << nested << "\"pd_decode_stall_pressure\": "
      << inferlab_plan::fixed6(result.pd_decode_stall_pressure)
      << ",\n";
  out << nested << "\"pd_decode_stall_pressure_uncapped\": "
      << inferlab_plan::fixed6(
             result.pd_decode_stall_pressure_uncapped) << ",\n";
  out << nested << "\"pd_tpot_ms\": "
      << inferlab_plan::fixed6(result.pd_tpot_ms) << ",\n";
  out << nested << "\"pd_ttft_ms\": "
      << inferlab_plan::fixed6(result.pd_ttft_ms) << ",\n";
  out << nested << "\"prefill_decode_timeline\": "
      << legacy_timeline_json(result, nested_indent) << ",\n";
  out << nested << "\"prefill_decode_timeline_scope\": "
      << inferlab_plan::json_string(
             "legacy aggregate lanes; canonical_transfer_stages is authoritative")
      << ",\n";
  out << nested << "\"prefill_gpu_busy_ms\": "
      << inferlab_plan::fixed6(result.transfer.prefill_compute_ms)
      << ",\n";
  out << nested << "\"queue_relief_ratio\": "
      << inferlab_plan::fixed6(result.queue_relief_ratio) << ",\n";
  out << nested << "\"ranking_applicable\": "
      << inferlab_plan::json_bool(result.ranking_applicable) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             pd_split_crossover_recommendation_name(
                 result.recommendation)) << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result, nested_indent) << ",\n";
  out << nested << "\"tpot_delta_ms\": "
      << inferlab_plan::fixed6(result.tpot_delta_ms) << ",\n";
  out << nested << "\"tpot_relief_ratio\": "
      << inferlab_plan::fixed6(result.tpot_relief_ratio) << ",\n";
  out << nested << "\"tpot_slo_met\": "
      << inferlab_plan::json_bool(result.tpot_slo_met) << ",\n";
  out << nested << "\"transfer_exposed_ms\": "
      << inferlab_plan::fixed6(result.transfer.transfer_exposed_ms)
      << ",\n";
  out << nested << "\"transfer_hidden_ms\": "
      << inferlab_plan::fixed6(result.transfer.transfer_hidden_ms)
      << ",\n";
  out << nested << "\"ttft_delta_ms\": "
      << inferlab_plan::fixed6(result.ttft_delta_ms) << ",\n";
  out << nested << "\"ttft_risk\": "
      << inferlab_plan::fixed6(result.ttft_risk) << ",\n";
  out << nested << "\"ttft_risk_uncapped\": "
      << inferlab_plan::fixed6(result.ttft_risk_uncapped) << ",\n";
  out << nested << "\"ttft_slo_met\": "
      << inferlab_plan::json_bool(result.ttft_slo_met) << "\n";
  out << pad << "}";
  return out.str();
}

std::string optional_name(
    const PdSplitCrossoverPlan& plan,
    const std::optional<std::size_t>& index,
    const char* name) {
  const auto* record = selected_record(plan, index, name);
  return record == nullptr
             ? "null"
             : inferlab_plan::json_string(record->scenario.name);
}

std::string optional_metric(
    const PdSplitCrossoverPlan& plan,
    const std::optional<std::size_t>& index,
    const char* name,
    const double PdSplitCrossoverResult::* member) {
  const auto* record = selected_record(plan, index, name);
  return record == nullptr
             ? "null"
             : inferlab_plan::fixed6(record->*member);
}

std::string optional_decode_stall(
    const PdSplitCrossoverPlan& plan,
    const std::optional<std::size_t>& index) {
  const auto* record = selected_record(
      plan, index, "most decode-blocked");
  return record == nullptr
             ? "null"
             : inferlab_plan::fixed6(
                   record->transfer.decode_stall_ms);
}

}  // namespace

std::string artifact_json(
    const pd_split_crossover_deep_dive::Plan& artifact_plan) {
  pd_split_crossover_deep_dive::validate_plan(artifact_plan);
  const PdSplitCrossoverPlan& plan = artifact_plan.crossover;
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope, plan.product_alignment,
      kTargetName, kArtifactPath, kRefreshCommand);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"deep_dive_type\": \"pd_split_crossover\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"evidence_level\": \"dry_run_ready\",\n";
  out << "  \"artifact_path\": "
      << inferlab_plan::json_string(kArtifactPath) << ",\n";
  out << "  \"target\": "
      << inferlab_plan::json_string(kTargetName) << ",\n";
  out << "  \"refresh_command\": "
      << inferlab_plan::json_string(kRefreshCommand) << ",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"compute_baseline\": \"inferlab::estimate_disaggregated_serving\",\n";
  out << "    \"transfer_timeline\": \"advanced_serving_pd_transfer_overlap.cpp\",\n";
  out << "    \"crossover_model\": \"advanced_serving_pd_split_crossover.cpp\",\n";
  out << "    \"fixture\": \"advanced_serving_pd_split_crossover_fixture.cpp\",\n";
  out << "    \"product_identity\": \"mature_product_profile_registry.hpp\",\n";
  out << "    \"trace_composition\": \"pd_split_crossover_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"trace_contract\": \"native_trace_context.hpp/.cpp\",\n";
  out << "    \"artifact_projection\": \"pd_split_crossover_deep_dive_view.hpp/.cpp\",\n";
  out << "    \"cli_request_output\": \"pd_split_crossover_deep_dive_main.cpp\"\n";
  out << "  },\n";
  out << "  \"fixture\": {\n";
  out << "    \"source\": \"compiled_defaults\",\n";
  out << "    \"modeled_not_measured\": true,\n";
  out << "    \"canonical_transfer_timeline_composed\": true,\n";
  out << "    \"product_runtime_executed\": false\n";
  out << "  },\n";
  out << "  \"config_reference\": {\n";
  out << "    \"present\": false,\n";
  out << "    \"consumed_by_current_cli\": false\n";
  out << "  },\n";
  out << "  \"unit_contract\": {\n";
  out << "    \"payload_storage\": \"bytes_and_binary_GiB\",\n";
  out << "    \"link_bandwidth\": \"decimal_Gbps\",\n";
  out << "    \"time\": \"milliseconds\",\n";
  out << "    \"overhead_input\": \"microseconds_per_chunk\"\n";
  out << "  },\n";
  out << "  \"model_assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(
             {"prefill/decode disaggregation",
              "KV transfer overlap",
              "goodput/SLO crossover",
              "Mooncake/Dynamo/llm-d serving split"}, 2)
      << ",\n";
  out << "  \"research_basis_scope\": "
      << inferlab_plan::json_string(
             "mature product alignment only; no external product core was reimplemented or executed")
      << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(plan.product_alignment, 2)
      << ",\n";
  out << "  \"native_trace_context\": "
      << inferlab_native_trace::trace_context_projection_json(
             artifact_plan.trace_context, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"active_scenario_count\": "
      << plan.active_record_count << ",\n";
  out << "    \"best_scenario\": "
      << optional_name(plan, plan.best_crossover_record_index,
                       "best crossover") << ",\n";
  out << "    \"best_crossover_score\": "
      << optional_metric(plan, plan.best_crossover_record_index,
                         "best crossover",
                         &PdSplitCrossoverResult::crossover_score)
      << ",\n";
  out << "    \"comparable_scenario_count\": "
      << plan.comparable_record_count << ",\n";
  out << "    \"most_decode_blocked_scenario\": "
      << optional_name(plan, plan.most_decode_blocked_record_index,
                       "most decode-blocked") << ",\n";
  out << "    \"most_decode_blocked_ms\": "
      << optional_decode_stall(
             plan, plan.most_decode_blocked_record_index) << ",\n";
  out << "    \"preferred_pd_scenario_count\": "
      << plan.preferred_pd_record_count << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(
             {"results/raw/native_scheduler_batching_deep_dive.json",
              "results/raw/native_kv_tier_decision_deep_dive.json",
              "results/raw/native_kv_lifecycle_deep_dive.json",
              "results/raw/product_dynamo_serving_profile.json",
              "results/raw/product_mooncake_kv_profile.json",
              "results/raw/product_kserve_llmd_profile.json"}, 2)
      << ",\n";
  out << "  \"evidence_bridge_input_scope\": "
      << inferlab_plan::json_string(
             "declared follow-up evidence sources; this compiled model does not read them")
      << ",\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << result_json(plan.records[index], 4)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({kRefreshCommand}, 2) << ",\n";
  out << "  \"claim_boundary\": {\n";
  out << "    \"compiled_fixture_only\": true,\n";
  out << "    \"transfer_timeline_is_canonical_analytical_model\": true,\n";
  out << "    \"optional_trace_sidecar_supported\": true,\n";
  out << "    \"trace_source_supplied\": "
      << inferlab_plan::json_bool(
             artifact_plan.trace_context.source.supplied)
      << ",\n";
  out << "    \"product_replay_or_execute_proven\": false,\n";
  out << "    \"distributed_serving_measured\": false,\n";
  out << "    \"performance_or_release_claim_allowed\": false\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::pd_split_crossover_deep_dive_view
