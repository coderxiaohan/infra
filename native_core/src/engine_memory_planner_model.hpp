#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "release_kv_budget_profile.hpp"

namespace inferlab::engine_memory_planner {

enum class NumericFormat {
  fp32,
  fp16,
  bf16,
  fp8,
  int8,
  fp4,
  int4,
  nvfp4,
};

enum class Recommendation {
  reduce_concurrency_lower_kv_precision_or_offload,
  reserve_more_kv_budget,
  kv_precision_is_main_limiter,
  use_chunked_prefill_paged_kv_prefix_reuse,
  memory_budget_acceptable,
};

enum class PlanDecision {
  profiles_exceed_memory,
  capacity_review_required,
  compiled_plan_ready,
};

struct Assumptions {
  std::string profile_name;
  double standard_weight_metadata_ratio = 0.0;
  double sub_byte_weight_metadata_ratio = 0.0;
  int activation_live_token_cap = 0;
  NumericFormat activation_format = NumericFormat::fp16;
  double activation_residency_ratio = 0.0;
  int io_hidden_copy_count = 0;
  int io_logits_copy_count = 0;
  NumericFormat io_format = NumericFormat::fp16;
  double target_memory_utilization = 0.0;
  double full_risk_memory_utilization = 0.0;
  double minimum_kv_headroom_stream_multiplier = 0.0;
};

struct Profile {
  std::string name;
  std::int64_t parameter_count = 0;
  int layers = 0;
  int hidden_size = 0;
  int kv_heads = 0;
  int head_dim = 0;
  int vocabulary_size = 0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int batch_size = 0;
  int max_concurrency = 0;
  NumericFormat weight_format = NumericFormat::fp16;
  NumericFormat kv_format = NumericFormat::fp16;
  double gpu_memory_gib = 0.0;
  double lora_adapter_gib = 0.0;
  double cuda_graph_workspace_gib = 0.0;
  double fragmentation_ratio = 0.0;
  int prefill_chunk_tokens = 0;
};

struct Record {
  Profile profile;
  int active_sequence_count = 0;
  int sequence_length_tokens = 0;
  int activation_live_tokens = 0;
  double weight_metadata_ratio = 0.0;
  double raw_weight_bytes = 0.0;
  double weight_bytes = 0.0;
  double kv_cache_bytes = 0.0;
  double activation_bytes = 0.0;
  double io_tensor_bytes = 0.0;
  double lora_adapter_bytes = 0.0;
  double cuda_graph_workspace_bytes = 0.0;
  double fixed_memory_bytes = 0.0;
  double configured_variable_bytes = 0.0;
  double fragmentation_bytes = 0.0;
  double total_required_bytes = 0.0;
  double available_after_weights_bytes = 0.0;
  double usable_before_fragmentation_bytes = 0.0;
  double kv_budget_bytes = 0.0;
  double aggregate_kv_token_bytes = 0.0;
  double one_concurrency_kv_bytes = 0.0;
  double one_concurrency_variable_bytes = 0.0;
  int max_tokens_by_kv_budget = 0;
  int max_concurrency_by_memory = 0;
  double kv_headroom_streams = 0.0;
  double memory_utilization = 0.0;
  double memory_pressure_over_target = 0.0;
  // Legacy normalized field retained for downstream compatibility.
  double oom_risk_score = 0.0;
  bool exceeds_gpu_memory = false;
  Recommendation recommendation = Recommendation::memory_budget_acceptable;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Record> records;
  double gpu_memory_gib = 0.0;
  std::size_t comparable_record_count = 0;
  std::size_t oom_profile_count = 0;
  std::size_t capacity_review_profile_count = 0;
  std::size_t max_kv_record_index = 0;
  std::size_t max_risk_record_index = 0;
  PlanDecision decision = PlanDecision::compiled_plan_ready;
};

const char* numeric_format_name(NumericFormat format);
int numeric_format_bits(NumericFormat format);
const char* recommendation_name(Recommendation recommendation);
const char* plan_decision_name(PlanDecision decision);
double bytes_to_gib(double bytes);

Assumptions default_assumptions();
std::vector<Profile> default_profiles(double gpu_memory_gib);
Record analyze_profile(const Assumptions& assumptions,
                       const Profile& profile);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Profile>& profiles);
inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan);

}  // namespace inferlab::engine_memory_planner
