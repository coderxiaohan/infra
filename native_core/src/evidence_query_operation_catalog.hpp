#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct EvidenceQueryOperationSpec {
  std::string operation;
  std::string positional_form;
  std::string ux_form;
  std::string selector_hint;
  std::string target_hint;
  std::vector<std::string> supported_targets;
  std::string catalog_current_repo_action;
  std::string catalog_target_machine_action;
  std::string operation_current_repo_action;
  std::string operation_target_machine_action;
};

struct EvidenceQueryOperationCatalogSummary {
  std::size_t supported_operation_count = 0;
  std::size_t read_only_operation_count = 0;
  std::size_t snapshot_required_operation_count = 0;
  std::size_t selector_recommended_operation_count = 0;
  std::size_t target_required_operation_count = 0;
};

const std::vector<EvidenceQueryOperationSpec>& evidence_query_operation_catalog();

const std::vector<std::string>& evidence_query_operation_ux_forms();

const std::vector<std::string>& evidence_query_operation_names();

const EvidenceQueryOperationSpec* find_evidence_query_operation_spec(
    const std::string& operation,
    const std::string& target);

EvidenceQueryOperationCatalogSummary
evidence_query_operation_catalog_summary();

std::string evidence_query_usage_summary();

}  // namespace inferlab_evidence_query
