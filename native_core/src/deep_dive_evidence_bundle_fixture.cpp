#include "deep_dive_evidence_bundle_fixture.hpp"

#include <cstddef>
#include <stdexcept>

namespace inferlab_deep_dive_bundle {
namespace {

bool ends_with(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

void validate_specs(const std::vector<ArtifactWiringSpec>& specs) {
  constexpr std::size_t kExpectedSpecCount = 8U;
  if (specs.size() != kExpectedSpecCount) {
    throw std::logic_error(
        "deep-dive artifact wiring registry count drift");
  }
  for (std::size_t i = 0U; i < specs.size(); ++i) {
    const auto& spec = specs[i];
    if (spec.contract_name.empty() || spec.artifact.empty() ||
        spec.legacy_bundle_name.empty() ||
        spec.source_path.empty() || spec.report_path.empty() ||
        spec.target.empty() || spec.matrix_entry.empty() ||
        spec.command.empty() || !ends_with(spec.artifact, ".json") ||
        spec.artifact.find('/') != std::string::npos ||
        spec.artifact.find('\\') != std::string::npos ||
        spec.artifact.find("..") != std::string::npos) {
      throw std::logic_error(
          "deep-dive artifact wiring registry contains an invalid field");
    }
    for (std::size_t j = i + 1U; j < specs.size(); ++j) {
      if (spec.contract_name == specs[j].contract_name ||
          spec.artifact == specs[j].artifact ||
          spec.legacy_bundle_name == specs[j].legacy_bundle_name ||
          spec.target == specs[j].target ||
          spec.matrix_entry == specs[j].matrix_entry) {
        throw std::logic_error(
            "deep-dive artifact wiring registry identity is duplicated");
      }
    }
  }
}

}  // namespace

const std::vector<ArtifactWiringSpec>& artifact_wiring_specs() {
  static const std::vector<ArtifactWiringSpec> specs = {
      {"native_kv_lifecycle_deep_dive",
       "native_kv_lifecycle_deep_dive.json",
       "kv_lifecycle",
       "native_core/src/kv_lifecycle_deep_dive_model.cpp",
       "results/raw/native_kv_lifecycle_deep_dive.json",
       "inferlab_kv_lifecycle_deep_dive",
       "native_kv_lifecycle_deep_dive_refresh",
       "./build/inferlab_kv_lifecycle_deep_dive --repo-root . --output results/raw/native_kv_lifecycle_deep_dive.json"},
      {"native_kv_tier_decision_deep_dive",
       "native_kv_tier_decision_deep_dive.json",
       "kv_tier_decision",
       "native_core/src/kv_tier_decision_deep_dive_model.cpp",
       "results/raw/native_kv_tier_decision_deep_dive.json",
       "inferlab_kv_tier_decision_deep_dive",
       "native_kv_tier_decision_deep_dive_refresh",
       "./build/inferlab_kv_tier_decision_deep_dive --repo-root . --output results/raw/native_kv_tier_decision_deep_dive.json"},
      {"native_pd_split_crossover_deep_dive",
       "native_pd_split_crossover_deep_dive.json",
       "pd_split_crossover",
       "native_core/src/pd_split_crossover_deep_dive_main.cpp",
       "results/raw/native_pd_split_crossover_deep_dive.json",
       "inferlab_pd_split_crossover_deep_dive",
       "native_pd_split_crossover_deep_dive_refresh",
       "./build/inferlab_pd_split_crossover_deep_dive --repo-root . --output results/raw/native_pd_split_crossover_deep_dive.json"},
      {"native_scheduler_root_cause_deep_dive",
       "native_scheduler_root_cause_deep_dive.json",
       "scheduler_root_cause",
       "native_core/src/scheduler_root_cause_deep_dive_main.cpp",
       "results/raw/native_scheduler_root_cause_deep_dive.json",
       "inferlab_scheduler_root_cause_deep_dive",
       "native_scheduler_root_cause_deep_dive_refresh",
       "./build/inferlab_scheduler_root_cause_deep_dive --repo-root . --output results/raw/native_scheduler_root_cause_deep_dive.json"},
      {"native_kernel_compatibility_matrix",
       "native_kernel_compatibility_matrix.json",
       "kernel_compatibility_matrix",
       "native_core/src/kernel_compatibility_matrix_main.cpp",
       "results/raw/native_kernel_compatibility_matrix.json",
       "inferlab_kernel_compatibility_matrix",
       "native_kernel_compatibility_matrix_refresh",
       "./build/inferlab_kernel_compatibility_matrix --repo-root . --output results/raw/native_kernel_compatibility_matrix.json"},
      {"product_replay_semantic_events",
       "product_replay_semantic_events.json",
       "product_replay_semantic_events",
       "native_core/src/product_replay_semantic_events_main.cpp",
       "results/raw/product_replay_semantic_events.json",
       "inferlab_product_replay_semantic_events",
       "product_replay_semantic_events_refresh",
       "./build/inferlab_product_replay_semantic_events --repo-root . --results results/raw --output results/raw/product_replay_semantic_events.json --format json"},
      {"product_replay_alignment",
       "native_product_replay_alignment.json",
       "product_replay_alignment",
       "native_core/src/product_replay_alignment_main.cpp",
       "results/raw/native_product_replay_alignment.json",
       "inferlab_product_replay_alignment",
       "native_product_replay_alignment_refresh",
       "./build/inferlab_product_replay_alignment --repo-root . --results results/raw --output results/raw/native_product_replay_alignment.json --format json"},
      {"native_deep_dive_contract_check",
       "native_deep_dive_contract_check.json",
       "deep_dive_contract_check",
       "native_core/src/deep_dive_contract_check_main.cpp",
       "results/raw/native_deep_dive_contract_check.json",
       "inferlab_deep_dive_contract_check",
       "native_deep_dive_contract_check_refresh",
       "./build/inferlab_deep_dive_contract_check --repo-root . --results results/raw --output results/raw/native_deep_dive_contract_check.json --format json"}};
  static const bool validated = [] {
    validate_specs(specs);
    return true;
  }();
  (void)validated;
  return specs;
}

const ArtifactWiringSpec& require_artifact_wiring_spec(
    const std::string& contract_name) {
  if (contract_name.empty()) {
    throw std::invalid_argument(
        "deep-dive artifact wiring lookup name must not be empty");
  }
  const ArtifactWiringSpec* match = nullptr;
  for (const auto& spec : artifact_wiring_specs()) {
    if (spec.contract_name != contract_name) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "deep-dive artifact wiring lookup is ambiguous: " +
          contract_name);
    }
    match = &spec;
  }
  if (match == nullptr) {
    throw std::out_of_range(
        "unknown deep-dive artifact wiring: " + contract_name);
  }
  return *match;
}

}  // namespace inferlab_deep_dive_bundle
