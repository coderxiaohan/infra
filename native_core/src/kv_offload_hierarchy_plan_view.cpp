#include "kv_offload_hierarchy_plan_view.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"
#include "kv_offload_hierarchy_budget_profile.hpp"
#include "plan_json_utils.hpp"
#include "release_kv_budget_profile_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::kv_offload_hierarchy_plan_view {
namespace {

constexpr const char* kArtifactType =
    "native_kv_offload_hierarchy_plan";
constexpr const char* kArtifactPath =
    "results/raw/native_kv_offload_hierarchy_plan.json";
constexpr const char* kTargetName =
    "inferlab_kv_offload_hierarchy_plan";
constexpr const char* kRefreshCommand =
    "./build/inferlab_kv_offload_hierarchy_plan --repo-root . --output results/raw/native_kv_offload_hierarchy_plan.json";
constexpr const char* kClaimScope =
    "compiled KV offload hierarchy analytical fixture only; not live trace ingestion, runtime allocation/eviction, transport execution, product replay/execute, measured cache hits, benchmark performance, or release readiness";

bool approximately_equal(const double lhs, const double rhs) {
  const double scale = std::max({1.0, std::fabs(lhs), std::fabs(rhs)});
  return std::fabs(lhs - rhs) <= scale * 1e-9;
}

bool same_tier(const KvOffloadTier& lhs, const KvOffloadTier& rhs) {
  return lhs.kind == rhs.kind &&
         approximately_equal(lhs.capacity_gib, rhs.capacity_gib) &&
         approximately_equal(lhs.bandwidth_gbps, rhs.bandwidth_gbps) &&
         approximately_equal(lhs.access_latency_us, rhs.access_latency_us) &&
         approximately_equal(lhs.cost_per_gib_month,
                             rhs.cost_per_gib_month);
}

bool same_policy_profile(const KvOffloadPolicyProfile& lhs,
                         const KvOffloadPolicyProfile& rhs) {
  return lhs.policy == rhs.policy &&
         approximately_equal(lhs.retention_multiplier,
                             rhs.retention_multiplier);
}

bool same_workload(const KvOffloadWorkload& lhs,
                   const KvOffloadWorkload& rhs) {
  return lhs.name == rhs.name && lhs.requests == rhs.requests &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         approximately_equal(lhs.prompt_reuse_rate, rhs.prompt_reuse_rate) &&
         approximately_equal(lhs.cross_engine_reuse_rate,
                             rhs.cross_engine_reuse_rate) &&
         approximately_equal(lhs.session_pin_rate, rhs.session_pin_rate) &&
         approximately_equal(lhs.kv_bytes_per_token,
                             rhs.kv_bytes_per_token) &&
         approximately_equal(lhs.prefill_ms_per_token,
                             rhs.prefill_ms_per_token) &&
         lhs.prefill_chunk_tokens == rhs.prefill_chunk_tokens;
}

void require_non_negative(const double value, const char* name) {
  advanced_serving_internal::require_non_negative_finite(value, name);
}

void validate_workload(const KvOffloadWorkload& workload) {
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_unit_interval;

  require_nonempty(workload.name, "KV offload view workload name");
  if (workload.requests < 0 || workload.prompt_tokens < 0 ||
      workload.output_tokens < 0) {
    throw std::invalid_argument(
        "KV offload view requires non-negative workload counts");
  }
  require_unit_interval(workload.prompt_reuse_rate,
                        "KV offload view prompt reuse rate");
  require_unit_interval(workload.cross_engine_reuse_rate,
                        "KV offload view cross-engine reuse rate");
  require_unit_interval(workload.session_pin_rate,
                        "KV offload view session pin rate");
  require_non_negative(workload.kv_bytes_per_token,
                       "KV offload view bytes per token");
  require_non_negative(workload.prefill_ms_per_token,
                       "KV offload view prefill milliseconds");
  if (workload.prefill_chunk_tokens <= 0 ||
      (workload.prompt_tokens > 0 &&
       workload.prefill_chunk_tokens > workload.prompt_tokens)) {
    throw std::invalid_argument("KV offload view prefill chunk is invalid");
  }
}

void validate_tier(const KvOffloadTier& tier) {
  kv_offload_tier_kind_name(tier.kind);
  require_non_negative(tier.capacity_gib,
                       "KV offload view tier capacity");
  require_non_negative(tier.bandwidth_gbps,
                       "KV offload view tier bandwidth");
  require_non_negative(tier.access_latency_us,
                       "KV offload view tier access latency");
  require_non_negative(tier.cost_per_gib_month,
                       "KV offload view tier monthly cost");
}

void validate_placement(const KvOffloadPlacement& placement,
                        const KvOffloadTier& expected_tier) {
  validate_tier(placement.tier);
  if (!same_tier(placement.tier, expected_tier)) {
    throw std::invalid_argument(
        "KV offload view placement tier contract mismatch");
  }
  require_non_negative(placement.usable_capacity_gib,
                       "KV offload view usable tier capacity");
  require_non_negative(placement.placed_gib,
                       "KV offload view placed storage");
  require_non_negative(placement.placed_prompt_reuse_gib,
                       "KV offload view placed prompt storage");
  require_non_negative(placement.placed_session_output_gib,
                       "KV offload view placed output storage");
  require_non_negative(placement.transfer_ms,
                       "KV offload view transfer time");
  require_non_negative(placement.access_ms,
                       "KV offload view access time");
  require_non_negative(placement.monthly_cost,
                       "KV offload view placement cost");
  const double placement_components =
      internal::checked_add_finite_double(
          placement.placed_prompt_reuse_gib,
          placement.placed_session_output_gib,
          "KV offload view placement components");
  if (!approximately_equal(placement.placed_gib,
                           placement_components) ||
      placement.placed_gib > placement.usable_capacity_gib) {
    throw std::invalid_argument(
        "KV offload view placement accounting mismatch");
  }
}

void validate_result(const KvOffloadHierarchyResult& result,
                     const std::vector<KvOffloadTier>& tiers,
                     const KvOffloadWorkload& expected_workload,
                     const KvOffloadPolicyProfile& expected_profile) {
  using advanced_serving_internal::require_finite;
  using advanced_serving_internal::require_unit_interval;

  validate_workload(result.workload);
  kv_offload_retention_policy_name(result.policy_profile.policy);
  require_unit_interval(result.policy_profile.retention_multiplier,
                        "KV offload view retention multiplier");
  if (!same_workload(result.workload, expected_workload) ||
      !same_policy_profile(result.policy_profile, expected_profile)) {
    throw std::invalid_argument(
        "KV offload view record identity contract mismatch");
  }
  if (result.prompt_token_instances < 0 ||
      result.output_token_instances < 0 ||
      result.total_token_instances < 0 ||
      result.total_token_instances !=
          internal::checked_add_i64(
              result.prompt_token_instances,
              result.output_token_instances,
              "KV offload view total token instances")) {
    throw std::invalid_argument(
        "KV offload view token-instance accounting mismatch");
  }
  if (result.active_workload != (result.total_token_instances > 0)) {
    throw std::invalid_argument(
        "KV offload view active-workload state mismatch");
  }
  require_unit_interval(result.effective_prompt_reuse_rate,
                        "KV offload view effective prompt reuse");
  require_unit_interval(
      result.effective_session_output_reuse_rate,
      "KV offload view effective output reuse");
  require_non_negative(result.prompt_kv_gib,
                       "KV offload view prompt storage");
  require_non_negative(result.output_kv_gib,
                       "KV offload view output storage");
  require_non_negative(result.total_kv_gib,
                       "KV offload view total storage");
  require_non_negative(result.reusable_prompt_kv_gib,
                       "KV offload view reusable prompt storage");
  require_non_negative(result.reusable_session_output_kv_gib,
                       "KV offload view reusable output storage");
  require_non_negative(result.reusable_kv_gib,
                       "KV offload view reusable storage");
  require_non_negative(result.placed_reusable_kv_gib,
                       "KV offload view placed reusable storage");
  require_non_negative(result.placed_prompt_reuse_kv_gib,
                       "KV offload view placed prompt storage");
  require_non_negative(result.unplaced_reusable_kv_gib,
                       "KV offload view unplaced reusable storage");
  require_non_negative(result.cross_engine_eligible_kv_gib,
                       "KV offload view cross-engine storage");
  require_non_negative(result.usable_hbm_gib,
                       "KV offload view usable HBM");
  require_non_negative(result.hbm_pressure_gib,
                       "KV offload view HBM pressure");
  require_non_negative(result.hbm_saved_gib,
                       "KV offload view HBM saved");
  require_non_negative(result.ideal_saved_prefill_ms,
                       "KV offload view ideal prefill saving");
  require_non_negative(result.saved_prefill_ms,
                       "KV offload view placed prefill saving");
  if (result.comparable !=
      (result.active_workload && result.saved_prefill_ms > 0.0)) {
    throw std::invalid_argument(
        "KV offload view comparable state mismatch");
  }
  require_non_negative(result.transfer_ms,
                       "KV offload view aggregate transfer");
  require_non_negative(result.access_ms,
                       "KV offload view aggregate access");
  require_finite(result.net_ttft_saving_ms,
                 "KV offload view net TTFT saving");
  require_non_negative(result.monthly_cache_cost,
                       "KV offload view monthly cost");
  require_finite(result.reuse_efficiency,
                 "KV offload view reuse efficiency");
  kv_offload_recommendation_text(result.recommendation);

  if (result.placements.size() != tiers.size()) {
    throw std::invalid_argument(
        "KV offload view placement/tier cardinality mismatch");
  }
  double placed_total = 0.0;
  double placed_prompt_total = 0.0;
  double hbm_saved_total = 0.0;
  for (std::size_t index = 0; index < result.placements.size(); ++index) {
    const auto& placement = result.placements[index];
    validate_placement(placement, tiers[index]);
    placed_total = internal::checked_add_finite_double(
        placed_total, placement.placed_gib,
        "KV offload view placed total");
    placed_prompt_total = internal::checked_add_finite_double(
        placed_prompt_total, placement.placed_prompt_reuse_gib,
        "KV offload view placed prompt total");
    if (placement.tier.kind != KvOffloadTierKind::hbm) {
      hbm_saved_total = internal::checked_add_finite_double(
          hbm_saved_total, placement.placed_gib,
          "KV offload view HBM saved total");
    }
  }
  const double placed_and_unplaced =
      internal::checked_add_finite_double(
          result.placed_reusable_kv_gib,
          result.unplaced_reusable_kv_gib,
          "KV offload view placed/unplaced total");
  if (!approximately_equal(placed_total,
                           result.placed_reusable_kv_gib) ||
      !approximately_equal(placed_prompt_total,
                           result.placed_prompt_reuse_kv_gib) ||
      !approximately_equal(hbm_saved_total, result.hbm_saved_gib) ||
      !approximately_equal(result.reusable_kv_gib,
                           placed_and_unplaced)) {
    throw std::invalid_argument(
        "KV offload view aggregate placement accounting mismatch");
  }
}

void validate_plan(const KvOffloadHierarchyPlan& plan) {
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_unit_interval;

  require_nonempty(plan.assumptions.profile_name,
                   "KV offload view assumption profile");
  advanced_serving_internal::require_positive_finite(
      plan.assumptions.bytes_per_gib,
      "KV offload view bytes per GiB");
  require_unit_interval(
      plan.assumptions.hbm_session_reservation_weight,
      "KV offload view HBM reservation weight");
  require_non_negative(
      plan.assumptions.hierarchy_pressure_multiplier_threshold,
      "KV offload view hierarchy threshold");
  require_unit_interval(
      plan.assumptions.session_pin_recommendation_threshold,
      "KV offload view session-pin threshold");
  require_unit_interval(
      plan.assumptions.cross_engine_reuse_recommendation_threshold,
      "KV offload view cross-engine threshold");
  if (!plan.assumptions.prompt_reuse_priority ||
      !plan.assumptions.one_time_population_transfer ||
      !plan.assumptions.one_access_latency_per_occupied_tier ||
      plan.tiers.size() != 4U ||
      plan.policy_profiles.size() != 4U || plan.workloads.empty()) {
    throw std::invalid_argument(
        "KV offload view requires the complete disclosed fixture contract");
  }
  for (const auto& tier : plan.tiers) {
    validate_tier(tier);
  }
  std::vector<std::string> workload_names;
  workload_names.reserve(plan.workloads.size());
  for (const auto& workload : plan.workloads) {
    validate_workload(workload);
    workload_names.push_back(workload.name);
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      workload_names, "KV offload view workload identities");
  for (const auto& profile : plan.policy_profiles) {
    kv_offload_retention_policy_name(profile.policy);
    require_unit_interval(profile.retention_multiplier,
                          "KV offload view policy multiplier");
  }

  const std::size_t expected_records = internal::checked_product_size(
      plan.workloads.size(), plan.policy_profiles.size(),
      "KV offload view expected records");
  if (plan.records.size() != expected_records) {
    throw std::invalid_argument(
        "KV offload view record cardinality mismatch");
  }
  std::size_t active_count = 0;
  std::size_t comparable_count = 0;
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    const std::size_t workload_index =
        index / plan.policy_profiles.size();
    const std::size_t policy_index =
        index % plan.policy_profiles.size();
    validate_result(record, plan.tiers, plan.workloads[workload_index],
                    plan.policy_profiles[policy_index]);
    if (record.active_workload) {
      active_count = internal::checked_add_size(
          active_count, 1U, "KV offload view active count");
    }
    if (record.comparable) {
      comparable_count = internal::checked_add_size(
          comparable_count, 1U,
          "KV offload view comparable count");
    }
  }
  if (active_count != plan.active_record_count ||
      comparable_count != plan.comparable_record_count ||
      plan.best_record_index.has_value() != (comparable_count > 0U) ||
      plan.max_hbm_saved_record_index.has_value() != (active_count > 0U) ||
      (plan.best_record_index.has_value() &&
       (*plan.best_record_index >= plan.records.size() ||
        !plan.records[*plan.best_record_index].comparable)) ||
      (plan.max_hbm_saved_record_index.has_value() &&
       (*plan.max_hbm_saved_record_index >= plan.records.size() ||
        !plan.records[*plan.max_hbm_saved_record_index].active_workload))) {
    throw std::invalid_argument(
        "KV offload view summary/index accounting mismatch");
  }
}

std::string policy_profile_json(const KvOffloadPolicyProfile& profile,
                                const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "KV offload policy JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"policy\": "
      << inferlab_plan::json_string(
             kv_offload_retention_policy_name(profile.policy))
      << ",\n";
  out << nested << "\"retention_multiplier\": "
      << inferlab_plan::fixed6(profile.retention_multiplier) << "\n";
  out << pad << "}";
  return out.str();
}

std::string tier_json(const KvOffloadTier& tier, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "KV offload tier JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"access_latency_us\": "
      << inferlab_plan::fixed6(tier.access_latency_us) << ",\n";
  out << nested << "\"bandwidth_gbps\": "
      << inferlab_plan::fixed6(tier.bandwidth_gbps) << ",\n";
  out << nested << "\"capacity_gb\": "
      << inferlab_plan::fixed6(tier.capacity_gib) << ",\n";
  out << nested << "\"capacity_gib\": "
      << inferlab_plan::fixed6(tier.capacity_gib) << ",\n";
  out << nested << "\"cost_per_gb_month\": "
      << inferlab_plan::fixed6(tier.cost_per_gib_month) << ",\n";
  out << nested << "\"cost_per_gib_month\": "
      << inferlab_plan::fixed6(tier.cost_per_gib_month) << ",\n";
  out << nested << "\"tier\": "
      << inferlab_plan::json_string(kv_offload_tier_kind_name(tier.kind))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string workload_json(const KvOffloadWorkload& workload,
                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "KV offload workload JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cross_engine_reuse_rate\": "
      << inferlab_plan::fixed6(workload.cross_engine_reuse_rate) << ",\n";
  out << nested << "\"kv_bytes_per_token\": "
      << inferlab_plan::fixed6(workload.kv_bytes_per_token) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(workload.name) << ",\n";
  out << nested << "\"output_tokens\": " << workload.output_tokens
      << ",\n";
  out << nested << "\"prefill_ms_per_token\": "
      << inferlab_plan::fixed6(workload.prefill_ms_per_token) << ",\n";
  out << nested << "\"prefill_chunk_tokens\": "
      << workload.prefill_chunk_tokens << ",\n";
  out << nested << "\"prompt_reuse_rate\": "
      << inferlab_plan::fixed6(workload.prompt_reuse_rate) << ",\n";
  out << nested << "\"prompt_tokens\": " << workload.prompt_tokens
      << ",\n";
  out << nested << "\"requests\": " << workload.requests << ",\n";
  out << nested << "\"reuse_rate\": "
      << inferlab_plan::fixed6(workload.prompt_reuse_rate) << ",\n";
  out << nested << "\"session_output_reuse_rate\": "
      << inferlab_plan::fixed6(workload.session_pin_rate) << ",\n";
  out << nested << "\"session_pin_rate\": "
      << inferlab_plan::fixed6(workload.session_pin_rate) << "\n";
  out << pad << "}";
  return out.str();
}

std::string placement_json(const KvOffloadPlacement& placement,
                           const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "KV offload placement JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"access_latency_us\": "
      << inferlab_plan::fixed6(placement.tier.access_latency_us) << ",\n";
  out << nested << "\"access_ms\": "
      << inferlab_plan::fixed6(placement.access_ms) << ",\n";
  out << nested << "\"bandwidth_gbps\": "
      << inferlab_plan::fixed6(placement.tier.bandwidth_gbps) << ",\n";
  out << nested << "\"capacity_gb\": "
      << inferlab_plan::fixed6(placement.tier.capacity_gib) << ",\n";
  out << nested << "\"capacity_gib\": "
      << inferlab_plan::fixed6(placement.tier.capacity_gib) << ",\n";
  out << nested << "\"cost_per_gb_month\": "
      << inferlab_plan::fixed6(placement.tier.cost_per_gib_month) << ",\n";
  out << nested << "\"cost_per_gib_month\": "
      << inferlab_plan::fixed6(placement.tier.cost_per_gib_month) << ",\n";
  out << nested << "\"monthly_cost\": "
      << inferlab_plan::fixed6(placement.monthly_cost) << ",\n";
  out << nested << "\"placed_gb\": "
      << inferlab_plan::fixed6(placement.placed_gib) << ",\n";
  out << nested << "\"placed_gib\": "
      << inferlab_plan::fixed6(placement.placed_gib) << ",\n";
  out << nested << "\"placed_prompt_reuse_gib\": "
      << inferlab_plan::fixed6(placement.placed_prompt_reuse_gib)
      << ",\n";
  out << nested << "\"placed_session_output_gib\": "
      << inferlab_plan::fixed6(placement.placed_session_output_gib)
      << ",\n";
  out << nested << "\"tier\": "
      << inferlab_plan::json_string(
             kv_offload_tier_kind_name(placement.tier.kind))
      << ",\n";
  out << nested << "\"transfer_ms\": "
      << inferlab_plan::fixed6(placement.transfer_ms) << ",\n";
  out << nested << "\"usable_capacity_gib\": "
      << inferlab_plan::fixed6(placement.usable_capacity_gib) << "\n";
  out << pad << "}";
  return out.str();
}

std::string result_json(const KvOffloadHierarchyResult& result,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "KV offload result JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const int placement_indent = internal::checked_add_int(
      indent, 4, "KV offload placement-array JSON indent");
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"access_ms\": "
      << inferlab_plan::fixed6(result.access_ms) << ",\n";
  out << nested << "\"active_workload\": "
      << inferlab_plan::json_bool(result.active_workload) << ",\n";
  out << nested << "\"comparable\": "
      << inferlab_plan::json_bool(result.comparable) << ",\n";
  out << nested << "\"cross_engine_eligible_kv_gib\": "
      << inferlab_plan::fixed6(result.cross_engine_eligible_kv_gib)
      << ",\n";
  out << nested << "\"cross_engine_reusable_gb\": "
      << inferlab_plan::fixed6(result.cross_engine_eligible_kv_gib)
      << ",\n";
  out << nested << "\"effective_prompt_reuse_rate\": "
      << inferlab_plan::fixed6(result.effective_prompt_reuse_rate)
      << ",\n";
  out << nested << "\"effective_session_output_reuse_rate\": "
      << inferlab_plan::fixed6(
             result.effective_session_output_reuse_rate)
      << ",\n";
  out << nested << "\"hbm_pressure_gb\": "
      << inferlab_plan::fixed6(result.hbm_pressure_gib) << ",\n";
  out << nested << "\"hbm_pressure_gib\": "
      << inferlab_plan::fixed6(result.hbm_pressure_gib) << ",\n";
  out << nested << "\"hbm_saved_gb\": "
      << inferlab_plan::fixed6(result.hbm_saved_gib) << ",\n";
  out << nested << "\"hbm_saved_gib\": "
      << inferlab_plan::fixed6(result.hbm_saved_gib) << ",\n";
  out << nested << "\"ideal_saved_prefill_ms\": "
      << inferlab_plan::fixed6(result.ideal_saved_prefill_ms) << ",\n";
  out << nested << "\"monthly_cache_cost\": "
      << inferlab_plan::fixed6(result.monthly_cache_cost) << ",\n";
  out << nested << "\"net_ttft_saving_ms\": "
      << inferlab_plan::fixed6(result.net_ttft_saving_ms) << ",\n";
  out << nested << "\"output_kv_gib\": "
      << inferlab_plan::fixed6(result.output_kv_gib) << ",\n";
  out << nested << "\"output_token_instances\": "
      << result.output_token_instances << ",\n";
  out << nested << "\"placed_prompt_reuse_kv_gib\": "
      << inferlab_plan::fixed6(result.placed_prompt_reuse_kv_gib)
      << ",\n";
  out << nested << "\"placed_reusable_kv_gib\": "
      << inferlab_plan::fixed6(result.placed_reusable_kv_gib) << ",\n";
  out << nested << "\"placements\": [\n";
  for (std::size_t index = 0; index < result.placements.size(); ++index) {
    out << placement_json(result.placements[index], placement_indent)
        << (index == result.placements.size() - 1U ? "\n" : ",\n");
  }
  out << nested << "],\n";
  out << nested << "\"policy\": "
      << inferlab_plan::json_string(
             kv_offload_retention_policy_name(
                 result.policy_profile.policy))
      << ",\n";
  out << nested << "\"policy_retention_multiplier\": "
      << inferlab_plan::fixed6(
             result.policy_profile.retention_multiplier)
      << ",\n";
  out << nested << "\"prompt_kv_gib\": "
      << inferlab_plan::fixed6(result.prompt_kv_gib) << ",\n";
  out << nested << "\"prompt_token_instances\": "
      << result.prompt_token_instances << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             kv_offload_recommendation_text(result.recommendation))
      << ",\n";
  out << nested << "\"reusable_kv_gb\": "
      << inferlab_plan::fixed6(result.reusable_kv_gib) << ",\n";
  out << nested << "\"reusable_kv_gib\": "
      << inferlab_plan::fixed6(result.reusable_kv_gib) << ",\n";
  out << nested << "\"reusable_prompt_kv_gib\": "
      << inferlab_plan::fixed6(result.reusable_prompt_kv_gib) << ",\n";
  out << nested << "\"reusable_session_output_kv_gib\": "
      << inferlab_plan::fixed6(
             result.reusable_session_output_kv_gib)
      << ",\n";
  out << nested << "\"reuse_efficiency\": "
      << inferlab_plan::fixed6(result.reuse_efficiency) << ",\n";
  out << nested << "\"saved_prefill_ms\": "
      << inferlab_plan::fixed6(result.saved_prefill_ms) << ",\n";
  out << nested << "\"total_kv_gb\": "
      << inferlab_plan::fixed6(result.total_kv_gib) << ",\n";
  out << nested << "\"total_kv_gib\": "
      << inferlab_plan::fixed6(result.total_kv_gib) << ",\n";
  out << nested << "\"total_token_instances\": "
      << result.total_token_instances << ",\n";
  out << nested << "\"transfer_ms\": "
      << inferlab_plan::fixed6(result.transfer_ms) << ",\n";
  out << nested << "\"unplaced_reusable_kv_gib\": "
      << inferlab_plan::fixed6(result.unplaced_reusable_kv_gib)
      << ",\n";
  out << nested << "\"usable_hbm_gib\": "
      << inferlab_plan::fixed6(result.usable_hbm_gib) << ",\n";
  out << nested << "\"workload\": "
      << workload_json(result.workload, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

std::string optional_string(const KvOffloadHierarchyPlan& plan,
                            const std::optional<std::size_t>& index,
                            const bool policy) {
  if (!index.has_value()) {
    return "null";
  }
  const auto& record = plan.records[*index];
  return inferlab_plan::json_string(
      policy ? kv_offload_retention_policy_name(
                   record.policy_profile.policy)
             : record.workload.name);
}

std::string optional_number(const KvOffloadHierarchyPlan& plan,
                            const std::optional<std::size_t>& index,
                            const bool hbm_saved) {
  if (!index.has_value()) {
    return "null";
  }
  const auto& record = plan.records[*index];
  return inferlab_plan::fixed6(
      hbm_saved ? record.hbm_saved_gib : record.net_ttft_saving_ms);
}

}  // namespace

std::string artifact_json(const KvOffloadHierarchyPlan& plan) {
  validate_plan(plan);
  const auto kv_budget_profile = release_kv_budget_binding(plan);
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"release_kv_budget_profile\": "
      << inferlab_release_kv_budget::binding_json(kv_budget_profile, 2)
      << ",\n";
  out << "  \"canonical_owner\": {\n";
  out << "    \"domain\": \"advanced_serving_kv_offload_hierarchy.cpp\",\n";
  out << "    \"public_contract\": \"inferlab/advanced_serving_kv_tier.hpp\",\n";
  out << "    \"target\": "
      << inferlab_plan::json_string(kTargetName) << ",\n";
  out << "    \"artifact\": "
      << inferlab_plan::json_string(kArtifactPath) << ",\n";
  out << "    \"refresh_command\": "
      << inferlab_plan::json_string(kRefreshCommand) << "\n";
  out << "  },\n";
  out << "  \"analytical_fixture\": {\n";
  out << "    \"compiled_defaults\": true,\n";
  out << "    \"live_trace_ingested\": false,\n";
  out << "    \"runtime_allocation_executed\": false,\n";
  out << "    \"transport_executed\": false\n";
  out << "  },\n";
  out << "  \"config_reference\": {\n";
  out << "    \"path\": \"configs/kv_offload_hierarchy.example.json\",\n";
  out << "    \"reference_only\": true,\n";
  out << "    \"consumed_by_current_cli\": false\n";
  out << "  },\n";
  out << "  \"unit_contract\": {\n";
  out << "    \"storage_unit\": \"binary_gib\",\n";
  out << "    \"legacy_gb_fields_are_binary_gib_aliases\": true,\n";
  out << "    \"bandwidth_unit\": \"decimal_gigabits_per_second\",\n";
  out << "    \"transfer_scope\": \"one_time_population_per_non_hbm_occupied_tier\",\n";
  out << "    \"access_scope\": \"one_latency_charge_per_occupied_tier\"\n";
  out << "  },\n";
  out << "  \"assumptions\": {\n";
  out << "    \"bytes_per_gib\": "
      << inferlab_plan::fixed6(plan.assumptions.bytes_per_gib) << ",\n";
  out << "    \"cross_engine_reuse_recommendation_threshold\": "
      << inferlab_plan::fixed6(
             plan.assumptions.cross_engine_reuse_recommendation_threshold)
      << ",\n";
  out << "    \"hbm_session_reservation_weight\": "
      << inferlab_plan::fixed6(
             plan.assumptions.hbm_session_reservation_weight)
      << ",\n";
  out << "    \"hierarchy_pressure_multiplier_threshold\": "
      << inferlab_plan::fixed6(
             plan.assumptions.hierarchy_pressure_multiplier_threshold)
      << ",\n";
  out << "    \"one_access_latency_per_occupied_tier\": "
      << inferlab_plan::json_bool(
             plan.assumptions.one_access_latency_per_occupied_tier)
      << ",\n";
  out << "    \"one_time_population_transfer\": "
      << inferlab_plan::json_bool(
             plan.assumptions.one_time_population_transfer)
      << ",\n";
  out << "    \"profile_name\": "
      << inferlab_plan::json_string(plan.assumptions.profile_name)
      << ",\n";
  out << "    \"prompt_reuse_priority\": "
      << inferlab_plan::json_bool(
             plan.assumptions.prompt_reuse_priority)
      << ",\n";
  out << "    \"session_pin_recommendation_threshold\": "
      << inferlab_plan::fixed6(
             plan.assumptions.session_pin_recommendation_threshold)
      << "\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"active_record_count\": " << plan.active_record_count
      << ",\n";
  out << "    \"best_policy\": "
      << optional_string(plan, plan.best_record_index, true) << ",\n";
  out << "    \"best_workload\": "
      << optional_string(plan, plan.best_record_index, false) << ",\n";
  out << "    \"comparable_record_count\": "
      << plan.comparable_record_count << ",\n";
  out << "    \"max_hbm_saved_gb\": "
      << optional_number(plan, plan.max_hbm_saved_record_index, true)
      << ",\n";
  out << "    \"max_hbm_saved_gib\": "
      << optional_number(plan, plan.max_hbm_saved_record_index, true)
      << ",\n";
  out << "    \"max_net_ttft_saving_ms\": "
      << optional_number(plan, plan.best_record_index, false) << ",\n";
  out << "    \"policy_count\": " << plan.policy_profiles.size()
      << ",\n";
  out << "    \"record_count\": " << plan.records.size() << ",\n";
  out << "    \"tier_count\": " << plan.tiers.size() << ",\n";
  out << "    \"workload_count\": " << plan.workloads.size() << "\n";
  out << "  },\n";
  out << "  \"policy_profiles\": [\n";
  for (std::size_t index = 0; index < plan.policy_profiles.size(); ++index) {
    out << policy_profile_json(plan.policy_profiles[index], 4)
        << (index == plan.policy_profiles.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"tiers\": [\n";
  for (std::size_t index = 0; index < plan.tiers.size(); ++index) {
    out << tier_json(plan.tiers[index], 4)
        << (index == plan.tiers.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << result_json(plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": [\n";
  out << "    \"CMake configure/build\",\n";
  out << "    \"native execution and exact artifact parity\",\n";
  out << "    \"live trace ingestion and runtime allocation/eviction\",\n";
  out << "    \"transport execution and measured cache hits\",\n";
  out << "    \"product replay/execute and benchmark\",\n";
  out << "    \"Summary, Prometheus, Final Report, GPU validation\"\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kv_offload_hierarchy_plan_view
