#include "artifact_catalog_single_source_views.hpp"

#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_catalog_single_source {
namespace {

void write_ordered_step(std::ostringstream& out,
                        const std::string& pad,
                        const std::size_t index,
                        const std::string& id,
                        const std::string& environment,
                        const std::string& action,
                        const std::string& claim_boundary,
                        const bool is_last,
                        const SurfaceProjection* surface = nullptr) {
  out << "\n" << pad << "{\n";
  out << pad << "  \"step\": " << index << ",\n";
  out << pad << "  \"id\": " << inferlab_plan::json_string(id) << ",\n";
  out << pad << "  \"environment\": "
      << inferlab_plan::json_string(environment) << ",\n";
  if (surface != nullptr) {
    out << pad << "  \"surface\": "
        << inferlab_plan::json_string(surface->status.surface) << ",\n";
    out << pad << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface->status.fallback_location)
        << ",\n";
  }
  out << pad << "  \"action\": " << inferlab_plan::json_string(action)
      << ",\n";
  out << pad << "  \"claim_boundary\": "
      << inferlab_plan::json_string(claim_boundary) << "\n";
  out << pad << "}" << (is_last ? "" : ",");
}

}  // namespace

std::string fallback_removal_shortest_path_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const int step_indent = catalog_view_indent(indent, 4);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  const std::string step_pad = inferlab_plan::json_indent(step_indent);
  const auto* first = first_fallback_surface(projection);

  std::ostringstream out;
  out << "{\n";
  out << child << "\"present\": true,\n";
  out << child << "\"state\": "
      << inferlab_plan::json_string(projection.fallback_shortest_path_state)
      << ",\n";
  out << child << "\"step_count\": "
      << projection.fallback_shortest_path_step_count << ",\n";
  out << child << "\"current_repo_step_count\": "
      << projection.fallback_shortest_path_current_repo_step_count << ",\n";
  out << child << "\"target_machine_step_count\": "
      << projection.fallback_shortest_path_target_machine_step_count << ",\n";
  out << child << "\"source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << child << "\"primary_environment\": "
      << inferlab_plan::json_string(
             projection.fallback_removal_blocked
                 ? "current_repo_then_target_machine"
                 : "review_clear")
      << ",\n";
  out << child << "\"primary_blocker\": "
      << inferlab_plan::json_string(
             projection.fallback_removal_blocked
                 ? "catalog_fallback_retained_until_parity"
                 : "none")
      << ",\n";
  out << child << "\"first_surface\": "
      << inferlab_plan::json_string(first == nullptr ? ""
                                                      : first->status.surface)
      << ",\n";
  out << child << "\"first_fallback_location\": "
      << inferlab_plan::json_string(
             first == nullptr ? "" : first->status.fallback_location)
      << ",\n";
  out << child << "\"first_current_repo_action\": "
      << inferlab_plan::json_string(
             first == nullptr ? "no fallback removal action remains"
                              : first->status.next_action)
      << ",\n";
  out << child << "\"first_target_machine_action\": "
      << inferlab_plan::json_string(
             first == nullptr
                 ? "no target-machine parity refresh is required"
                 : "refresh the downstream surface, compare catalog-derived output against retained fallback output, and record parity evidence")
      << ",\n";
  out << child << "\"steps\": [";
  if (first != nullptr) {
    const std::string boundary =
        "fallback-removal shortest-path step is cleanup routing only; it does not refresh artifacts, delete fallback wiring, or prove parity";
    write_ordered_step(
        out, step_pad, 1U, "confirm_registry_primary",
        "current_repo_fixable",
        "confirm catalog-derived entries remain the primary source for the fallback surface",
        boundary, false, first);
    write_ordered_step(out, step_pad, 2U, "refresh_downstream_surface",
                       "target_gpu_or_execute_machine",
                       "refresh the downstream surface when validation is enabled",
                       boundary, false, first);
    write_ordered_step(
        out, step_pad, 3U, "record_downstream_parity",
        "target_gpu_or_execute_machine",
        "compare catalog-derived output with retained fallback output and record parity in results/raw",
        boundary, false, first);
    write_ordered_step(
        out, step_pad, 4U, "remove_retained_fallback",
        "current_repo_fixable",
        "remove the retained fallback location only after parity evidence exists",
        boundary, true, first);
    out << "\n" << child;
  }
  out << "],\n";
  out << child
      << "\"claim_boundary\": \"fallback removal shortest path is "
         "read-only cleanup guidance; it does not run refresh commands, "
         "remove fallback lists, render reports, or prove downstream "
         "parity\"\n";
  out << pad << "}";
  return out.str();
}

std::string fallback_removal_parity_gate_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const int item_indent = catalog_view_indent(indent, 4);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  const std::string item_pad = inferlab_plan::json_indent(item_indent);
  const auto* first = first_fallback_surface(projection);

  std::ostringstream out;
  out << "{\n";
  out << child << "\"present\": true,\n";
  out << child << "\"decision\": "
      << inferlab_plan::json_string(projection.fallback_parity_gate_decision)
      << ",\n";
  out << child << "\"removal_allowed\": "
      << inferlab_plan::json_bool(!projection.fallback_removal_blocked)
      << ",\n";
  out << child << "\"blocker_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"blocking_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"fallback_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"current_repo_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"target_machine_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << child << "\"blocking_surface_source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << child << "\"first_blocking_surface\": "
      << inferlab_plan::json_string(first == nullptr ? ""
                                                      : first->status.surface)
      << ",\n";
  out << child << "\"first_blocking_fallback_location\": "
      << inferlab_plan::json_string(
             first == nullptr ? "" : first->status.fallback_location)
      << ",\n";
  out << child << "\"current_repo_next_action\": "
      << inferlab_plan::json_string(
             first == nullptr
                 ? "keep catalog-only surfaces documented and wait for normal artifact refresh"
                 : first->status.next_action)
      << ",\n";
  out << child << "\"target_machine_next_action\": "
      << inferlab_plan::json_string(
             first == nullptr
                 ? "refresh catalog-managed consumers during normal validation"
                 : "refresh all downstream catalog consumers, compare catalog-derived output with retained fallback output, and record parity evidence before fallback deletion")
      << ",\n";
  out << child << "\"required_evidence\": "
      << inferlab_plan::json_string_array(
             {"refreshed native_artifact_catalog_single_source.json",
              "refreshed summary artifact containing catalog fields",
              "refreshed Prometheus export containing catalog metrics",
              "refreshed final report showing catalog parity state",
              "recorded downstream parity before deleting fallback lists"},
             child_indent)
      << ",\n";
  out << child << "\"blocking_surfaces\": [";
  for (std::size_t i = 0; i < projection.fallback_surface_indices.size();
       ++i) {
    const auto& surface = fallback_surface_at(projection, i);
    out << "\n" << item_pad << "{\n";
    out << item_pad << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << item_pad << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << item_pad << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << item_pad << "  \"consumer_status\": "
        << inferlab_plan::json_string(surface.status.consumer_status)
        << ",\n";
    out << item_pad << "  \"parity_refresh_required\": true,\n";
    out << item_pad << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << item_pad
        << "  \"target_machine_action\": \"refresh this downstream "
           "surface, compare catalog-derived output with the retained "
           "fallback output, and record parity evidence before fallback "
           "deletion\",\n";
    out << item_pad
        << "  \"claim_boundary\": \"blocking surface explains why fallback "
           "removal is blocked; it does not refresh downstream artifacts, "
           "delete fallback wiring, or prove parity\"\n";
    out << item_pad << "}";
    const bool is_last =
        i == projection.fallback_surface_indices.size() - 1U;
    out << (is_last ? "" : ",");
  }
  if (!projection.fallback_surface_indices.empty()) out << "\n" << child;
  out << "],\n";
  out << child << "\"ordered_steps\": [";
  if (projection.fallback_removal_blocked) {
    const std::string boundary =
        "parity gate step is read-only cleanup guidance; it does not run validation or delete fallback wiring";
    write_ordered_step(
        out, item_pad, 1U, "keep_fallback_until_parity",
        "current_repo_fixable",
        "retain fallback lists while catalog-derived consumers are visible but unrefreshed",
        boundary, false);
    write_ordered_step(
        out, item_pad, 2U, "refresh_downstream_consumers",
        "target_or_validation_enabled_machine",
        "refresh summary, Prometheus, final report, evidence bundle, and trace correlation from catalog-derived views",
        boundary, false);
    write_ordered_step(
        out, item_pad, 3U, "record_parity_evidence",
        "target_or_validation_enabled_machine",
        "record that refreshed downstream outputs match the retained fallback surfaces",
        boundary, false);
    write_ordered_step(
        out, item_pad, 4U, "delete_fallback_after_parity",
        "current_repo_fixable",
        "remove the retained fallback locations only after parity evidence exists",
        boundary, true);
    out << "\n" << child;
  }
  out << "],\n";
  out << child
      << "\"claim_boundary\": \"fallback removal parity gate is read-only "
         "release cleanup metadata; it does not refresh artifacts, run "
         "validation, delete fallback lists, or prove "
         "benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

std::string fallback_removal_queue_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const int item_indent = catalog_view_indent(indent, 4);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  const std::string item_pad = inferlab_plan::json_indent(item_indent);
  std::ostringstream out;
  out << "{\n";
  out << child << "\"queue_item_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"fallback_removal_blocked_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"current_repo_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"target_machine_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << child << "\"queue_state\": "
      << inferlab_plan::json_string(projection.fallback_queue_state) << ",\n";
  out << child
      << "\"claim_boundary\": \"fallback removal queue is cleanup guidance "
         "only; it does not delete legacy fallback lists, execute refresh "
         "commands, or prove downstream parity\",\n";
  out << child << "\"queue\": [";
  for (std::size_t i = 0; i < projection.fallback_surface_indices.size();
       ++i) {
    const auto& surface = fallback_surface_at(projection, i);
    out << "\n" << item_pad << "{\n";
    out << item_pad << "  \"queue_id\": "
        << inferlab_plan::json_string("fallback_removal_" +
                                      surface.status.surface)
        << ",\n";
    out << item_pad << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << item_pad << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << item_pad << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << item_pad << "  \"consumer_status\": "
        << inferlab_plan::json_string(surface.status.consumer_status)
        << ",\n";
    out << item_pad << "  \"fallback_retained\": true,\n";
    out << item_pad << "  \"fallback_removal_blocked\": true,\n";
    out << item_pad << "  \"parity_refresh_required\": true,\n";
    out << item_pad
        << "  \"blocker\": \"retained fallback requires refreshed "
           "downstream parity evidence before deletion\",\n";
    out << item_pad << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << item_pad
        << "  \"target_machine_action\": \"refresh the downstream "
           "surface, compare catalog-derived output against the retained "
           "fallback output, and record parity before removing the fallback "
           "list\",\n";
    out << item_pad << "  \"ordered_steps\": "
        << inferlab_plan::json_string_array(
               {"keep catalog-derived view wired as the primary source",
                "refresh this downstream surface when validation is enabled",
                "compare catalog-derived output with retained fallback output",
                "record parity evidence in results/raw before deleting fallback",
                "remove the fallback location only after parity is recorded"},
               item_indent)
        << ",\n";
    out << item_pad << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_views.hpp",
                surface.status.fallback_location},
               item_indent)
        << ",\n";
    out << item_pad
        << "  \"claim_boundary\": \"queue item explains fallback cleanup "
           "only; it does not run refresh commands or create benchmark, "
           "replay, execute, or release-readiness evidence\"\n";
    out << item_pad << "}";
    const bool is_last =
        i == projection.fallback_surface_indices.size() - 1U;
    out << (is_last ? "" : ",");
  }
  if (!projection.fallback_surface_indices.empty()) out << "\n" << child;
  out << "]\n";
  out << pad << "}";
  return out.str();
}

std::string consumer_field_coverage_summary_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"surface_count\": " << projection.surfaces.size()
      << ",\n";
  out << pad << "  \"catalog_managed_surface_count\": "
      << projection.field_coverage_catalog_managed_surface_count << ",\n";
  out << pad << "  \"fallback_surface_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << pad << "  \"required_field_count\": "
      << projection.field_coverage_required_field_count << ",\n";
  out << pad << "  \"covered_field_count\": "
      << projection.field_coverage_covered_field_count << ",\n";
  out << pad << "  \"field_gap_count\": "
      << projection.field_coverage_field_gap_count << ",\n";
  out << pad << "  \"parity_refresh_required_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << pad << "  \"current_repo_action_count\": "
      << projection.field_coverage_current_repo_action_count << ",\n";
  out << pad << "  \"target_machine_action_count\": "
      << projection.field_coverage_target_machine_action_count << ",\n";
  out << pad << "  \"coverage_decision\": "
      << inferlab_plan::json_string(projection.field_coverage_decision)
      << ",\n";
  out << pad
      << "  \"claim_boundary\": \"consumer field coverage is registry "
         "metadata only; it does not execute downstream refresh commands, "
         "remove fallback lists, or prove parity\"\n";
  out << pad << "}";
  return out.str();
}

std::string consumer_field_coverage_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.surfaces.size(); ++i) {
    const auto& view = projection.surfaces[i].field_coverage;
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(view.surface) << ",\n";
    out << child << "  \"consumer_kind\": "
        << inferlab_plan::json_string(view.consumer_kind) << ",\n";
    out << child << "  \"catalog_view\": "
        << inferlab_plan::json_string(view.catalog_view) << ",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(view.source_of_truth) << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(view.fallback_location) << ",\n";
    out << child << "  \"consumes_catalog\": "
        << inferlab_plan::json_bool(view.consumes_catalog) << ",\n";
    out << child << "  \"fallback_retained\": "
        << inferlab_plan::json_bool(view.fallback_retained) << ",\n";
    out << child << "  \"field_coverage_status\": "
        << inferlab_plan::json_string(view.field_coverage_status) << ",\n";
    out << child << "  \"required_catalog_fields\": "
        << inferlab_plan::json_string_array(view.required_catalog_fields,
                                            child_indent)
        << ",\n";
    out << child << "  \"covered_catalog_fields\": "
        << inferlab_plan::json_string_array(view.covered_catalog_fields,
                                            child_indent)
        << ",\n";
    out << child << "  \"missing_catalog_fields\": "
        << inferlab_plan::json_string_array(view.missing_catalog_fields,
                                            child_indent)
        << ",\n";
    out << child << "  \"required_field_count\": "
        << view.required_catalog_fields.size() << ",\n";
    out << child << "  \"covered_field_count\": "
        << view.covered_catalog_fields.size() << ",\n";
    out << child << "  \"field_gap_count\": "
        << view.missing_catalog_fields.size() << ",\n";
    out << child << "  \"parity_refresh_required\": "
        << inferlab_plan::json_bool(view.fallback_retained) << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(view.current_repo_action) << ",\n";
    out << child << "  \"target_machine_action\": "
        << inferlab_plan::json_string(view.target_machine_action) << ",\n";
    out << child << "  \"claim_boundary\": "
        << inferlab_plan::json_string(view.claim_boundary) << "\n";
    out << child << "}";
    const bool is_last = i == projection.surfaces.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string migration_readiness_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"surface_count\": " << projection.surfaces.size()
      << ",\n";
  out << pad << "  \"surface_migration_ready_count\": "
      << projection.migration_ready_surface_count << ",\n";
  out << pad << "  \"surface_migration_fallback_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"surface_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"surface_current_repo_action_count\": "
      << projection.surface_current_repo_action_count << ",\n";
  out << pad << "  \"surface_target_machine_action_count\": "
      << projection.surface_target_machine_action_count << ",\n";
  out << pad << "  \"surface_consumer_migration_plan_count\": "
      << projection.surfaces.size() << ",\n";
  out << pad << "  \"surface_consumer_fallback_removal_blocked_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"surface_consumer_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"surface_consumer_current_repo_action_count\": "
      << projection.surface_current_repo_action_count << ",\n";
  out << pad << "  \"surface_consumer_target_machine_action_count\": "
      << projection.surface_target_machine_action_count << ",\n";
  out << pad << "  \"consumer_field_coverage_surface_count\": "
      << projection.surfaces.size() << ",\n";
  out << pad
      << "  \"consumer_field_coverage_catalog_managed_surface_count\": "
      << projection.field_coverage_catalog_managed_surface_count << ",\n";
  out << pad << "  \"consumer_field_coverage_fallback_surface_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << pad << "  \"consumer_field_coverage_field_gap_count\": "
      << projection.field_coverage_field_gap_count << ",\n";
  out << pad
      << "  \"consumer_field_coverage_parity_refresh_required_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << pad << "  \"contract_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "artifact_contract").consumes_catalog)
      << ",\n";
  out << pad << "  \"freshness_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "freshness_gate").consumes_catalog)
      << ",\n";
  out << pad << "  \"summary_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "summary").consumes_catalog)
      << ",\n";
  out << pad << "  \"prometheus_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "prometheus").consumes_catalog)
      << ",\n";
  out << pad << "  \"final_report_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "final_report").consumes_catalog)
      << ",\n";
  out << pad << "  \"evidence_bundle_can_consume_registry\": "
      << inferlab_plan::json_bool(
             require_surface(projection, "evidence_bundle").consumes_catalog)
      << ",\n";
  out << pad << "  \"decision\": "
      << inferlab_plan::json_string(projection.migration_readiness_decision)
      << ",\n";
  out << pad
      << "  \"claim_boundary\": \"migration readiness is catalog wiring "
         "metadata only; downstream parity still requires refreshed "
         "artifacts\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_catalog_single_source
