#include "kv_paged_attention_deep_dive_model.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"
#include "inferlab/paged_kv_cache.hpp"
#include "inferlab/paged_kv_locality.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace inferlab::kv_paged_attention_deep_dive {
namespace {

constexpr RequestId kRequestId = 1;
constexpr PrefixId kPrefixId = 1;

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

bool has_json_suffix(const std::string& value) {
  constexpr const char* kSuffix = ".json";
  constexpr std::size_t kSuffixSize = 5;
  return value.size() > kSuffixSize &&
         value.compare(value.size() - kSuffixSize, kSuffixSize, kSuffix) == 0;
}

void validate_evidence_path(const std::string& value) {
  constexpr const char* kPrefix = "results/raw/";
  if (value.rfind(kPrefix, 0) != 0 || !has_json_suffix(value) ||
      value.find("..") != std::string::npos ||
      value.find('\\') != std::string::npos) {
    throw std::invalid_argument(
        "KV/PagedAttention evidence path must be a safe results/raw JSON path");
  }
}

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.fixture_name,
                   "KV/PagedAttention fixture name");
  require_positive_finite(assumptions.bytes_per_gib,
                          "KV/PagedAttention bytes per GiB");
  if (assumptions.max_request_tokens <= 0 ||
      assumptions.max_physical_blocks <= 0 ||
      assumptions.max_kv_bytes_per_token <= 0) {
    throw std::invalid_argument(
        "KV/PagedAttention analytical resource caps must be positive");
  }

  const double internal_weight = require_unit_interval(
      assumptions.internal_fragmentation_weight,
      "KV/PagedAttention internal fragmentation weight");
  const double external_weight = require_unit_interval(
      assumptions.external_fragmentation_weight,
      "KV/PagedAttention external fragmentation weight");
  const double prefix_weight = require_unit_interval(
      assumptions.prefix_sharing_weight,
      "KV/PagedAttention prefix sharing weight");
  const double read_weight = require_unit_interval(
      assumptions.read_efficiency_weight,
      "KV/PagedAttention read efficiency weight");
  const double weight_sum = internal::checked_add_finite_double(
      internal::checked_add_finite_double(internal_weight, external_weight,
                                          "KV/PagedAttention score weights"),
      internal::checked_add_finite_double(prefix_weight, read_weight,
                                          "KV/PagedAttention score weights"),
      "KV/PagedAttention score weights");
  if (std::fabs(weight_sum - 1.0) > 1e-12) {
    throw std::invalid_argument(
        "KV/PagedAttention score weights must sum to one");
  }
  require_unit_interval(assumptions.prefix_sharing_bonus,
                        "KV/PagedAttention prefix sharing bonus");
  require_unit_interval(assumptions.replay_comparison_score_threshold,
                        "KV/PagedAttention replay comparison threshold");
  if (require_positive_finite(
          assumptions.high_read_amplification_threshold,
          "KV/PagedAttention high read amplification threshold") < 1.0) {
    throw std::invalid_argument(
        "KV/PagedAttention high read amplification threshold must be at least one");
  }
  if (!assumptions.require_block_aligned_shared_prefix) {
    throw std::invalid_argument(
        "KV/PagedAttention locality model requires block-aligned shared prefixes");
  }
}

void validate_scenario(const Assumptions& assumptions,
                       const Scenario& scenario) {
  require_nonempty(scenario.name, "KV/PagedAttention scenario name");
  validate_evidence_path(scenario.evidence_path);
  if (scenario.request_tokens <= 0 ||
      scenario.request_tokens > assumptions.max_request_tokens) {
    throw std::invalid_argument(
        "KV/PagedAttention request tokens exceed the analytical fixture range");
  }
  if (scenario.page_size_tokens <= 0 ||
      scenario.page_size_tokens > assumptions.max_request_tokens) {
    throw std::invalid_argument(
        "KV/PagedAttention page size exceeds the analytical fixture range");
  }
  if (scenario.live_tokens < 0 ||
      scenario.live_tokens > scenario.request_tokens) {
    throw std::invalid_argument(
        "KV/PagedAttention live transfer-payload tokens must fit the request");
  }
  if (scenario.shared_prefix_tokens < 0 ||
      scenario.shared_prefix_tokens > scenario.request_tokens) {
    throw std::invalid_argument(
        "KV/PagedAttention shared prefix must fit the request");
  }
  if (scenario.shared_prefix_tokens % scenario.page_size_tokens != 0) {
    throw std::invalid_argument(
        "KV/PagedAttention shared prefix must align to a page boundary");
  }
  if (scenario.free_tail_tokens < 0 ||
      scenario.free_tail_tokens > assumptions.max_request_tokens) {
    throw std::invalid_argument(
        "KV/PagedAttention free-tail tokens exceed the analytical fixture range");
  }
  if (scenario.kv_bytes_per_token <= 0 ||
      scenario.kv_bytes_per_token > assumptions.max_kv_bytes_per_token) {
    throw std::invalid_argument(
        "KV/PagedAttention KV bytes per token exceed the analytical fixture range");
  }

  const double dense_reads = require_positive_finite(
      scenario.dense_read_tokens, "KV/PagedAttention dense read tokens");
  if (dense_reads < static_cast<double>(scenario.request_tokens)) {
    throw std::invalid_argument(
        "KV/PagedAttention dense read baseline cannot be below logical request tokens");
  }
  const double bandwidth = require_non_negative_finite(
      scenario.transfer_bandwidth_gbps,
      "KV/PagedAttention transfer bandwidth");
  const double setup = require_non_negative_finite(
      scenario.transfer_setup_ms, "KV/PagedAttention transfer setup");
  if (scenario.live_tokens > 0 && bandwidth <= 0.0) {
    throw std::invalid_argument(
        "KV/PagedAttention positive transfer payload requires positive bandwidth");
  }
  if (scenario.live_tokens == 0 && setup != 0.0) {
    throw std::invalid_argument(
        "KV/PagedAttention zero transfer payload cannot carry setup latency");
  }
  const int recompute_tokens =
      scenario.request_tokens - scenario.shared_prefix_tokens;
  const double prefill_cost = require_non_negative_finite(
      scenario.prefill_ms_per_token,
      "KV/PagedAttention prefill milliseconds per token");
  if (recompute_tokens > 0 && prefill_cost <= 0.0) {
    throw std::invalid_argument(
        "KV/PagedAttention positive recompute work requires positive prefill cost");
  }
}

int expected_physical_blocks(const Scenario& scenario) {
  const int prefix_blocks = internal::checked_ceil_div_non_negative_int(
      scenario.shared_prefix_tokens, scenario.page_size_tokens,
      "KV/PagedAttention prefix blocks");
  const int private_tokens =
      scenario.request_tokens - scenario.shared_prefix_tokens;
  const int private_blocks = internal::checked_ceil_div_non_negative_int(
      private_tokens, scenario.page_size_tokens,
      "KV/PagedAttention private blocks");
  return internal::checked_add_int(prefix_blocks, private_blocks,
                                   "KV/PagedAttention physical blocks");
}

RequestBlocks allocate_request(PagedKVCacheManager& manager,
                               const Scenario& scenario) {
  bool allocated = false;
  if (scenario.shared_prefix_tokens > 0) {
    if (!manager.create_prefix(kPrefixId, scenario.shared_prefix_tokens)) {
      throw std::logic_error(
          "KV/PagedAttention canonical prefix allocation failed");
    }
    allocated = manager.add_request_with_prefix(
        kRequestId, kPrefixId,
        scenario.request_tokens - scenario.shared_prefix_tokens);
  } else {
    allocated = manager.add_request(kRequestId, scenario.request_tokens);
  }
  if (!allocated) {
    throw std::logic_error(
        "KV/PagedAttention canonical request allocation failed");
  }
  const auto blocks = manager.request_blocks(kRequestId);
  if (!blocks.has_value()) {
    throw std::logic_error(
        "KV/PagedAttention canonical request block table is missing");
  }
  return *blocks;
}

void append_unique_position(std::vector<int>& positions, const int value,
                            const int request_tokens) {
  if (value < 0 || value >= request_tokens ||
      std::find(positions.begin(), positions.end(), value) != positions.end()) {
    return;
  }
  positions.push_back(value);
}

std::vector<MappingSample> mapping_samples(
    const PagedKVCacheManager& manager, const Scenario& scenario) {
  std::vector<int> positions;
  positions.reserve(5);
  append_unique_position(positions, 0, scenario.request_tokens);
  if (scenario.shared_prefix_tokens > 0) {
    append_unique_position(positions, scenario.shared_prefix_tokens - 1,
                           scenario.request_tokens);
  }
  append_unique_position(positions, scenario.shared_prefix_tokens,
                         scenario.request_tokens);
  const int after_prefix_page = internal::checked_add_int(
      scenario.shared_prefix_tokens, scenario.page_size_tokens,
      "KV/PagedAttention mapping sample position");
  append_unique_position(positions, after_prefix_page,
                         scenario.request_tokens);
  append_unique_position(positions, scenario.request_tokens - 1,
                         scenario.request_tokens);

  std::vector<MappingSample> samples;
  samples.reserve(positions.size());
  for (const int logical_token : positions) {
    const auto location = manager.locate_token(kRequestId, logical_token);
    if (!location.has_value()) {
      throw std::logic_error(
          "KV/PagedAttention canonical token location is missing");
    }
    samples.push_back({logical_token, location->block_id,
                       location->block_offset,
                       logical_token < scenario.shared_prefix_tokens});
  }
  if (samples.empty()) {
    throw std::logic_error(
        "KV/PagedAttention mapping sample set must not be empty");
  }
  return samples;
}

double checked_ratio(const double numerator, const double denominator,
                     const char* name) {
  require_non_negative_finite(numerator, name);
  require_positive_finite(denominator, name);
  return require_finite(numerator / denominator, name);
}

void validate_result(const Result& result) {
  if (result.physical_blocks <= 0 || result.reserved_tokens <= 0 ||
      result.reserved_bytes <= 0 || result.resident_tokens <= 0 ||
      result.resident_kv_bytes <= 0 || result.kv_live_bytes < 0 ||
      result.block_table_entries <= 0 || result.mapping_samples.empty()) {
    throw std::logic_error(
        "KV/PagedAttention result contains invalid discrete facts");
  }
  require_unit_interval(result.internal_fragmentation_ratio,
                        "KV/PagedAttention internal fragmentation result");
  require_unit_interval(result.external_fragmentation_ratio,
                        "KV/PagedAttention external fragmentation result");
  require_positive_finite(result.dense_kv_read_tokens,
                          "KV/PagedAttention dense read result");
  require_positive_finite(result.paged_kv_read_tokens,
                          "KV/PagedAttention paged read result");
  require_unit_interval(result.paged_to_dense_read_ratio,
                        "KV/PagedAttention paged-to-dense read ratio");
  if (require_positive_finite(result.read_amplification,
                              "KV/PagedAttention read amplification") < 1.0) {
    throw std::logic_error(
        "KV/PagedAttention block-capacity read amplification is below one");
  }
  require_unit_interval(result.prefix_sharing_rate,
                        "KV/PagedAttention prefix sharing result");
  require_non_negative_finite(result.transfer_payload_gib,
                              "KV/PagedAttention transfer payload result");
  require_non_negative_finite(result.transfer_ms,
                              "KV/PagedAttention transfer time result");
  require_non_negative_finite(result.prefill_recompute_ms,
                              "KV/PagedAttention prefill result");
  require_finite(result.transfer_vs_prefill_delta_ms,
                 "KV/PagedAttention transfer delta result");
  require_unit_interval(result.deep_dive_score,
                        "KV/PagedAttention deep-dive score result");
}

}  // namespace

const char* transfer_decision_name(const TransferDecision decision) {
  switch (decision) {
    case TransferDecision::prefix_hit_avoids_transfer_and_recompute:
      return "prefix_hit_avoids_transfer_and_recompute";
    case TransferDecision::prefer_kv_transfer_or_prefix_reuse:
      return "prefer_kv_transfer_or_prefix_reuse";
    case TransferDecision::prefer_recompute_or_reduce_transfer_payload:
      return "prefer_recompute_or_reduce_transfer_payload";
  }
  throw std::logic_error("unknown KV/PagedAttention transfer decision");
}

const char* recommendation_name(const Recommendation recommendation) {
  switch (recommendation) {
    case Recommendation::ready_to_compare_against_product_replay_evidence:
      return "ready_to_compare_against_product_replay_evidence";
    case Recommendation::reduce_fragmentation_or_page_size_before_claiming_efficiency:
      return "reduce_fragmentation_or_page_size_before_claiming_efficiency";
    case Recommendation::attach_real_vllm_or_sglang_kv_metrics_after_migration:
      return "attach_real_vllm_or_sglang_kv_metrics_after_migration";
  }
  throw std::logic_error("unknown KV/PagedAttention recommendation");
}

Assumptions default_assumptions() { return {}; }

std::vector<Scenario> default_scenarios() {
  return {
      {"single_worker_paged_decode",
       "results/raw/paged_kv_locality_probe.json", 8192, 16, 8192, 0,
       7, 524288, 8192.0, 56.0, 0.40, 0.018},
      {"shared_prefix_multi_tenant",
       "results/raw/prefix_cache_compare.json", 12288, 16, 6144, 6144,
       5, 524288, 12288.0, 56.0, 0.40, 0.018},
      {"fragmented_long_context",
       "results/raw/kv_fragmentation_probe.json", 32768, 32, 24576, 4096,
       640, 524288, 32768.0, 24.0, 1.25, 0.020},
      {"remote_kv_transfer_candidate",
       "results/raw/native_kv_lifecycle_deep_dive.json", 16384, 16,
       12288, 8192, 9, 524288, 16384.0, 100.0, 0.75, 0.019},
  };
}

Result analyze(const Assumptions& assumptions, const Scenario& scenario) {
  validate_assumptions(assumptions);
  validate_scenario(assumptions, scenario);

  const int expected_blocks = expected_physical_blocks(scenario);
  if (expected_blocks <= 0 ||
      expected_blocks > assumptions.max_physical_blocks) {
    throw std::invalid_argument(
        "KV/PagedAttention physical block count exceeds the analytical fixture range");
  }

  PagedKVCacheManager manager(
      {expected_blocks, scenario.page_size_tokens,
       scenario.kv_bytes_per_token});
  const RequestBlocks blocks = allocate_request(manager, scenario);
  const AllocationStats stats = manager.stats();
  if (stats.used_blocks != expected_blocks ||
      stats.total_tokens != scenario.request_tokens ||
      stats.active_requests != 1 || stats.free_blocks != 0) {
    throw std::logic_error(
        "KV/PagedAttention canonical allocation facts do not match the fixture");
  }

  const PagedKVShape locality_shape{1, 1, scenario.page_size_tokens};
  const BlockTableLocalityMetrics locality =
      analyze_block_table_locality(blocks, locality_shape);
  const int block_table_entries = internal::checked_int_from_size(
      blocks.block_table.size(), "KV/PagedAttention block-table entries");
  if (locality.logical_blocks != block_table_entries ||
      locality.unique_physical_blocks != stats.used_blocks) {
    throw std::logic_error(
        "KV/PagedAttention canonical locality facts do not match allocation");
  }

  Result result;
  result.scenario = scenario;
  result.physical_blocks = stats.used_blocks;
  result.reserved_tokens = internal::checked_multiply_int(
      result.physical_blocks, scenario.page_size_tokens,
      "KV/PagedAttention reserved tokens");
  result.reserved_bytes = stats.reserved_bytes;
  result.resident_tokens = scenario.request_tokens;
  result.resident_kv_bytes = stats.used_bytes;
  result.kv_live_bytes = internal::checked_product_i64(
      scenario.live_tokens, scenario.kv_bytes_per_token,
      "KV/PagedAttention transfer payload bytes");
  result.block_table_entries = block_table_entries;
  result.internal_fragmentation_ratio = checked_ratio(
      static_cast<double>(result.reserved_tokens - result.resident_tokens),
      static_cast<double>(result.reserved_tokens),
      "KV/PagedAttention internal fragmentation");
  const int external_capacity_tokens = internal::checked_add_int(
      result.reserved_tokens, scenario.free_tail_tokens,
      "KV/PagedAttention external capacity tokens");
  result.external_fragmentation_ratio = checked_ratio(
      static_cast<double>(scenario.free_tail_tokens),
      static_cast<double>(external_capacity_tokens),
      "KV/PagedAttention external fragmentation");
  result.dense_kv_read_tokens = scenario.dense_read_tokens;
  result.paged_kv_read_tokens = static_cast<double>(scenario.request_tokens);
  result.paged_to_dense_read_ratio = checked_ratio(
      result.paged_kv_read_tokens, result.dense_kv_read_tokens,
      "KV/PagedAttention paged-to-dense read ratio");
  result.read_amplification = locality.block_element_amplification;
  result.prefix_sharing_rate = checked_ratio(
      static_cast<double>(scenario.shared_prefix_tokens),
      static_cast<double>(scenario.request_tokens),
      "KV/PagedAttention prefix sharing rate");
  result.transfer_payload_gib = checked_ratio(
      static_cast<double>(result.kv_live_bytes), assumptions.bytes_per_gib,
      "KV/PagedAttention transfer payload GiB");
  if (scenario.live_tokens > 0) {
    result.transfer_ms = internal::checked_add_finite_double(
        scenario.transfer_setup_ms,
        runtime_communication_internal::transfer_milliseconds(
            result.transfer_payload_gib, scenario.transfer_bandwidth_gbps,
            "KV/PagedAttention transfer serialization"),
        "KV/PagedAttention transfer time");
  }
  const int recompute_tokens =
      scenario.request_tokens - scenario.shared_prefix_tokens;
  result.prefill_recompute_ms = finite_product(
      static_cast<double>(recompute_tokens), scenario.prefill_ms_per_token,
      "KV/PagedAttention prefill recompute time");
  result.transfer_vs_prefill_delta_ms = require_finite(
      result.transfer_ms - result.prefill_recompute_ms,
      "KV/PagedAttention transfer-versus-prefill delta");

  if (recompute_tokens == 0) {
    result.transfer_vs_prefill_decision =
        TransferDecision::prefix_hit_avoids_transfer_and_recompute;
  } else if (scenario.live_tokens > 0 &&
             result.transfer_vs_prefill_delta_ms < 0.0) {
    result.transfer_vs_prefill_decision =
        TransferDecision::prefer_kv_transfer_or_prefix_reuse;
  } else {
    result.transfer_vs_prefill_decision =
        TransferDecision::prefer_recompute_or_reduce_transfer_payload;
  }

  const double prefix_signal = std::min(
      1.0, result.prefix_sharing_rate + assumptions.prefix_sharing_bonus);
  const double read_efficiency = checked_ratio(
      1.0, result.read_amplification,
      "KV/PagedAttention read efficiency");
  const double fragmentation_score = internal::checked_add_finite_double(
      finite_product(1.0 - result.internal_fragmentation_ratio,
                     assumptions.internal_fragmentation_weight,
                     "KV/PagedAttention internal fragmentation score"),
      finite_product(1.0 - result.external_fragmentation_ratio,
                     assumptions.external_fragmentation_weight,
                     "KV/PagedAttention external fragmentation score"),
      "KV/PagedAttention fragmentation score");
  const double reuse_and_read_score = internal::checked_add_finite_double(
      finite_product(prefix_signal, assumptions.prefix_sharing_weight,
                     "KV/PagedAttention prefix sharing score"),
      finite_product(read_efficiency, assumptions.read_efficiency_weight,
                     "KV/PagedAttention read efficiency score"),
      "KV/PagedAttention reuse and read score");
  result.deep_dive_score = internal::checked_add_finite_double(
      fragmentation_score, reuse_and_read_score,
      "KV/PagedAttention deep-dive score");

  if (result.deep_dive_score >=
      assumptions.replay_comparison_score_threshold) {
    result.recommendation =
        Recommendation::ready_to_compare_against_product_replay_evidence;
  } else if (result.read_amplification >
             assumptions.high_read_amplification_threshold) {
    result.recommendation = Recommendation::
        reduce_fragmentation_or_page_size_before_claiming_efficiency;
  } else {
    result.recommendation = Recommendation::
        attach_real_vllm_or_sglang_kv_metrics_after_migration;
  }
  result.mapping_samples = mapping_samples(manager, scenario);
  validate_result(result);
  return result;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios,
                inferlab_native_trace::Source trace_source) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "KV/PagedAttention scenario set must not be empty");
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.records.reserve(scenarios.size());
  std::vector<std::string> scenario_names;
  scenario_names.reserve(scenarios.size());
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (scenarios[prior].name == scenarios[index].name) {
        throw std::invalid_argument(
            "duplicate KV/PagedAttention scenario name: " +
            scenarios[index].name);
      }
    }
    scenario_names.push_back(scenarios[index].name);
    plan.records.push_back(analyze(assumptions, scenarios[index]));
    const auto& record = plan.records.back();
    if (record.transfer_vs_prefill_decision ==
        TransferDecision::prefer_kv_transfer_or_prefix_reuse) {
      plan.transfer_preferred_record_count = internal::checked_add_size(
          plan.transfer_preferred_record_count, 1U,
          "KV/PagedAttention transfer-preferred record count");
    }
    if (index == 0 ||
        record.deep_dive_score >
            plan.records[plan.best_record_index].deep_dive_score) {
      plan.best_record_index = index;
    }
    if (index == 0 ||
        record.read_amplification >
            plan.records[plan.worst_read_record_index].read_amplification) {
      plan.worst_read_record_index = index;
    }
  }
  plan.trace_context = inferlab_native_trace::bind_trace_context(
      inferlab_native_trace::Domain::kv_paged_attention, scenario_names,
      std::move(trace_source));
  if (plan.best_record_index >= plan.records.size() ||
      plan.worst_read_record_index >= plan.records.size() ||
      plan.transfer_preferred_record_count > plan.records.size()) {
    throw std::logic_error(
        "KV/PagedAttention plan summary is inconsistent");
  }
  return plan;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios) {
  return build_plan(assumptions, scenarios,
                    inferlab_native_trace::Source{});
}

}  // namespace inferlab::kv_paged_attention_deep_dive
