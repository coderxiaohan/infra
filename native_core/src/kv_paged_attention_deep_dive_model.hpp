#pragma once

#include "native_trace_context.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inferlab::kv_paged_attention_deep_dive {

enum class TransferDecision {
  prefix_hit_avoids_transfer_and_recompute,
  prefer_kv_transfer_or_prefix_reuse,
  prefer_recompute_or_reduce_transfer_payload,
};

enum class Recommendation {
  ready_to_compare_against_product_replay_evidence,
  reduce_fragmentation_or_page_size_before_claiming_efficiency,
  attach_real_vllm_or_sglang_kv_metrics_after_migration,
};

struct Assumptions {
  std::string fixture_name =
      "compiled_kv_paged_attention_analytical_fixture_v2";
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  int max_request_tokens = 16777216;
  int max_physical_blocks = 1048576;
  int max_kv_bytes_per_token = 16777216;
  double internal_fragmentation_weight = 0.25;
  double external_fragmentation_weight = 0.20;
  double prefix_sharing_weight = 0.25;
  double read_efficiency_weight = 0.30;
  double prefix_sharing_bonus = 0.15;
  double replay_comparison_score_threshold = 0.72;
  double high_read_amplification_threshold = 1.20;
  bool require_block_aligned_shared_prefix = true;
};

struct Scenario {
  std::string name;
  std::string evidence_path;
  int request_tokens = 0;
  int page_size_tokens = 0;
  int live_tokens = 0;
  int shared_prefix_tokens = 0;
  int free_tail_tokens = 0;
  int kv_bytes_per_token = 0;
  double dense_read_tokens = 0.0;
  double transfer_bandwidth_gbps = 0.0;
  double transfer_setup_ms = 0.0;
  double prefill_ms_per_token = 0.0;
};

struct MappingSample {
  int logical_token = 0;
  int physical_block = 0;
  int block_offset = 0;
  bool shared_prefix = false;
};

struct Result {
  Scenario scenario;
  int physical_blocks = 0;
  int reserved_tokens = 0;
  std::int64_t reserved_bytes = 0;
  int resident_tokens = 0;
  std::int64_t resident_kv_bytes = 0;
  std::int64_t kv_live_bytes = 0;
  int block_table_entries = 0;
  double internal_fragmentation_ratio = 0.0;
  double external_fragmentation_ratio = 0.0;
  double dense_kv_read_tokens = 0.0;
  double paged_kv_read_tokens = 0.0;
  double paged_to_dense_read_ratio = 0.0;
  double read_amplification = 0.0;
  double prefix_sharing_rate = 0.0;
  double transfer_payload_gib = 0.0;
  double transfer_ms = 0.0;
  double prefill_recompute_ms = 0.0;
  double transfer_vs_prefill_delta_ms = 0.0;
  double deep_dive_score = 0.0;
  TransferDecision transfer_vs_prefill_decision =
      TransferDecision::prefer_recompute_or_reduce_transfer_payload;
  Recommendation recommendation =
      Recommendation::attach_real_vllm_or_sglang_kv_metrics_after_migration;
  std::vector<MappingSample> mapping_samples;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Result> records;
  inferlab_native_trace::Projection trace_context;
  std::size_t best_record_index = 0;
  std::size_t worst_read_record_index = 0;
  std::size_t transfer_preferred_record_count = 0;
};

const char* transfer_decision_name(TransferDecision decision);
const char* recommendation_name(Recommendation recommendation);

Assumptions default_assumptions();
std::vector<Scenario> default_scenarios();
Result analyze(const Assumptions& assumptions, const Scenario& scenario);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios,
                inferlab_native_trace::Source trace_source);

}  // namespace inferlab::kv_paged_attention_deep_dive
