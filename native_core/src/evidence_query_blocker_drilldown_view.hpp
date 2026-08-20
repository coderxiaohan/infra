#pragma once

#include "evidence_json_writer.hpp"
#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {

inline std::string explain_tree_json(
    const BlockerDrilldown& drilldown,
    const QueryOperation& operation,
    const int indent) {
  const auto& category_counts = drilldown.category_counts;
  const auto& source_stats = drilldown.source_stats;
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<std::string> empty_locations;
  std::vector<inferlab_evidence::ExplainTreeNode> nodes;
  nodes.reserve(category_counts.size());
  for (const auto& entry : category_counts) {
    const std::vector<std::string> locations =
        wiring_locations_for_category(entry.first);
    const bool source_exact =
        category_has_exact_wiring_locations(entry.first) && !locations.empty();
    const inferlab_evidence::RepairEnvironment environment =
        inferlab_evidence::repair_environment_kind(entry.first);
    const bool current_repo =
        environment ==
        inferlab_evidence::RepairEnvironment::kCurrentRepoFixable;
    const std::vector<std::string>& exact_locations =
        source_exact ? locations : empty_locations;
    const std::vector<std::string>& fallback_locations =
        source_exact ? empty_locations : locations;
    inferlab_evidence::ExplainTreeNode node;
    node.fields = {
        inferlab_evidence::evidence_json_string_field("category", entry.first),
        inferlab_evidence::evidence_json_unsigned_field("count", entry.second),
        inferlab_evidence::evidence_json_string_field(
            "why_blocked", repair_action(entry.first)),
        inferlab_evidence::evidence_json_string_field(
            "repair_environment",
            inferlab_evidence::repair_environment_name(environment)),
        inferlab_evidence::evidence_json_string_field(
            "source_resolution_status",
            source_exact ? "exact_source_location_available"
                         : "source_location_mapping_missing"),
        inferlab_evidence::evidence_json_boolean_field("source_exact",
                                                        source_exact),
        inferlab_evidence::evidence_json_unsigned_field(
            "source_location_count", exact_locations.size()),
        inferlab_evidence::evidence_json_boolean_field(
            "source_location_gap", !source_exact),
        inferlab_evidence::evidence_json_boolean_field(
            "current_repo_action_required", current_repo),
        inferlab_evidence::evidence_json_boolean_field(
            "target_or_validation_action_required", !current_repo),
        inferlab_evidence::evidence_json_string_array_field(
            "exact_wiring_locations", exact_locations),
        inferlab_evidence::evidence_json_string_array_field(
            "fallback_investigation_locations", fallback_locations),
        inferlab_evidence::evidence_json_string_field(
            "next_action",
            source_exact
                ? "inspect exact_wiring_locations and matching repair cards before changing code or report wiring"
                : "add category-to-source mapping before presenting this blocker as source-exact")};
    nodes.push_back(std::move(node));
  }

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(operation.selector) << ",\n";
  out << nested
      << "\"root_cause\": \"evidence query blockers are derived from existing artifact metadata, warehouse records, and local wiring probes\",\n";
  out << nested << "\"node_count\": " << source_stats.node_count << ",\n";
  out << nested << "\"source_exact_node_count\": "
      << source_stats.source_exact_node_count << ",\n";
  out << nested << "\"source_gap_node_count\": "
      << source_stats.source_gap_node_count << ",\n";
  out << nested << "\"current_repo_source_gap_count\": "
      << source_stats.current_repo_source_gap_count << ",\n";
  out << nested << "\"target_or_validation_source_gap_count\": "
      << source_stats.target_or_validation_source_gap_count << ",\n";
  out << nested << "\"source_exactness_percent\": "
      << explain_tree_source_exactness_percent(source_stats) << ",\n";
  out << nested << "\"nodes\": "
      << inferlab_evidence::render_explain_tree_nodes_json(nodes, indent + 2)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "explain tree is a read-only blocker explanation; it does not execute repair commands or create benchmark evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string blocker_drilldown_json(
    const BlockerDrilldown& drilldown,
    const QueryOperation& operation,
    const SelectorResolution& resolution,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<std::string> no_claim_impact = {
      "no blocking claim impact detected for current explain selector"};
  const std::vector<std::string>& affected_claims =
      drilldown.affected_claims.empty() ? no_claim_impact
                                        : drilldown.affected_claims;
  const std::vector<std::string> next_actions = {
      drilldown.first_action,
      drilldown.source_location_gap_count == 0U
          ? "inspect exact_wiring_locations before changing code or report wiring"
          : "add exact source/config/report mappings for blocker categories with source-location gaps",
      "refresh the owning evidence producers only when validation is enabled"};

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(operation.selector) << ",\n";
  out << nested << "\"selector_focus\": "
      << selector_focus_json(resolution, indent + 2) << ",\n";
  out << nested << "\"blocker_record_count\": "
      << drilldown.blocker_record_count << ",\n";
  out << nested << "\"category_count\": "
      << drilldown.category_counts.size() << ",\n";
  out << nested << "\"source_exact_node_count\": "
      << drilldown.source_stats.source_exact_node_count << ",\n";
  out << nested << "\"source_gap_node_count\": "
      << drilldown.source_stats.source_gap_node_count << ",\n";
  out << nested << "\"current_repo_source_gap_count\": "
      << drilldown.source_stats.current_repo_source_gap_count << ",\n";
  out << nested << "\"target_or_validation_source_gap_count\": "
      << drilldown.source_stats.target_or_validation_source_gap_count << ",\n";
  out << nested << "\"source_exactness_percent\": "
      << explain_tree_source_exactness_percent(drilldown.source_stats)
      << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << drilldown.source_location_gap_count << ",\n";
  out << nested << "\"affected_claim_count\": " << affected_claims.size()
      << ",\n";
  out << nested << "\"affected_claims\": "
      << inferlab_plan::json_string_array(affected_claims, indent + 2)
      << ",\n";
  out << nested << "\"exact_wiring_location_count\": "
      << drilldown.exact_wiring_locations.size() << ",\n";
  out << nested << "\"exact_wiring_locations\": "
      << inferlab_plan::json_string_array(drilldown.exact_wiring_locations,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << drilldown.current_repo_actions.size() << ",\n";
  out << nested << "\"current_server_action_count\": "
      << drilldown.current_server_actions.size() << ",\n";
  out << nested << "\"target_machine_action_count\": "
      << drilldown.target_machine_actions.size() << ",\n";
  out << nested << "\"target_product_action_count\": "
      << drilldown.target_product_actions.size() << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(drilldown.current_repo_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_server_actions\": "
      << inferlab_plan::json_string_array(drilldown.current_server_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(drilldown.target_machine_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_product_actions\": "
      << inferlab_plan::json_string_array(drilldown.target_product_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"first_artifact\": "
      << inferlab_plan::json_string(drilldown.first_artifact) << ",\n";
  out << nested << "\"first_category\": "
      << inferlab_plan::json_string(drilldown.first_category) << ",\n";
  out << nested << "\"first_location\": "
      << inferlab_plan::json_string(drilldown.first_location) << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(drilldown.first_action) << ",\n";
  out << nested << "\"next_actions\": "
      << inferlab_plan::json_string_array(next_actions, indent + 2) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "blocker drilldown is read-only explanation metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
