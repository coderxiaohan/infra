#include "release_evidence_pipeline_root_cause_projection.hpp"

#include "artifact_field_projection.hpp"
#include "checked_index.hpp"

#include <cstddef>

namespace inferlab_release_pipeline {
namespace {

std::size_t repair_card_contract_issues(
    const inferlab_evidence::RootCauseRepairCardProjection& cards,
    const bool source_present) {
  if (!source_present) return 0U;
  std::size_t issue_count = 0U;
  if (!cards.declared_repair_card_count_present ||
      cards.declared_repair_card_count_invalid ||
      !cards.repair_cards_field_present || !cards.repair_cards_array_valid ||
      cards.card_contract_issue || cards.environment_counts.split_schema_gap) {
    issue_count = inferlab::internal::checked_add_size(
        issue_count, 1U,
        "release pipeline Evidence Query repair-card contract issue");
  }
  return issue_count;
}

}  // namespace

ReleasePipelineRootCauseSourceProjection::
    ReleasePipelineRootCauseSourceProjection(
        const ReleasePipelineSourceContext& sources)
    : artifact(inferlab_high_value::require_managed_artifact(
          "evidence_query_result")),
      source_present(sources.evidence_query->metadata.present),
      source_readable(sources.evidence_query->read_result.readable),
      summary_usable(sources.evidence_query_summary_projection.usable),
      source_read_status(sources.evidence_query->read_result.status),
      repair_cards(inferlab_evidence::project_root_cause_repair_cards(
          sources.evidence_query_document)) {
  repair_card_contract_issue_count =
      repair_card_contract_issues(repair_cards, source_present);
  const auto flow_check = inferlab_artifact::project_unsigned_field(
      sources.evidence_query_summary,
      {"root_cause_flow_check_repair_card_count"});
  if (flow_check.present && !flow_check.invalid) {
    flow_check_repair_card_count = flow_check.value;
  }
  contract_issue_count = repair_card_contract_issue_count;
  if (source_present && (!source_readable || !summary_usable)) {
    contract_issue_count = inferlab::internal::checked_add_size(
        contract_issue_count, 1U,
        "release pipeline Evidence Query repair projection source issue");
  }
}

}  // namespace inferlab_release_pipeline
