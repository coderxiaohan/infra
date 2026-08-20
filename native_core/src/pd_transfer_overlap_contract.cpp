#include "pd_transfer_overlap_contract.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace inferlab::pd_transfer_overlap_contract {
namespace {

using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

bool approximately_equal(const double lhs, const double rhs) {
  const double scale = std::max({1.0, std::fabs(lhs), std::fabs(rhs)});
  return std::fabs(lhs - rhs) <= scale * kComparisonTolerance;
}

void validate_stage(const PdTransferStage& stage) {
  (void)pd_transfer_stage_kind_name(stage.kind);
  (void)pd_transfer_resource_kind_name(stage.resource);
  if (stage.chunk_index < 0 || stage.token_count <= 0) {
    throw std::invalid_argument(
        "PD transfer stage identity contract failed");
  }
  require_non_negative_finite(stage.start_ms,
                              "PD transfer stage start");
  require_non_negative_finite(stage.duration_ms,
                              "PD transfer stage duration");
  require_non_negative_finite(stage.end_ms,
                              "PD transfer stage end");
  if (!approximately_equal(
          internal::checked_add_finite_double(
              stage.start_ms, stage.duration_ms,
              "PD transfer stage end reconciliation"),
          stage.end_ms)) {
    throw std::invalid_argument(
        "PD transfer stage end contract failed");
  }
}

struct StageInventory {
  std::size_t prefill_count = 0U;
  std::size_t transfer_count = 0U;
  std::size_t decode_count = 0U;
  int prefill_tokens = 0;
  int transfer_tokens = 0;
  int decode_tokens = 0;
  double latest_transfer_end_ms = 0.0;
};

void require_stage_identity(const PdTransferStage& stage,
                            const std::size_t expected_index,
                            const int expected_tokens,
                            const char* name) {
  if (stage.chunk_index !=
          internal::checked_int_from_size(expected_index, name) ||
      stage.token_count != expected_tokens) {
    throw std::invalid_argument(
        std::string(name) + " identity contract failed");
  }
}

StageInventory validate_stage_inventory(
    const PdTransferOverlapResult& result) {
  StageInventory inventory;
  for (const auto& stage : result.stages) {
    validate_stage(stage);
    switch (stage.kind) {
      case PdTransferStageKind::prefill_chunk:
        if (stage.resource !=
            PdTransferResourceKind::prefill_worker) {
          throw std::invalid_argument(
              "PD transfer prefill stage resource mismatch");
        }
        require_stage_identity(
            stage, inventory.prefill_count,
            chunk_tokens_at(
                result.scenario.prompt_tokens,
                result.scenario.prefill_chunk_tokens,
                stage.chunk_index,
                "PD transfer prefill stage"),
            "PD transfer prefill stage");
        inventory.prefill_count = internal::checked_add_size(
            inventory.prefill_count, 1U,
            "PD transfer prefill stage count");
        inventory.prefill_tokens = internal::checked_add_int(
            inventory.prefill_tokens, stage.token_count,
            "PD transfer prefill stage tokens");
        break;
      case PdTransferStageKind::kv_send_recv:
        if (stage.resource !=
            PdTransferResourceKind::transfer_link) {
          throw std::invalid_argument(
              "PD transfer link stage resource mismatch");
        }
        require_stage_identity(
            stage, inventory.transfer_count,
            chunk_tokens_at(
                result.scenario.prompt_tokens,
                result.scenario.prefill_chunk_tokens,
                stage.chunk_index,
                "PD transfer transfer stage"),
            "PD transfer transfer stage");
        inventory.transfer_count = internal::checked_add_size(
            inventory.transfer_count, 1U,
            "PD transfer transfer stage count");
        inventory.transfer_tokens = internal::checked_add_int(
            inventory.transfer_tokens, stage.token_count,
            "PD transfer transfer stage tokens");
        inventory.latest_transfer_end_ms =
            std::max(inventory.latest_transfer_end_ms, stage.end_ms);
        break;
      case PdTransferStageKind::decode_chunk:
        if (stage.resource !=
                PdTransferResourceKind::decode_worker ||
            internal::checked_add_finite_double(
                stage.start_ms, kComparisonTolerance,
                "PD transfer decode readiness tolerance") <
                result.full_prompt_kv_ready_ms) {
          throw std::invalid_argument(
              "PD transfer decode stage readiness contract failed");
        }
        require_stage_identity(
            stage, inventory.decode_count,
            chunk_tokens_at(
                result.scenario.output_tokens,
                result.scenario.decode_chunk_tokens,
                stage.chunk_index,
                "PD transfer decode stage"),
            "PD transfer decode stage");
        inventory.decode_count = internal::checked_add_size(
            inventory.decode_count, 1U,
            "PD transfer decode stage count");
        inventory.decode_tokens = internal::checked_add_int(
            inventory.decode_tokens, stage.token_count,
            "PD transfer decode stage tokens");
        break;
    }
  }
  return inventory;
}

void validate_result_shape(const PdTransferOverlapResult& result) {
  const bool active_workload =
      result.scenario.prompt_tokens > 0 ||
      result.scenario.output_tokens > 0;
  const bool transfer_required =
      result.scenario.topology ==
          PdTransferTopology::prefill_decode_split &&
      result.scenario.prompt_tokens > 0 &&
      result.scenario.kv_bytes_per_token > 0.0;
  const bool ranking_applicable =
      transfer_required && result.scenario.output_tokens > 0;
  const int prefill_chunks =
      internal::checked_ceil_div_non_negative_int(
          result.scenario.prompt_tokens,
          result.scenario.prefill_chunk_tokens,
          "PD transfer prefill chunk count");
  const int decode_chunks =
      internal::checked_ceil_div_non_negative_int(
          result.scenario.output_tokens,
          result.scenario.decode_chunk_tokens,
          "PD transfer decode chunk count");

  if (result.active_workload != active_workload ||
      result.transfer_required != transfer_required ||
      result.ranking_applicable != ranking_applicable ||
      result.prefill_chunks != prefill_chunks ||
      result.decode_chunks != decode_chunks ||
      result.prompt_kv_token_count !=
          static_cast<std::int64_t>(
              result.scenario.prompt_tokens)) {
    throw std::invalid_argument(
        "PD transfer result shape contract failed");
  }
}

void validate_metric_domain(const PdTransferOverlapResult& result) {
  const double values[] = {
      result.prompt_kv_bytes,
      result.prompt_kv_gib,
      result.prefill_compute_ms,
      result.decode_compute_ms,
      result.aggregated_baseline_ms,
      result.transfer_payload_serialization_ms,
      result.transfer_overhead_ms,
      result.kv_transfer_total_ms,
      result.transfer_pipeline_span_ms,
      result.transfer_link_busy_ratio,
      result.full_prompt_kv_ready_ms,
      result.disaggregated_no_overlap_ms,
      result.overlapped_end_to_end_ms,
      result.transfer_exposed_ms,
      result.transfer_hidden_ms,
      result.decode_stall_ms,
      result.ttft_ms,
      result.tpot_ms,
      result.overlap_gain_ms,
      result.speedup_vs_no_overlap,
      result.speedup_vs_aggregated,
      result.fabric_utilization_score,
  };
  for (const double value : values) {
    require_non_negative_finite(
        value, "PD transfer result metric");
  }
  require_unit_interval(
      result.transfer_link_busy_ratio,
      "PD transfer link busy ratio");
  require_unit_interval(
      result.fabric_utilization_score,
      "PD transfer fabric utilization score");
}

void validate_stage_totals(
    const PdTransferOverlapResult& result,
    const StageInventory& inventory) {
  if (inventory.prefill_count !=
          static_cast<std::size_t>(result.prefill_chunks) ||
      inventory.decode_count !=
          static_cast<std::size_t>(result.decode_chunks) ||
      inventory.transfer_count !=
          (result.transfer_required
               ? inventory.prefill_count
               : 0U) ||
      inventory.prefill_tokens !=
          result.scenario.prompt_tokens ||
      inventory.decode_tokens !=
          result.scenario.output_tokens ||
      inventory.transfer_tokens !=
          (result.transfer_required
               ? result.scenario.prompt_tokens
               : 0)) {
    throw std::invalid_argument(
        "PD transfer stage inventory contract failed");
  }
}

void validate_transfer_accounting(
    const PdTransferOverlapResult& result,
    const StageInventory& inventory) {
  const double transfer_components =
      internal::checked_add_finite_double(
          result.transfer_payload_serialization_ms,
          result.transfer_overhead_ms,
          "PD transfer transfer components");
  const double visibility_components =
      internal::checked_add_finite_double(
          result.transfer_hidden_ms,
          result.transfer_exposed_ms,
          "PD transfer visibility components");
  if (!approximately_equal(
          transfer_components, result.kv_transfer_total_ms) ||
      !approximately_equal(
          visibility_components, result.kv_transfer_total_ms) ||
      !approximately_equal(
          result.fabric_utilization_score,
          result.transfer_link_busy_ratio)) {
    throw std::invalid_argument(
        "PD transfer transfer accounting contract failed");
  }

  if (result.transfer_required) {
    if (inventory.transfer_count == 0U ||
        inventory.latest_transfer_end_ms >
            internal::checked_add_finite_double(
                result.full_prompt_kv_ready_ms,
                kComparisonTolerance,
                "PD transfer readiness tolerance") ||
        internal::checked_add_finite_double(
            result.full_prompt_kv_ready_ms,
            kComparisonTolerance,
            "PD transfer prefill readiness tolerance") <
            result.prefill_compute_ms) {
      throw std::invalid_argument(
          "PD transfer complete-prompt-KV readiness contract failed");
    }
  } else if (result.kv_transfer_total_ms != 0.0 ||
             result.transfer_exposed_ms != 0.0 ||
             result.transfer_hidden_ms != 0.0 ||
             result.transfer_pipeline_span_ms != 0.0 ||
             result.transfer_link_busy_ratio != 0.0) {
    throw std::invalid_argument(
        "PD transfer no-transfer record carries transfer facts");
  }
}

}  // namespace

void validate_assumptions(
    const PdTransferOverlapAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "PD transfer assumption profile");
  require_positive_finite(assumptions.bytes_per_gib,
                          "PD transfer bytes per GiB");
  if (std::fabs(
          assumptions.bytes_per_gib -
          runtime_communication_internal::kBytesPerGib) >
      kComparisonTolerance) {
    throw std::invalid_argument(
        "PD transfer bytes-per-GiB assumption must match the canonical unit owner");
  }
  require_non_negative_finite(
      assumptions.high_decode_stall_to_decode_ratio_threshold,
      "PD transfer high decode-stall threshold");
  require_unit_interval(
      assumptions.high_transfer_link_busy_ratio_threshold,
      "PD transfer high link-busy threshold");
  require_positive_finite(
      assumptions.pd_speedup_vs_aggregated_threshold,
      "PD transfer speedup threshold");
  if (!assumptions.decode_requires_complete_prompt_kv ||
      !assumptions.sender_receiver_overhead_is_per_chunk ||
      !assumptions.overlap_efficiency_is_streaming_readiness) {
    throw std::invalid_argument(
        "PD transfer fixture requires its disclosed readiness and overhead semantics");
  }
}

PdTransferOverlapScenario validate_scenario(
    PdTransferOverlapScenario scenario) {
  require_nonempty(scenario.name, "PD transfer scenario name");
  (void)pd_transfer_topology_name(scenario.topology);
  if (scenario.prompt_tokens < 0 ||
      scenario.output_tokens < 0 ||
      scenario.prefill_chunk_tokens <= 0 ||
      scenario.decode_chunk_tokens <= 0 ||
      scenario.prefill_workers < 0 ||
      scenario.decode_workers < 0) {
    throw std::invalid_argument(
        "PD transfer token, chunk, and worker counts are invalid");
  }
  scenario.prefill_ms_per_token =
      require_non_negative_finite(
          scenario.prefill_ms_per_token,
          "PD transfer prefill milliseconds per token");
  scenario.decode_ms_per_token =
      require_non_negative_finite(
          scenario.decode_ms_per_token,
          "PD transfer decode milliseconds per token");
  scenario.kv_bytes_per_token =
      require_non_negative_finite(
          scenario.kv_bytes_per_token,
          "PD transfer KV bytes per token");
  scenario.link_bandwidth_gbps =
      require_non_negative_finite(
          scenario.link_bandwidth_gbps,
          "PD transfer link bandwidth");
  scenario.serialization_us_per_chunk =
      require_non_negative_finite(
          scenario.serialization_us_per_chunk,
          "PD transfer sender serialization overhead");
  scenario.receiver_setup_us_per_chunk =
      require_non_negative_finite(
          scenario.receiver_setup_us_per_chunk,
          "PD transfer receiver setup overhead");
  scenario.overlap_efficiency = require_unit_interval(
      scenario.overlap_efficiency,
      "PD transfer streaming overlap efficiency");

  if (scenario.prompt_tokens > 0) {
    if (scenario.prefill_workers <= 0) {
      throw std::invalid_argument(
          "PD transfer prompt work requires prefill workers");
    }
    require_positive_finite(
        scenario.prefill_ms_per_token,
        "PD transfer active prefill milliseconds per token");
  }
  if (scenario.output_tokens > 0) {
    if (scenario.decode_workers <= 0) {
      throw std::invalid_argument(
          "PD transfer output work requires decode workers");
    }
    require_positive_finite(
        scenario.decode_ms_per_token,
        "PD transfer active decode milliseconds per token");
  }

  if (scenario.topology ==
      PdTransferTopology::aggregated_reference) {
    if ((scenario.prompt_tokens > 0 &&
         scenario.prefill_workers != 1) ||
        (scenario.output_tokens > 0 &&
         scenario.decode_workers != 1) ||
        scenario.link_bandwidth_gbps != 0.0 ||
        scenario.serialization_us_per_chunk != 0.0 ||
        scenario.receiver_setup_us_per_chunk != 0.0 ||
        scenario.overlap_efficiency != 0.0) {
      throw std::invalid_argument(
          "PD transfer aggregated reference must be single-worker and transfer-free");
    }
  } else if (scenario.prompt_tokens > 0) {
    require_positive_finite(
        scenario.kv_bytes_per_token,
        "PD transfer active KV bytes per token");
    require_positive_finite(
        scenario.link_bandwidth_gbps,
        "PD transfer active link bandwidth");
  }
  return scenario;
}

bool same_scenario(
    const PdTransferOverlapScenario& left,
    const PdTransferOverlapScenario& right) {
  return left.name == right.name &&
         left.topology == right.topology &&
         left.prompt_tokens == right.prompt_tokens &&
         left.output_tokens == right.output_tokens &&
         left.prefill_chunk_tokens ==
             right.prefill_chunk_tokens &&
         left.decode_chunk_tokens ==
             right.decode_chunk_tokens &&
         left.prefill_workers == right.prefill_workers &&
         left.decode_workers == right.decode_workers &&
         left.prefill_ms_per_token ==
             right.prefill_ms_per_token &&
         left.decode_ms_per_token ==
             right.decode_ms_per_token &&
         left.kv_bytes_per_token ==
             right.kv_bytes_per_token &&
         left.link_bandwidth_gbps ==
             right.link_bandwidth_gbps &&
         left.serialization_us_per_chunk ==
             right.serialization_us_per_chunk &&
         left.receiver_setup_us_per_chunk ==
             right.receiver_setup_us_per_chunk &&
         left.overlap_efficiency ==
             right.overlap_efficiency;
}

int chunk_tokens_at(const int total_tokens,
                    const int chunk_tokens,
                    const int chunk_index,
                    const char* name) {
  const int start = internal::checked_multiply_int(
      chunk_index, chunk_tokens, name);
  if (start < 0 || start >= total_tokens) {
    throw std::invalid_argument(
        std::string(name) + " start is out of range");
  }
  return std::min(chunk_tokens, total_tokens - start);
}

void validate_result(const PdTransferOverlapResult& result) {
  const PdTransferOverlapScenario scenario =
      validate_scenario(result.scenario);
  if (!same_scenario(scenario, result.scenario)) {
    throw std::invalid_argument(
        "PD transfer result scenario normalization drift");
  }
  (void)pd_transfer_recommendation_name(
      result.recommendation);
  (void)pd_transfer_recommendation_text(
      result.recommendation);

  validate_result_shape(result);
  validate_metric_domain(result);
  const StageInventory inventory =
      validate_stage_inventory(result);
  validate_stage_totals(result, inventory);
  validate_transfer_accounting(result, inventory);
}

}  // namespace inferlab::pd_transfer_overlap_contract
