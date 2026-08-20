#include "release_evidence_pipeline_evidence_query_query_ux_projection.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {
namespace {

using QueryUxObjectPresenceMember =
    bool EvidenceQueryQueryUxObjectPresenceProjection::*;

struct RequiredQueryUxObject {
  const char* key;
  QueryUxObjectPresenceMember presence;
  bool array = false;
};

constexpr std::array<RequiredQueryUxObject, 12> kRequiredQueryUxObjects{{
    {"query_ux", &EvidenceQueryQueryUxObjectPresenceProjection::query_ux},
    {"explain_tree",
     &EvidenceQueryQueryUxObjectPresenceProjection::explain_tree},
    {"blocker_drilldown",
     &EvidenceQueryQueryUxObjectPresenceProjection::blocker_drilldown},
    {"repair_card_group_plan",
     &EvidenceQueryQueryUxObjectPresenceProjection::repair_card_group_plan,
     true},
    {"repair_group_summary",
     &EvidenceQueryQueryUxObjectPresenceProjection::repair_group_summary},
    {"repair_group_navigation",
     &EvidenceQueryQueryUxObjectPresenceProjection::repair_group_navigation},
    {"source_location_repair_worklist",
     &EvidenceQueryQueryUxObjectPresenceProjection::
         source_location_repair_worklist},
    {"repair_card_path_ranking",
     &EvidenceQueryQueryUxObjectPresenceProjection::repair_card_path_ranking},
    {"repair_card_rank_readiness_linkage",
     &EvidenceQueryQueryUxObjectPresenceProjection::
         repair_card_rank_readiness_linkage},
    {"repair_card_source_exactness",
     &EvidenceQueryQueryUxObjectPresenceProjection::
         repair_card_source_exactness},
    {"source_location_category_coverage",
     &EvidenceQueryQueryUxObjectPresenceProjection::
         source_location_category_coverage},
    {"readiness_path_drilldown",
     &EvidenceQueryQueryUxObjectPresenceProjection::readiness_path_drilldown},
}};

bool required_surface_present(const std::string& document, const char* key,
                              const bool array) {
  return inferlab_artifact::contains_top_level_key_fallback(document, key) &&
         !(array ? inferlab_artifact::json_array_section_or_empty(document,
                                                                  key)
                 : inferlab_artifact::json_object_section_or_empty(document,
                                                                   key))
              .empty();
}

void project_required_object(const std::string& document, const char* key,
                             const bool array, bool& present,
                             std::size_t& present_count,
                             std::size_t& issue_count) {
  present = required_surface_present(document, key, array);
  if (present) {
    present_count = inferlab::internal::checked_add_size(
        present_count, 1U,
        "release pipeline Evidence Query Query UX present object count");
    return;
  }
  issue_count = inferlab::internal::checked_add_size(
      issue_count, 1U,
      "release pipeline Evidence Query Query UX object contract issue");
}

}  // namespace

EvidenceQueryQueryUxClosureProjection::EvidenceQueryQueryUxClosureProjection(
    const std::string& document, const bool summary_usable,
    const bool source_present,
    const inferlab_evidence::EvidenceQueryNavigationSignals&
        navigation_value)
    : navigation(navigation_value) {
  required_object_count = kRequiredQueryUxObjects.size();
  if (!summary_usable) {
    missing_object_count = required_object_count;
    contract_issue_count = source_present ? 1U : 0U;
    repair_group_plan_status =
        source_present ? "contract_invalid" : "evidence_query_missing";
    return;
  }

  std::size_t object_issue_count = 0;
  const std::string query_ux =
      inferlab_artifact::json_object_section_or_empty(document, "query_ux");
  for (const auto& object : kRequiredQueryUxObjects) {
    const bool root_query_ux = std::string(object.key) == "query_ux";
    project_required_object(root_query_ux ? document : query_ux, object.key,
                            object.array, objects.*(object.presence),
                            present_object_count, object_issue_count);
  }

  missing_object_count = required_object_count - present_object_count;
  summary_field_contract_issue_count =
      navigation.query_ux_summary_contract_issue_count;
  contract_issue_count = inferlab::internal::checked_add_size(
      object_issue_count, summary_field_contract_issue_count,
      "release pipeline Evidence Query Query UX contract issue count");
  repair_group_plan_status =
      contract_issue_count > 0U
          ? "contract_invalid"
          : (navigation.repair_card_navigation.group.group_count > 0U
                 ? "repair_groups_available"
                 : "repair_groups_empty");
}

}  // namespace inferlab_release_pipeline
