#pragma once

#include "evidence_domain_types.hpp"
#include "mature_product_profile_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::mature_product_profiles {

struct ProductProfileArtifactContract {
  const ProductProfileSpec* profile = nullptr;
  bool present = false;
  bool regular_file = false;
  bool readable = false;
  bool parse_ok = false;
  bool schema_profile_v1 = false;
  bool product_value_present = false;
  bool product_name_value_present = false;
  bool category_value_present = false;
  bool official_sources_present = false;
  bool adapter_boundary_present = false;
  bool config_snapshot_present = false;
  bool planned_commands_present = false;
  bool raw_metrics_present = false;
  bool not_implemented_boundary_present = false;
  bool local_optimization_scope_present = false;
  bool claim_scope_present = false;
  bool execute_gate_present = false;
  bool product_boundary_present = false;
  bool mode_present = false;
  bool status_present = false;
  bool executed_present = false;
  bool replayed_present = false;
  bool mode_flags_consistent = false;
  bool mode_status_consistent = false;
  bool claim_scope_consistent = false;
  bool replay_evidence_consistent = false;
  bool replay_metric_evidence_present = false;
  bool contract_ready = false;
  bool product_evidence_eligible = false;
  bool can_claim_product_performance = false;
  bool executed = false;
  bool replayed = false;
  std::uintmax_t byte_size = 0U;
  std::size_t required_field_count = 0U;
  std::size_t present_field_count = 0U;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  double required_field_pass_rate = 0.0;
  double boundary_score = 0.0;
  double contract_coverage = 0.0;
  std::string path_probe_status = "not_checked";
  std::string text_read_status = "not_checked";
  std::string text_read_error;
  std::string mode = "missing";
  std::string status = "missing";
  std::string real_performance_claim;
  std::vector<std::string> missing_fields;
  std::vector<std::string> invalid_type_fields;
  std::vector<std::string> issues;
};

const ProductProfileSpec& contract_profile(
    const ProductProfileArtifactContract& contract);

ProductProfileArtifactContract evaluate_product_profile_artifact_contract(
    const ProductProfileSpec& profile,
    const std::filesystem::path& artifact_path,
    const inferlab_evidence::EvidenceSnapshot& snapshot);

}  // namespace inferlab::mature_product_profiles
