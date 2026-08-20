#pragma once

#include <string>

namespace inferlab {

enum class KvCompressionBlendStrategy {
  exact_prefix_reuse,
  compressed_kv_restore,
  non_prefix_blended_reuse,
  selective_kv_retention,
  attention_sink_recent_window,
};

const char* kv_compression_blend_strategy_name(
    KvCompressionBlendStrategy strategy);

struct KvCompressionBlendAssumptions {
  std::string profile_name =
      "compiled_kv_compression_blend_analytical_fixture_v1";
  double kv_bytes_per_token = 524288.0;
  int tokens_per_restore_unit = 1000;
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  double selective_reuse_overlap_scale = 0.25;
  double non_prefix_quality_risk_weight = 0.35;
  double compression_quality_risk_weight = 0.08;
  double research_only_quality_risk_threshold = 0.45;
  double promising_quality_risk_threshold = 0.35;
};

struct KvCompressionBlendScenario {
  std::string name;
  KvCompressionBlendStrategy strategy =
      KvCompressionBlendStrategy::exact_prefix_reuse;
  int prompt_tokens = 0;
  int reusable_tokens = 0;
  double compression_ratio = 1.0;
  double restore_ms_per_1k_tokens = 0.0;
  double prefill_ms_per_token = 0.0;
  double rag_context_overlap = 0.0;
  double correctness_risk = 0.0;
};

struct KvCompressionBlendResult {
  KvCompressionBlendScenario scenario;
  double raw_reusable_kv_gib = 0.0;
  double compressed_reusable_kv_gib = 0.0;
  double compression_ratio = 1.0;
  double restore_latency_ms = 0.0;
  double hbm_saved_gib = 0.0;
  double non_prefix_reuse_rate = 0.0;
  double rag_context_overlap = 0.0;
  double quality_risk_score = 0.0;
  double ttft_saved_ms = 0.0;
  double net_latency_delta_ms = 0.0;
  bool active_reuse_workload = false;
  bool ranking_applicable = false;
  std::string safety_policy;
  std::string recommendation;
};

KvCompressionBlendAssumptions
default_kv_compression_blend_assumptions();
KvCompressionBlendResult estimate_kv_compression_blend(
    const KvCompressionBlendAssumptions& assumptions,
    const KvCompressionBlendScenario& scenario);

}  // namespace inferlab
