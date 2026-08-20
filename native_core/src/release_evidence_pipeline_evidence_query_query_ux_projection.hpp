#pragma once

#include "evidence_query_projection.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

struct EvidenceQueryQueryUxObjectPresenceProjection {
  bool query_ux = false;
  bool explain_tree = false;
  bool blocker_drilldown = false;
  bool repair_card_group_plan = false;
  bool repair_group_summary = false;
  bool repair_group_navigation = false;
  bool source_location_repair_worklist = false;
  bool repair_card_path_ranking = false;
  bool repair_card_rank_readiness_linkage = false;
  bool repair_card_source_exactness = false;
  bool source_location_category_coverage = false;
  bool readiness_path_drilldown = false;
};

struct EvidenceQueryQueryUxClosureProjection {
  EvidenceQueryQueryUxClosureProjection(
      const std::string& document, bool summary_usable, bool source_present,
      const inferlab_evidence::EvidenceQueryNavigationSignals& navigation);

  const inferlab_evidence::EvidenceQueryNavigationSignals& navigation;
  EvidenceQueryQueryUxObjectPresenceProjection objects;
  std::size_t required_object_count = 0;
  std::size_t present_object_count = 0;
  std::size_t missing_object_count = 0;
  std::size_t summary_field_contract_issue_count = 0;
  std::size_t contract_issue_count = 0;
  std::string repair_group_plan_status = "evidence_query_missing";
};

}  // namespace inferlab_release_pipeline
