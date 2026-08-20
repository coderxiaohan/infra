#include "native_artifact_semantic_contract.hpp"

#include "advanced_serving_internal.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_evidence {
namespace {

void add_check(NativeArtifactSemanticContract& contract,
               const bool satisfied,
               const std::string& issue) {
  contract.required_check_count = inferlab::internal::checked_add_size(
      contract.required_check_count, 1U,
      "native artifact semantic required check count");
  if (satisfied) {
    contract.satisfied_check_count = inferlab::internal::checked_add_size(
        contract.satisfied_check_count, 1U,
        "native artifact semantic satisfied check count");
  } else {
    inferlab_plan::append_unique_nonempty_string(contract.issues, issue);
  }
}

}  // namespace

NativeArtifactSemanticContract evaluate_native_artifact_semantic_contract(
    const std::filesystem::path& artifact_path,
    const std::string& expected_artifact_type,
    const std::vector<std::string>& required_fields,
    const std::vector<std::string>& required_metrics,
    const EvidenceSnapshot& snapshot) {
  if (artifact_path.empty() ||
      !inferlab_plan::has_non_whitespace(expected_artifact_type)) {
    throw std::invalid_argument(
        "native artifact semantic contract requires a path and artifact type");
  }
  inferlab::advanced_serving_internal::require_unique_nonempty_strings(
      required_fields, "native artifact semantic required fields");
  inferlab::advanced_serving_internal::require_unique_nonempty_strings(
      required_metrics, "native artifact semantic required metrics");

  NativeArtifactSemanticContract contract;
  contract.path_probe_status =
      inferlab_artifact::path_probe_status(artifact_path);
  contract.path_probe_error =
      contract.path_probe_status == "path_probe_error"
          ? (snapshot.read_result.error.empty()
                 ? "failed to inspect artifact path"
                 : snapshot.read_result.error)
          : "";
  contract.text_read_status = snapshot.read_result.status;
  contract.text_read_error = snapshot.read_result.error;
  contract.regular_file = contract.path_probe_status == "file";
  contract.artifact_contract = evaluate_artifact_contract(snapshot);
  contract.required_check_count =
      contract.artifact_contract.required_check_count;
  contract.satisfied_check_count =
      contract.artifact_contract.satisfied_check_count;
  contract.issues = contract.artifact_contract.issues;

  add_check(contract, contract.regular_file,
            "artifact_path_is_not_a_regular_file");
  contract.exact_artifact_type =
      contract.artifact_contract.parse_ok &&
      contract.artifact_contract.artifact_type == expected_artifact_type;
  add_check(contract, contract.exact_artifact_type,
            "artifact_type_does_not_match_canonical_identity");

  const std::string& document = snapshot.read_result.text;
  for (const auto& field : required_fields) {
    const bool present = contract.artifact_contract.parse_ok &&
                         inferlab_artifact::contains_top_level_key_fallback(
                             document, field);
    add_check(contract, present, "required_field_missing:" + field);
    if (!present) contract.missing_required_fields.push_back(field);
  }
  for (const auto& metric : required_metrics) {
    const bool present = contract.artifact_contract.parse_ok &&
                         inferlab_artifact::contains_semantic_key_fallback(
                             document, metric);
    add_check(contract, present, "required_metric_missing:" + metric);
    if (!present) contract.missing_required_metrics.push_back(metric);
  }

  contract.required_fields_present =
      contract.missing_required_fields.empty();
  contract.required_metrics_present =
      contract.missing_required_metrics.empty();
  if (contract.required_check_count == 0U ||
      contract.satisfied_check_count > contract.required_check_count) {
    throw std::logic_error(
        "native artifact semantic contract accounting is invalid");
  }
  contract.contract_coverage =
      static_cast<double>(contract.satisfied_check_count) /
      static_cast<double>(contract.required_check_count);
  if (!std::isfinite(contract.contract_coverage) ||
      contract.contract_coverage < 0.0 ||
      contract.contract_coverage > 1.0) {
    throw std::logic_error(
        "native artifact semantic contract coverage is invalid");
  }
  contract.contract_ready =
      contract.satisfied_check_count == contract.required_check_count;
  contract.read_issue =
      contract.path_probe_status == "path_probe_error" ||
      (contract.artifact_contract.present &&
       (!contract.regular_file || !contract.artifact_contract.readable ||
        !contract.artifact_contract.parse_ok));
  return contract;
}

}  // namespace inferlab_evidence
