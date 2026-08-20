#include "inferlab/advanced_serving_reuse_cache.hpp"

#include <algorithm>
#include <cmath>

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {
namespace {

constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;
constexpr double kBytesPerMiB = 1024.0 * 1024.0;
constexpr int kPagedKvBlockTokens = 16;
constexpr int kRadixNodesPerSplit = 32;

}  // namespace

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::positive_or_default;
using advanced_serving_internal::positive_or_one;
using advanced_serving_internal::require_finite;

RadixPrefixCacheResult estimate_radix_prefix_cache(
    const RadixPrefixCacheScenario& scenario) {
  const int requests = positive_or_one(scenario.requests);
  const int avg_prompt = positive_or_one(scenario.avg_prompt_tokens);
  const int shared_prefix =
      std::min(avg_prompt, std::max(0, scenario.shared_prefix_tokens));
  const int unique_suffix = std::min(
      avg_prompt - shared_prefix, std::max(0, scenario.unique_suffix_tokens));
  const double requested_reuse_rate = clamp_unit_interval(require_finite(
      scenario.prefix_reuse_rate, "radix prefix cache reuse rate"));
  const double reuse_rate = shared_prefix <= 0 ? 0.0 : requested_reuse_rate;
  const double node_overhead =
      positive_or_default(scenario.node_overhead_bytes, 96.0);
  const double kv_bytes =
      positive_or_default(scenario.kv_bytes_per_token, 1.0);
  const double prefill_ms =
      positive_or_default(scenario.prefill_ms_per_token, 0.08);
  const double lookup_us =
      positive_or_default(scenario.radix_lookup_us_per_node, 0.50);
  const int branch_factor = internal::checked_non_negative_ceil_double_to_int(
      require_finite((1.0 - reuse_rate) * static_cast<double>(requests) /
                         16.0,
                     "radix prefix cache branch estimate"),
      "radix prefix cache branch factor");
  const int suffix_nodes = internal::checked_non_negative_ceil_double_to_int(
      require_finite(static_cast<double>(requests) *
                         static_cast<double>(unique_suffix) / 512.0,
                     "radix prefix cache suffix node estimate"),
      "radix prefix cache suffix nodes");
  const int nodes = internal::checked_add_int(
      internal::checked_add_int(1, branch_factor,
                                "radix prefix cache nodes"),
      suffix_nodes, "radix prefix cache nodes");
  const double reused_tokens = require_finite(
      static_cast<double>(requests) * static_cast<double>(shared_prefix) *
          reuse_rate,
      "radix prefix cache reused tokens");
  const double saved_prefill = finite_product(
      reused_tokens, prefill_ms, "radix prefix cache saved prefill latency");
  const double lookup_overhead = require_finite(
      static_cast<double>(requests) *
          std::log2(static_cast<double>(std::max(2, nodes))) * lookup_us /
          1000.0,
      "radix prefix cache lookup overhead");
  const double metadata_mb = require_finite(
      finite_product(static_cast<double>(nodes), node_overhead,
                     "radix prefix cache metadata bytes") /
          kBytesPerMiB,
      "radix prefix cache metadata size");
  const double saved_kv_gb = require_finite(
      finite_product(reused_tokens, kv_bytes,
                     "radix prefix cache saved KV bytes") /
          kBytesPerGiB,
      "radix prefix cache saved KV size");
  const double net_saving = require_finite(
      saved_prefill - lookup_overhead,
      "radix prefix cache net TTFT saving");
  const double baseline_prefill = require_finite(
      static_cast<double>(requests) * static_cast<double>(avg_prompt) *
          prefill_ms,
      "radix prefix cache baseline prefill latency");
  const double efficiency = clamp_unit_interval(require_finite(
      net_saving / std::max(1.0, baseline_prefill),
      "radix prefix cache efficiency"));
  const int shared_blocks = internal::checked_ceil_div_non_negative_int(
      shared_prefix, kPagedKvBlockTokens,
      "radix prefix cache shared block count");
  const int suffix_blocks = internal::checked_ceil_div_non_negative_int(
      unique_suffix, kPagedKvBlockTokens,
      "radix prefix cache suffix block count");
  const int split_count = internal::checked_ceil_div_non_negative_int(
      nodes, kRadixNodesPerSplit, "radix prefix cache split count");
  const int total_blocks = internal::checked_add_int(
      shared_blocks, suffix_blocks, "radix prefix cache total block count");
  const double exact_hit_rate = require_finite(
      reuse_rate * 0.62, "radix prefix cache exact hit rate");
  const double hash_hit_rate = require_finite(
      std::max(exact_hit_rate, reuse_rate * 0.82),
      "radix prefix cache hash hit rate");
  const double radix_hit_rate = clamp_unit_interval(require_finite(
      reuse_rate * 0.96, "radix prefix cache radix hit rate"));
  const double block_sharing_ratio = require_finite(
      static_cast<double>(shared_blocks) /
          static_cast<double>(std::max(1, total_blocks)),
      "radix prefix cache block sharing ratio");
  const int fragmentation_units = internal::checked_add_int(
      suffix_blocks % 7, split_count,
      "radix prefix cache fragmentation units");
  const double fragmentation_ratio =
      total_blocks <= 0
          ? 0.0
          : clamp_unit_interval(require_finite(
                static_cast<double>(fragmentation_units) /
                    static_cast<double>(total_blocks),
                "radix prefix cache fragmentation ratio"));
  const double eviction_cost_ms =
      total_blocks <= 0
          ? 0.0
          : require_finite(
                static_cast<double>(split_count) * lookup_us / 1000.0 +
                    fragmentation_ratio * static_cast<double>(shared_prefix) *
                        prefill_ms * 0.08,
                "radix prefix cache eviction cost");

  RadixPrefixCacheResult result;
  result.scenario = scenario;
  result.scenario.requests = requests;
  result.scenario.avg_prompt_tokens = avg_prompt;
  result.scenario.shared_prefix_tokens = shared_prefix;
  result.scenario.unique_suffix_tokens = unique_suffix;
  result.scenario.prefix_reuse_rate = reuse_rate;
  result.scenario.node_overhead_bytes = node_overhead;
  result.scenario.kv_bytes_per_token = kv_bytes;
  result.scenario.prefill_ms_per_token = prefill_ms;
  result.scenario.radix_lookup_us_per_node = lookup_us;
  result.estimated_nodes = nodes;
  result.reused_tokens = reused_tokens;
  result.saved_prefill_ms = saved_prefill;
  result.lookup_overhead_ms = lookup_overhead;
  result.radix_metadata_mb = metadata_mb;
  result.saved_kv_cache_gb = saved_kv_gb;
  result.net_ttft_saving_ms = net_saving;
  result.cache_efficiency_score = efficiency;
  result.recommendation =
      efficiency >= 0.35
          ? "enable_radix_prefix_cache"
          : (reuse_rate >= 0.15 && shared_prefix > 0
                 ? "enable_for_session_or_rag_prefixes"
                 : "prefer_plain_prefix_cache");
  result.shared_blocks = shared_blocks;
  result.suffix_blocks = suffix_blocks;
  result.split_count = split_count;
  result.exact_prefix_hit_rate = exact_hit_rate;
  result.hash_prefix_hit_rate = hash_hit_rate;
  result.radix_prefix_hit_rate = radix_hit_rate;
  result.block_sharing_ratio = block_sharing_ratio;
  result.fragmentation_ratio = fragmentation_ratio;
  result.eviction_cost_ms = eviction_cost_ms;
  return result;
}

std::vector<RadixPrefixCacheScenario> default_radix_prefix_cache_scenarios() {
  return {
      {"sglang_chat_templates", "sglang", 2000, 2048, 1200, 848, 0.72,
       96.0, 524288.0, 0.08, 0.50},
      {"sglang_rag_shared_system_prompt", "sglang", 5000, 4096, 1800,
       2296, 0.58, 96.0, 524288.0, 0.08, 0.60},
      {"vllm_prefix_cache_baseline", "vllm", 3000, 2048, 1024, 1024,
       0.45, 112.0, 524288.0, 0.09, 0.70},
      {"agentic_tool_sessions", "native", 1500, 8192, 4096, 4096, 0.38,
       128.0, 655360.0, 0.10, 0.90},
      {"low_reuse_open_chat", "native", 2000, 1536, 256, 1280, 0.08,
       96.0, 524288.0, 0.08, 0.50},
      {"native_release_adapter_scoped_prefix", "native", 2000, 4096,
       2048, 2048, 0.60, 96.0, 524288.0, 0.08, 0.50, 12,
       "base_model_id:adapter_id:adapter_revision:template_hash", false,
       true},
  };
}

}  // namespace inferlab
