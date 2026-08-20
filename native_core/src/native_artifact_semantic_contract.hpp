#pragma once

#include "artifact_evidence_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_evidence {

struct NativeArtifactSemanticContract {
  ArtifactContractProjection artifact_contract;
  bool regular_file = false;
  bool exact_artifact_type = false;
  bool required_fields_present = false;
  bool required_metrics_present = false;
  bool contract_ready = false;
  bool read_issue = false;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  double contract_coverage = 0.0;
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string text_read_status = "not_checked";
  std::string text_read_error;
  std::vector<std::string> missing_required_fields;
  std::vector<std::string> missing_required_metrics;
  std::vector<std::string> issues;
};

NativeArtifactSemanticContract evaluate_native_artifact_semantic_contract(
    const std::filesystem::path& artifact_path,
    const std::string& expected_artifact_type,
    const std::vector<std::string>& required_fields,
    const std::vector<std::string>& required_metrics,
    const EvidenceSnapshot& snapshot);

}  // namespace inferlab_evidence
