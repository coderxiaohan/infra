#include "engine_memory_planner_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"
#include "inferlab/kv_cache.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace inferlab::engine_memory_planner {
namespace {

constexpr double kBytesPerGib = 1073741824.0;

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

double gib_to_bytes(const double gib, const char* name) {
  return finite_product(
      require_non_negative_finite(gib, name), kBytesPerGib, name);
}

double checked_add_bytes(const double lhs, const double rhs,
                         const char* name) {
  return internal::checked_add_finite_double(lhs, rhs, name);
}

void require_positive(const int value, const char* name) {
  if (value <= 0) {
    throw std::invalid_argument(std::string(name) + " must be positive");
  }
}

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.profile_name, "engine memory assumption profile");
  require_unit_interval(assumptions.standard_weight_metadata_ratio,
                        "standard weight metadata ratio");
  require_unit_interval(assumptions.sub_byte_weight_metadata_ratio,
                        "sub-byte weight metadata ratio");
  if (assumptions.sub_byte_weight_metadata_ratio <
      assumptions.standard_weight_metadata_ratio) {
    throw std::invalid_argument(
        "sub-byte weight metadata ratio must not be below standard ratio");
  }
  require_positive(assumptions.activation_live_token_cap,
                   "activation live-token cap");
  numeric_format_bits(assumptions.activation_format);
  require_unit_interval(assumptions.activation_residency_ratio,
                        "activation residency ratio");
  require_positive(assumptions.io_hidden_copy_count,
                   "I/O hidden copy count");
  if (assumptions.io_logits_copy_count < 0) {
    throw std::invalid_argument("I/O logits copy count must be non-negative");
  }
  numeric_format_bits(assumptions.io_format);
  require_unit_interval(assumptions.target_memory_utilization,
                        "target memory utilization");
  require_positive_finite(assumptions.full_risk_memory_utilization,
                          "full-risk memory utilization");
  if (assumptions.full_risk_memory_utilization <=
      assumptions.target_memory_utilization) {
    throw std::invalid_argument(
        "full-risk utilization must exceed target utilization");
  }
  require_positive_finite(
      assumptions.minimum_kv_headroom_stream_multiplier,
      "minimum KV headroom stream multiplier");
}

void validate_profile(const Profile& profile) {
  require_nonempty(profile.name, "engine memory profile name");
  if (profile.parameter_count <= 0) {
    throw std::invalid_argument("parameter count must be positive");
  }
  require_positive(profile.layers, "layer count");
  require_positive(profile.hidden_size, "hidden size");
  require_positive(profile.kv_heads, "KV head count");
  require_positive(profile.head_dim, "head dimension");
  require_positive(profile.vocabulary_size, "vocabulary size");
  if (profile.prompt_tokens < 0 || profile.output_tokens < 0) {
    throw std::invalid_argument("prompt and output tokens must be non-negative");
  }
  if (profile.prompt_tokens == 0 && profile.output_tokens == 0) {
    throw std::invalid_argument("profile must contain at least one KV token");
  }
  if (profile.prefill_chunk_tokens < 0 ||
      profile.prefill_chunk_tokens > profile.prompt_tokens) {
    throw std::invalid_argument(
        "prefill chunk must be zero or fit the prompt token count");
  }
  require_positive(profile.batch_size, "batch size");
  require_positive(profile.max_concurrency, "maximum concurrency");
  numeric_format_bits(profile.weight_format);
  numeric_format_bits(profile.kv_format);
  require_positive_finite(profile.gpu_memory_gib, "GPU memory GiB");
  require_non_negative_finite(profile.lora_adapter_gib, "LoRA adapter GiB");
  require_non_negative_finite(profile.cuda_graph_workspace_gib,
                              "CUDA graph workspace GiB");
  require_unit_interval(profile.fragmentation_ratio,
                        "fragmentation ratio");
  if (profile.fragmentation_ratio >= 1.0) {
    throw std::invalid_argument("fragmentation ratio must be below one");
  }
}

DType kv_storage_dtype(const NumericFormat format) {
  switch (format) {
    case NumericFormat::fp32:
      return DType::kFloat32;
    case NumericFormat::fp16:
      return DType::kFloat16;
    case NumericFormat::bf16:
      return DType::kBFloat16;
    case NumericFormat::fp8:
    case NumericFormat::int8:
      return DType::kInt8;
    case NumericFormat::fp4:
    case NumericFormat::int4:
    case NumericFormat::nvfp4:
      return DType::kInt4;
  }
  throw std::invalid_argument("unknown engine memory numeric format");
}

std::int64_t kv_bytes(const Profile& profile, const int batch_size,
                      const int sequence_length) {
  const KVCacheConfig config{
      batch_size, sequence_length, profile.layers, profile.kv_heads,
      profile.head_dim, kv_storage_dtype(profile.kv_format)};
  return estimate_kv_cache(config).total_bytes;
}

double activation_bytes(const Assumptions& assumptions,
                        const Profile& profile, const int sequence_count,
                        const int live_tokens) {
  const auto elements = internal::checked_product_i64(
      profile.layers, profile.hidden_size, live_tokens, sequence_count,
      "engine memory activation elements");
  const auto packed = internal::checked_packed_bytes_for_bits(
      elements, numeric_format_bits(assumptions.activation_format),
      "engine memory activation bytes");
  return finite_product(static_cast<double>(packed),
                        assumptions.activation_residency_ratio,
                        "engine memory resident activation bytes");
}

double io_tensor_bytes(const Assumptions& assumptions,
                       const Profile& profile, const int sequence_count) {
  const auto hidden = internal::checked_product_i64(
      profile.hidden_size, assumptions.io_hidden_copy_count,
      "engine memory hidden I/O elements");
  const auto logits = internal::checked_product_i64(
      profile.vocabulary_size, assumptions.io_logits_copy_count,
      "engine memory logits I/O elements");
  const auto per_sequence = internal::checked_add_i64(
      hidden, logits, "engine memory per-sequence I/O elements");
  const auto elements = internal::checked_product_i64(
      per_sequence, sequence_count, "engine memory total I/O elements");
  return static_cast<double>(internal::checked_packed_bytes_for_bits(
      elements, numeric_format_bits(assumptions.io_format),
      "engine memory I/O bytes"));
}

Recommendation choose_recommendation(const Assumptions& assumptions,
                                     const Record& record) {
  if (record.exceeds_gpu_memory) {
    return Recommendation::reduce_concurrency_lower_kv_precision_or_offload;
  }
  if (record.kv_headroom_streams <
      assumptions.minimum_kv_headroom_stream_multiplier) {
    return Recommendation::reserve_more_kv_budget;
  }
  if (numeric_format_bits(record.profile.weight_format) <= 4 &&
      numeric_format_bits(record.profile.kv_format) >= 16) {
    return Recommendation::kv_precision_is_main_limiter;
  }
  if (record.profile.prompt_tokens >= 16000) {
    return Recommendation::use_chunked_prefill_paged_kv_prefix_reuse;
  }
  return Recommendation::memory_budget_acceptable;
}

}  // namespace

const char* numeric_format_name(const NumericFormat format) {
  switch (format) {
    case NumericFormat::fp32: return "fp32";
    case NumericFormat::fp16: return "fp16";
    case NumericFormat::bf16: return "bf16";
    case NumericFormat::fp8: return "fp8";
    case NumericFormat::int8: return "int8";
    case NumericFormat::fp4: return "fp4";
    case NumericFormat::int4: return "int4";
    case NumericFormat::nvfp4: return "nvfp4";
  }
  throw std::invalid_argument("unknown engine memory numeric format");
}

int numeric_format_bits(const NumericFormat format) {
  switch (format) {
    case NumericFormat::fp32: return 32;
    case NumericFormat::fp16:
    case NumericFormat::bf16: return 16;
    case NumericFormat::fp8:
    case NumericFormat::int8: return 8;
    case NumericFormat::fp4:
    case NumericFormat::int4:
    case NumericFormat::nvfp4: return 4;
  }
  throw std::invalid_argument("unknown engine memory numeric format");
}

const char* recommendation_name(const Recommendation recommendation) {
  switch (recommendation) {
    case Recommendation::reduce_concurrency_lower_kv_precision_or_offload:
      return "reduce concurrency, lower KV precision, or enable KV offload";
    case Recommendation::reserve_more_kv_budget:
      return "reserve more KV budget before increasing max model length";
    case Recommendation::kv_precision_is_main_limiter:
      return "weight quantization helps, but KV precision is now the main limiter";
    case Recommendation::use_chunked_prefill_paged_kv_prefix_reuse:
      return "combine chunked prefill with paged KV and prefix reuse";
    case Recommendation::memory_budget_acceptable:
      return "memory budget is acceptable for the planned profile";
  }
  throw std::invalid_argument("unknown engine memory recommendation");
}

const char* plan_decision_name(const PlanDecision decision) {
  switch (decision) {
    case PlanDecision::profiles_exceed_memory:
      return "profiles_exceed_memory";
    case PlanDecision::capacity_review_required:
      return "capacity_review_required";
    case PlanDecision::compiled_plan_ready:
      return "compiled_plan_ready";
  }
  throw std::invalid_argument("unknown engine memory plan decision");
}

double bytes_to_gib(const double bytes) {
  return require_finite(bytes, "engine memory byte value") / kBytesPerGib;
}

Assumptions default_assumptions() {
  return {"compiled_engine_memory_fixture_v1", 0.01, 0.08, 2048,
          NumericFormat::fp16, 0.18, 8, 1, NumericFormat::fp16,
          0.82, 1.10, 2.0};
}

std::vector<Profile> default_profiles(const double gpu_memory_gib) {
  require_positive_finite(gpu_memory_gib, "GPU memory GiB");
  return {
      {"llama_7b_fp16_2k", 7000000000LL, 32, 4096, 32, 128, 32000,
       2048, 256, 1, 8, NumericFormat::fp16, NumericFormat::fp16,
       gpu_memory_gib, 0.0, 1.0, 0.08},
      {"llama_13b_int4_8k_lora", 13000000000LL, 40, 5120, 40, 128,
       32000, 8192, 512, 1, 6, NumericFormat::int4, NumericFormat::fp16,
       gpu_memory_gib, 1.2, 1.5, 0.10},
      {"mixtral_like_fp8_32k", 46700000000LL, 32, 4096, 8, 128, 32000,
       32768, 512, 1, 4, NumericFormat::fp8, NumericFormat::fp8,
       gpu_memory_gib, 0.0, 2.0, 0.12},
      {"qwen_32b_int4_16k", 32000000000LL, 64, 5120, 40, 128, 152064,
       16384, 1024, 1, 4, NumericFormat::int4, NumericFormat::fp8,
       gpu_memory_gib, 0.0, 2.0, 0.12},
      {"release_kv_13b_int4_8k", 13000000000LL, 40, 5120, 40, 128,
       32000, 8192, 256, 1, 1, NumericFormat::int4, NumericFormat::fp16,
       gpu_memory_gib, 0.0, 1.5, 0.10, 2048},
  };
}

Record analyze_profile(const Assumptions& assumptions,
                       const Profile& profile) {
  validate_assumptions(assumptions);
  validate_profile(profile);

  Record record;
  record.profile = profile;
  record.sequence_length_tokens = internal::checked_add_int(
      profile.prompt_tokens, profile.output_tokens,
      "engine memory sequence length");
  record.active_sequence_count = internal::checked_multiply_int(
      profile.batch_size, profile.max_concurrency,
      "engine memory active sequence count");
  record.activation_live_tokens =
      std::min(profile.prompt_tokens, assumptions.activation_live_token_cap);
  record.weight_metadata_ratio =
      numeric_format_bits(profile.weight_format) < 8
          ? assumptions.sub_byte_weight_metadata_ratio
          : assumptions.standard_weight_metadata_ratio;

  record.raw_weight_bytes = static_cast<double>(
      internal::checked_packed_bytes_for_bits(
          profile.parameter_count, numeric_format_bits(profile.weight_format),
          "engine memory raw weight bytes"));
  const double weight_metadata_bytes = finite_product(
      record.raw_weight_bytes, record.weight_metadata_ratio,
      "engine memory weight metadata bytes");
  record.weight_bytes = checked_add_bytes(
      record.raw_weight_bytes, weight_metadata_bytes,
      "engine memory total weight bytes");
  record.kv_cache_bytes = static_cast<double>(kv_bytes(
      profile, record.active_sequence_count, record.sequence_length_tokens));
  record.activation_bytes = activation_bytes(
      assumptions, profile, record.active_sequence_count,
      record.activation_live_tokens);
  record.io_tensor_bytes = io_tensor_bytes(
      assumptions, profile, record.active_sequence_count);
  record.lora_adapter_bytes = gib_to_bytes(
      profile.lora_adapter_gib, "engine memory LoRA bytes");
  record.cuda_graph_workspace_bytes = gib_to_bytes(
      profile.cuda_graph_workspace_gib,
      "engine memory CUDA graph workspace bytes");

  record.fixed_memory_bytes = checked_add_bytes(
      checked_add_bytes(record.weight_bytes, record.lora_adapter_bytes,
                        "engine memory fixed bytes"),
      record.cuda_graph_workspace_bytes, "engine memory fixed bytes");
  record.configured_variable_bytes = checked_add_bytes(
      checked_add_bytes(record.kv_cache_bytes, record.activation_bytes,
                        "engine memory configured variable bytes"),
      record.io_tensor_bytes, "engine memory configured variable bytes");
  const double subtotal = checked_add_bytes(
      record.fixed_memory_bytes, record.configured_variable_bytes,
      "engine memory subtotal bytes");
  record.fragmentation_bytes = finite_product(
      subtotal, profile.fragmentation_ratio,
      "engine memory fragmentation bytes");
  record.total_required_bytes = checked_add_bytes(
      subtotal, record.fragmentation_bytes,
      "engine memory total required bytes");

  const double gpu_bytes = gib_to_bytes(profile.gpu_memory_gib,
                                        "engine memory GPU bytes");
  record.available_after_weights_bytes = gpu_bytes - record.fixed_memory_bytes;
  record.usable_before_fragmentation_bytes =
      gpu_bytes / (1.0 + profile.fragmentation_ratio);
  const double non_kv_remaining =
      record.usable_before_fragmentation_bytes - record.fixed_memory_bytes -
      record.activation_bytes - record.io_tensor_bytes;
  record.kv_budget_bytes = std::max(0.0, non_kv_remaining);

  record.aggregate_kv_token_bytes = static_cast<double>(
      kv_bytes(profile, 1, 1));
  record.one_concurrency_kv_bytes = static_cast<double>(
      kv_bytes(profile, profile.batch_size, record.sequence_length_tokens));
  const double one_activation = activation_bytes(
      assumptions, profile, profile.batch_size,
      record.activation_live_tokens);
  const double one_io = io_tensor_bytes(
      assumptions, profile, profile.batch_size);
  record.one_concurrency_variable_bytes = checked_add_bytes(
      checked_add_bytes(record.one_concurrency_kv_bytes, one_activation,
                        "engine memory one-concurrency variable bytes"),
      one_io, "engine memory one-concurrency variable bytes");
  require_positive_finite(record.aggregate_kv_token_bytes,
                          "aggregate KV token bytes");
  require_positive_finite(record.one_concurrency_kv_bytes,
                          "one-concurrency KV bytes");
  require_positive_finite(record.one_concurrency_variable_bytes,
                          "one-concurrency variable bytes");

  record.max_tokens_by_kv_budget =
      internal::checked_non_negative_floor_double_to_int(
          record.kv_budget_bytes / record.aggregate_kv_token_bytes,
          "engine memory max aggregate KV tokens");
  record.max_concurrency_by_memory =
      internal::checked_non_negative_floor_double_to_int(
          (record.usable_before_fragmentation_bytes -
           record.fixed_memory_bytes) /
              record.one_concurrency_variable_bytes,
          "engine memory max concurrency");
  record.kv_headroom_streams =
      record.kv_budget_bytes / record.one_concurrency_kv_bytes;
  record.memory_utilization = record.total_required_bytes / gpu_bytes;
  record.memory_pressure_over_target =
      record.memory_utilization <= assumptions.target_memory_utilization
          ? 0.0
          : (record.memory_utilization -
             assumptions.target_memory_utilization) /
                (assumptions.full_risk_memory_utilization -
                 assumptions.target_memory_utilization);
  require_non_negative_finite(record.memory_pressure_over_target,
                              "engine memory pressure over target");
  record.oom_risk_score =
      std::min(1.0, record.memory_pressure_over_target);
  record.exceeds_gpu_memory = record.total_required_bytes > gpu_bytes;
  record.recommendation = choose_recommendation(assumptions, record);
  return record;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Profile>& profiles) {
  validate_assumptions(assumptions);
  if (profiles.empty()) {
    throw std::invalid_argument("engine memory profiles must not be empty");
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.gpu_memory_gib = profiles.front().gpu_memory_gib;
  plan.records.reserve(profiles.size());
  for (std::size_t index = 0; index < profiles.size(); ++index) {
    if (profiles[index].gpu_memory_gib != plan.gpu_memory_gib) {
      throw std::invalid_argument(
          "engine memory profiles must share one GPU capacity");
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (profiles[prior].name == profiles[index].name) {
        throw std::invalid_argument(
            "engine memory profile names must be unique");
      }
    }
    plan.records.push_back(analyze_profile(assumptions, profiles[index]));
  }

  plan.comparable_record_count = plan.records.size();
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    if (record.exceeds_gpu_memory) {
      plan.oom_profile_count = internal::checked_add_size(
          plan.oom_profile_count, 1, "engine memory OOM profile count");
    }
    if (record.max_concurrency_by_memory < record.profile.max_concurrency ||
        record.kv_headroom_streams <
            assumptions.minimum_kv_headroom_stream_multiplier) {
      plan.capacity_review_profile_count = internal::checked_add_size(
          plan.capacity_review_profile_count, 1,
          "engine memory capacity-review profile count");
    }
    if (record.kv_cache_bytes >
        plan.records[plan.max_kv_record_index].kv_cache_bytes) {
      plan.max_kv_record_index = index;
    }
    if (record.memory_pressure_over_target >
        plan.records[plan.max_risk_record_index]
            .memory_pressure_over_target) {
      plan.max_risk_record_index = index;
    }
  }

  if (plan.oom_profile_count > 0) {
    plan.decision = PlanDecision::profiles_exceed_memory;
  } else if (plan.capacity_review_profile_count > 0) {
    plan.decision = PlanDecision::capacity_review_required;
  } else {
    plan.decision = PlanDecision::compiled_plan_ready;
  }
  return plan;
}

inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan) {
  std::vector<inferlab_release_kv_budget::Observation> observations;
  observations.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    if (record.profile.prefill_chunk_tokens <= 0) continue;
    const bool compatible =
        !record.exceeds_gpu_memory &&
        record.kv_cache_bytes <= record.kv_budget_bytes;
    observations.push_back(
        {record.profile.name, record.profile.gpu_memory_gib,
         record.profile.prompt_tokens, record.profile.output_tokens,
         record.profile.prefill_chunk_tokens, compatible,
         compatible ? "fits_local_kv_budget"
                    : "exceeds_local_kv_budget"});
  }
  return inferlab_release_kv_budget::bind_profile("engine_memory_planner",
                                                   observations);
}

}  // namespace inferlab::engine_memory_planner
