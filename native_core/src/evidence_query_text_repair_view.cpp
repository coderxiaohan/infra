#include "evidence_query_text_view.hpp"

#include "evidence_query_consumer_filter_signals.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_release_policy.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query::detail {

std::string evidence_query_text_repair_and_results(
    const EvidenceQueryRequest& request) {
  const auto& release = request.release_sources;
  std::ostringstream out;
  out << "shortest_paths:\n";
  for (const auto& path : request.shortest_paths) {
    const std::string first_action =
        path.steps.empty() ? "<none>" : path.steps.front();
    out << "- " << path.target << " | state=" << path.state
        << " | blocking_repair_cards=" << path.blocking_repair_card_count
        << " | source_locations="
        << release.repair_path_source_location_count
        << " | source_location_gaps="
        << release.repair_path_source_location_gap_count
        << " | catalog_flow_discrepancies="
        << path.catalog_flow_discrepancy_count
        << " | surface_consumer_fallback_blocked="
        << path.catalog_flow_surface_consumer_fallback_removal_blocked_count
        << " | surface_consumer_parity_refresh="
        << path.catalog_flow_surface_consumer_parity_refresh_required_count
        << " | surface_consumer_current_repo_actions="
        << path.catalog_flow_surface_consumer_current_repo_action_count
        << " | surface_consumer_target_machine_actions="
        << path.catalog_flow_surface_consumer_target_machine_action_count
        << " | field_coverage_repair_cards="
        << path.catalog_flow_consumer_field_coverage_repair_card_count
        << " | field_coverage_field_gaps="
        << path.catalog_flow_consumer_field_coverage_field_gap_count
        << " | field_coverage_parity_refresh="
        << path.catalog_flow_consumer_field_coverage_parity_refresh_required_count
        << " | native_core_audit_blockers="
        << native_core_audit_blocker_count(request.native_core_audit)
        << " | first_action=" << first_action << "\n";
  }
  out << "repair_card_group_plan:\n";
  for (const auto& group : request.repair_groups) {
    out << "- " << group.repair_environment
        << " | cards=" << group.repair_card_count
        << " | categories=" << group.categories.size()
        << " | artifacts=" << group.artifacts.size()
        << " | first_artifact="
        << (group.first_artifact.empty() ? "<none>" : group.first_artifact)
        << " | first_category="
        << (group.first_category.empty() ? "<none>" : group.first_category)
        << " | first_action="
        << (group.first_action.empty() ? "<none>" : group.first_action)
        << "\n";
  }
  out << "current_repo_actions:\n";
  for (const auto& action : request.repair_collection.current_repo_actions) {
    out << "- " << action << "\n";
  }
  out << "current_server_actions:\n";
  for (const auto& action : request.repair_collection.current_server_actions) {
    out << "- " << action << "\n";
  }
  out << "target_machine_actions:\n";
  for (const auto& action : request.repair_collection.target_machine_actions) {
    out << "- " << action << "\n";
  }
  out << "target_product_actions:\n";
  for (const auto& action : request.repair_collection.target_product_actions) {
    out << "- " << action << "\n";
  }
  out << "top_blockers:\n";
  for (const auto& entry : request.query_artifacts.blocker_category_counts) {
    out << "- " << entry.first << ": " << entry.second << " | "
        << repair_action(entry.first) << " | "
        << inferlab_evidence::repair_environment(entry.first) << "\n";
  }
  out << "records:\n";
  for (const auto* record : request.query_artifacts.matched_records) {
    out << "- " << record->artifact
        << " | status=" << result_status(*record, request.query)
        << " | blocker=" << blocker_category(*record)
        << " | confidence=" << evidence_confidence(*record) << "\n";
  }
  out << "claim_boundary: read-only evidence inspection; no benchmark or product execute claim is created\n";
  return out.str();
}

}  // namespace inferlab_evidence_query::detail
