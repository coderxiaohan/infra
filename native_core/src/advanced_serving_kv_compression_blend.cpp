#include "inferlab/advanced_serving_precision.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace inferlab {
namespace {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

void validate_kv_compression_blend_assumptions(
    const KvCompressionBlendAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "KV compression blend assumption profile");
  require_positive_finite(assumptions.kv_bytes_per_token,
                          "KV compression blend bytes per token");
  if (assumptions.tokens_per_restore_unit <= 0) {
    throw std::invalid_argument(
        "KV compression blend restore token unit must be positive");
  }
  require_positive_finite(assumptions.bytes_per_gib,
                          "KV compression blend bytes per GiB");
  require_unit_interval(assumptions.selective_reuse_overlap_scale,
                        "KV compression blend selective overlap scale");
  require_unit_interval(assumptions.non_prefix_quality_risk_weight,
                        "KV compression blend non-prefix risk weight");
  require_unit_interval(assumptions.compression_quality_risk_weight,
                        "KV compression blend compression risk weight");
  require_unit_interval(
      assumptions.research_only_quality_risk_threshold,
      "KV compression blend research-only risk threshold");
  require_unit_interval(
      assumptions.promising_quality_risk_threshold,
      "KV compression blend promising risk threshold");
  if (assumptions.promising_quality_risk_threshold >
      assumptions.research_only_quality_risk_threshold) {
    throw std::invalid_argument(
        "KV compression blend promising threshold must not exceed the research-only threshold");
  }
}

KvCompressionBlendScenario validate_kv_compression_blend_scenario(
    KvCompressionBlendScenario scenario) {
  require_nonempty(scenario.name, "KV compression blend scenario name");
  (void)kv_compression_blend_strategy_name(scenario.strategy);
  if (scenario.prompt_tokens < 0 || scenario.reusable_tokens < 0) {
    throw std::invalid_argument(
        "KV compression blend token counts must be non-negative");
  }
  if (scenario.reusable_tokens > scenario.prompt_tokens) {
    throw std::invalid_argument(
        "KV compression blend reusable tokens cannot exceed prompt tokens");
  }
  scenario.compression_ratio = require_positive_finite(
      scenario.compression_ratio,
      "KV compression blend retained-size ratio");
  if (scenario.compression_ratio > 1.0) {
    throw std::invalid_argument(
        "KV compression blend retained-size ratio must not exceed one");
  }
  if (scenario.strategy ==
          KvCompressionBlendStrategy::exact_prefix_reuse &&
      std::fabs(scenario.compression_ratio - 1.0) > 1e-9) {
    throw std::invalid_argument(
        "exact prefix reuse must retain the full KV representation");
  }
  scenario.restore_ms_per_1k_tokens = require_non_negative_finite(
      scenario.restore_ms_per_1k_tokens,
      "KV compression blend restore latency per token unit");
  scenario.prefill_ms_per_token = require_non_negative_finite(
      scenario.prefill_ms_per_token,
      "KV compression blend prefill latency per token");
  scenario.rag_context_overlap = require_unit_interval(
      scenario.rag_context_overlap,
      "KV compression blend RAG context overlap");
  scenario.correctness_risk = require_unit_interval(
      scenario.correctness_risk,
      "KV compression blend correctness risk");
  return scenario;
}

double non_prefix_reuse_rate(
    const KvCompressionBlendAssumptions& assumptions,
    const KvCompressionBlendScenario& scenario) {
  switch (scenario.strategy) {
    case KvCompressionBlendStrategy::non_prefix_blended_reuse:
      return scenario.rag_context_overlap;
    case KvCompressionBlendStrategy::exact_prefix_reuse:
      return 0.0;
    case KvCompressionBlendStrategy::compressed_kv_restore:
    case KvCompressionBlendStrategy::selective_kv_retention:
    case KvCompressionBlendStrategy::attention_sink_recent_window:
      return finite_product(
          scenario.rag_context_overlap,
          assumptions.selective_reuse_overlap_scale,
          "KV compression blend selective non-prefix reuse rate");
  }
  throw std::invalid_argument("unknown KV compression blend strategy");
}

}  // namespace

const char* kv_compression_blend_strategy_name(
    const KvCompressionBlendStrategy strategy) {
  switch (strategy) {
    case KvCompressionBlendStrategy::exact_prefix_reuse:
      return "exact_prefix_reuse";
    case KvCompressionBlendStrategy::compressed_kv_restore:
      return "compressed_kv_restore";
    case KvCompressionBlendStrategy::non_prefix_blended_reuse:
      return "non_prefix_blended_reuse";
    case KvCompressionBlendStrategy::selective_kv_retention:
      return "selective_kv_retention";
    case KvCompressionBlendStrategy::attention_sink_recent_window:
      return "attention_sink_recent_window";
  }
  throw std::invalid_argument("unknown KV compression blend strategy");
}

KvCompressionBlendAssumptions
default_kv_compression_blend_assumptions() {
  KvCompressionBlendAssumptions assumptions;
  assumptions.profile_name =
      "compiled_kv_compression_blend_analytical_fixture_v1";
  return assumptions;
}

KvCompressionBlendResult estimate_kv_compression_blend(
    const KvCompressionBlendAssumptions& assumptions,
    const KvCompressionBlendScenario& input_scenario) {
  validate_kv_compression_blend_assumptions(assumptions);
  const KvCompressionBlendScenario scenario =
      validate_kv_compression_blend_scenario(input_scenario);

  KvCompressionBlendResult result;
  result.scenario = scenario;
  result.compression_ratio = scenario.compression_ratio;
  result.rag_context_overlap = scenario.rag_context_overlap;
  result.active_reuse_workload = scenario.reusable_tokens > 0;
  result.ranking_applicable = result.active_reuse_workload;
  if (!result.active_reuse_workload) {
    result.safety_policy = "no_active_reuse_workload";
    result.recommendation = "no_reusable_tokens_to_evaluate";
    return result;
  }

  const double reusable_tokens =
      static_cast<double>(scenario.reusable_tokens);
  const double raw_reusable_bytes = finite_product(
      reusable_tokens, assumptions.kv_bytes_per_token,
      "KV compression blend raw reusable bytes");
  result.raw_reusable_kv_gib = require_finite(
      raw_reusable_bytes / assumptions.bytes_per_gib,
      "KV compression blend raw reusable GiB");
  result.compressed_reusable_kv_gib = finite_product(
      result.raw_reusable_kv_gib, scenario.compression_ratio,
      "KV compression blend compressed reusable GiB");
  result.hbm_saved_gib = std::max(
      0.0, require_finite(result.raw_reusable_kv_gib -
                              result.compressed_reusable_kv_gib,
                          "KV compression blend HBM saving"));

  const double restore_units = require_finite(
      reusable_tokens /
          static_cast<double>(assumptions.tokens_per_restore_unit),
      "KV compression blend restore units");
  result.restore_latency_ms = finite_product(
      restore_units, scenario.restore_ms_per_1k_tokens,
      "KV compression blend restore latency");
  const double reusable_prefill_ms = finite_product(
      reusable_tokens, scenario.prefill_ms_per_token,
      "KV compression blend reusable prefill latency");
  result.ttft_saved_ms = finite_product(
      reusable_prefill_ms, scenario.rag_context_overlap,
      "KV compression blend gross TTFT saving");
  result.net_latency_delta_ms = require_finite(
      result.restore_latency_ms - result.ttft_saved_ms,
      "KV compression blend net latency delta");

  result.non_prefix_reuse_rate =
      non_prefix_reuse_rate(assumptions, scenario);
  const double non_prefix_risk = finite_product(
      result.non_prefix_reuse_rate,
      assumptions.non_prefix_quality_risk_weight,
      "KV compression blend non-prefix quality risk");
  const double compression_risk = finite_product(
      1.0 - scenario.compression_ratio,
      assumptions.compression_quality_risk_weight,
      "KV compression blend compression quality risk");
  double aggregate_risk = internal::checked_add_finite_double(
      scenario.correctness_risk, non_prefix_risk,
      "KV compression blend aggregate quality risk");
  aggregate_risk = internal::checked_add_finite_double(
      aggregate_risk, compression_risk,
      "KV compression blend aggregate quality risk");
  result.quality_risk_score = clamp_unit_interval(aggregate_risk);

  if (scenario.strategy ==
      KvCompressionBlendStrategy::exact_prefix_reuse) {
    result.safety_policy =
        "exact_token_match_candidate_requires_runtime_validation";
  } else if (
      result.quality_risk_score >=
      assumptions.research_only_quality_risk_threshold) {
    result.safety_policy = "research_only_require_quality_oracle";
  } else {
    result.safety_policy = "allow_dry_run_then_gate_with_output_diff";
  }
  if (result.net_latency_delta_ms < 0.0 &&
      result.quality_risk_score <
          assumptions.promising_quality_risk_threshold) {
    result.recommendation = "promising_after_real_quality_validation";
  } else if (
      result.quality_risk_score >=
      assumptions.research_only_quality_risk_threshold) {
    result.recommendation =
        "do_not_claim_safe_kv_reuse_without_correctness_proof";
  } else {
    result.recommendation =
        "profile_restore_latency_before_enablement";
  }
  return result;
}

}  // namespace inferlab
