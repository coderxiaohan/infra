#include "artifact_catalog_single_source_views.hpp"

#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_catalog_single_source {
namespace {

std::string downstream_surfaces_json(
    const ArtifactProjection& artifact,
    const CatalogSingleSourceProjection& projection,
    const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.surfaces.size(); ++i) {
    const auto& surface = projection.surfaces[i];
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"registry_entry_present\": true,\n";
    out << child << "  \"consumer_status\": "
        << inferlab_plan::json_string(surface.status.consumer_status)
        << ",\n";
    out << child << "  \"expected_entry\": "
        << inferlab_plan::json_string(
               expected_surface_entry(artifact, surface))
        << ",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << child << "  \"fallback_retained\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"drift_status\": "
        << inferlab_plan::json_string(surface.drift_status)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"wiring presence only; refreshed "
           "results/raw artifact evidence is still required before release "
           "claims\"\n";
    out << child << "}";
    const bool is_last = i == projection.surfaces.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace

std::string downstream_wiring_matrix_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.artifacts.size(); ++i) {
    const auto& artifact = projection.artifacts[i];
    const auto& spec = artifact_spec(artifact);
    out << child << "{\n";
    out << child << "  \"artifact\": "
        << inferlab_plan::json_string(spec.artifact) << ",\n";
    out << child << "  \"target\": "
        << inferlab_plan::json_string(spec.target) << ",\n";
    out << child << "  \"report\": "
        << inferlab_plan::json_string(spec.report) << ",\n";
    out << child << "  \"registry_surface_count\": "
        << projection.surfaces.size() << ",\n";
    out << child << "  \"registry_entry_count\": "
        << projection.surfaces.size() << ",\n";
    out << child << "  \"fallback_retained_surface_count\": "
        << projection.fallback_surface_count << ",\n";
    out << child << "  \"missing_surface_count\": "
        << projection.missing_surface_count << ",\n";
    out << child << "  \"drift_status\": "
        << inferlab_plan::json_string(
               projection.drift_decision ==
                       "no_static_catalog_gap_detected"
                   ? "no_static_catalog_gap_detected"
                   : "static_catalog_gap_detected")
        << ",\n";
    out << child << "  \"surfaces\": "
        << downstream_surfaces_json(artifact, projection, child_indent)
        << "\n";
    out << child << "}";
    const bool is_last = i == projection.artifacts.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string catalog_drift_review_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"artifact_count\": " << projection.artifacts.size()
      << ",\n";
  out << pad << "  \"surface_count\": " << projection.surfaces.size()
      << ",\n";
  out << pad << "  \"matrix_entry_count\": "
      << projection.matrix_entry_count << ",\n";
  out << pad << "  \"registry_entry_count\": "
      << projection.matrix_entry_count << ",\n";
  out << pad << "  \"missing_surface_count\": "
      << projection.missing_surface_count << ",\n";
  out << pad << "  \"fallback_retained_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"fallback_retained_matrix_entry_count\": "
      << projection.fallback_matrix_entry_count << ",\n";
  out << pad << "  \"drift_decision\": "
      << inferlab_plan::json_string(projection.drift_decision) << ",\n";
  out << pad << "  \"next_action\": "
      << inferlab_plan::json_string(projection.drift_next_action) << ",\n";
  out << pad
      << "  \"claim_boundary\": \"drift review is source-registration "
         "evidence only; it does not prove downstream artifact freshness or "
         "benchmark execution\"\n";
  out << pad << "}";
  return out.str();
}

std::string single_source_parity_review_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"artifact_count\": " << projection.artifacts.size()
      << ",\n";
  out << pad << "  \"surface_count\": " << projection.surfaces.size()
      << ",\n";
  out << pad << "  \"matrix_entry_count\": "
      << projection.matrix_entry_count << ",\n";
  out << pad << "  \"fallback_retained_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"fallback_retained_matrix_entry_count\": "
      << projection.fallback_matrix_entry_count << ",\n";
  out << pad << "  \"duplicate_registration_count\": "
      << projection.duplicate_registration_count << ",\n";
  out << pad << "  \"missing_wiring_policy_count\": "
      << projection.missing_wiring_policy_count << ",\n";
  out << pad << "  \"repair_card_count\": "
      << projection.single_source_repair_card_count << ",\n";
  out << pad << "  \"current_repo_action_count\": "
      << projection.single_source_current_repo_action_count << ",\n";
  out << pad << "  \"target_machine_action_count\": "
      << projection.single_source_target_machine_action_count << ",\n";
  out << pad << "  \"parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << pad << "  \"parity_decision\": "
      << inferlab_plan::json_string(projection.single_source_parity_decision)
      << ",\n";
  out << pad << "  \"next_action\": "
      << inferlab_plan::json_string(projection.single_source_next_action)
      << ",\n";
  out << pad
      << "  \"claim_boundary\": \"single-source parity review explains "
         "catalog wiring only; it does not run downstream refresh commands "
         "or prove benchmark/replay/execute readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string single_source_repair_cards_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[";
  bool first = true;
  for (std::size_t i = 0; i < projection.fallback_surface_indices.size();
       ++i) {
    const auto& surface = fallback_surface_at(projection, i);
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child << "  \"card_id\": "
        << inferlab_plan::json_string("catalog_fallback_" +
                                      surface.status.surface)
        << ",\n";
    out << child
        << "  \"category\": \"catalog_fallback_retained\",\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"severity\": \"warning\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_then_validation_machine\",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << child << "  \"fallback_retained\": true,\n";
    out << child << "  \"affected_artifact_count\": "
        << projection.artifacts.size() << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << child
        << "  \"validation_action\": \"refresh the downstream artifact for "
           "this surface before removing explicit fallback wiring\",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_views.hpp",
                surface.status.fallback_location},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"repair card is catalog wiring guidance "
           "only; it does not execute refresh commands or create evidence\"\n";
    out << child << "}";
  }
  for (std::size_t i = 0; i < projection.unconsumed_surface_indices.size();
       ++i) {
    const auto& surface = unconsumed_surface_at(projection, i);
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child << "  \"card_id\": "
        << inferlab_plan::json_string(
               "catalog_surface_consumption_" + surface.status.surface)
        << ",\n";
    out << child
        << "  \"category\": \"catalog_surface_consumption_missing\",\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"severity\": \"blocking\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_fixable\",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_surface_contract.cpp",
                surface.status.fallback_location},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"surface-consumption repair only; it "
           "does not refresh downstream artifacts or create runtime "
           "evidence\"\n";
    out << child << "}";
  }
  for (std::size_t i = 0; i < projection.field_gap_surface_indices.size();
       ++i) {
    const auto& surface = field_gap_surface_at(projection, i);
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child << "  \"card_id\": "
        << inferlab_plan::json_string(
               "catalog_field_coverage_" + surface.status.surface)
        << ",\n";
    out << child
        << "  \"category\": \"catalog_field_consumption_gap\",\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"severity\": \"blocking\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_fixable\",\n";
    out << child << "  \"missing_catalog_fields\": "
        << inferlab_plan::json_string_array(
               surface.field_coverage.missing_catalog_fields, child_indent)
        << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(
               surface.field_coverage.current_repo_action)
        << ",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_surface_contract.cpp",
                surface.field_coverage.fallback_location},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"field-coverage repair only; it does "
           "not run the consumer or prove downstream parity\"\n";
    out << child << "}";
  }
  if (projection.duplicate_registration_count > 0) {
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child
        << "  \"card_id\": \"catalog_duplicate_registration\",\n";
    out << child
        << "  \"category\": \"catalog_duplicate_registration\",\n";
    out << child << "  \"surface\": \"registry\",\n";
    out << child << "  \"severity\": \"blocking\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_fixable\",\n";
    out << child << "  \"duplicate_registration_count\": "
        << projection.duplicate_registration_count << ",\n";
    out << child
        << "  \"current_repo_action\": \"dedupe duplicate artifact entries "
           "in high_value_artifact_catalog.hpp before trusting registry "
           "coverage\",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp"},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"registry duplicate repair only; it "
           "does not validate producer output\"\n";
    out << child << "}";
  }
  if (projection.missing_wiring_policy_count > 0) {
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child
        << "  \"card_id\": \"catalog_missing_wiring_policy\",\n";
    out << child
        << "  \"category\": \"catalog_missing_wiring_policy\",\n";
    out << child << "  \"surface\": \"registry\",\n";
    out << child << "  \"severity\": \"blocking\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_fixable\",\n";
    out << child << "  \"missing_wiring_policy_count\": "
        << projection.missing_wiring_policy_count << ",\n";
    out << child
        << "  \"current_repo_action\": \"fill artifact, target, report, "
           "refresh command, and required-field metadata in the catalog "
           "entry\",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_views.hpp"},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"wiring policy repair only; downstream "
           "artifacts still need refresh evidence\"\n";
    out << child << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string hardcoded_duplicate_locations_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.fallback_surface_indices.size();
       ++i) {
    const auto& surface = fallback_surface_at(projection, i);
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << child << "  \"migration_status\": "
        << inferlab_plan::json_string(surface.status.consumer_status)
        << ",\n";
    out << child
        << "  \"recommended_action\": \"consume registry-derived entries "
           "before removing fallback\"\n";
    out << child << "}";
    const bool is_last =
        i == projection.fallback_surface_indices.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string consumer_statuses_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.surfaces.size(); ++i) {
    const auto& status = projection.surfaces[i].status;
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(status.surface) << ",\n";
    out << child << "  \"consumer_status\": "
        << inferlab_plan::json_string(status.consumer_status) << ",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(status.source_of_truth) << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(status.fallback_location) << ",\n";
    out << child << "  \"migration_status\": "
        << inferlab_plan::json_string(status.migration_status) << ",\n";
    out << child << "  \"next_action\": "
        << inferlab_plan::json_string(status.next_action) << "\n";
    out << child << "}";
    const bool is_last = i == projection.surfaces.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string surface_migration_readiness_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.surfaces.size(); ++i) {
    const auto& surface = projection.surfaces[i];
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"can_consume_registry\": "
        << inferlab_plan::json_bool(surface.consumes_catalog) << ",\n";
    out << child << "  \"fallback_retained\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"parity_refresh_required\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"readiness_decision\": "
        << inferlab_plan::json_string(surface.readiness_decision)
        << ",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << child << "  \"target_machine_action\": "
        << inferlab_plan::json_string(surface.migration_target_machine_action)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"surface migration readiness is wiring "
           "metadata only; it does not execute refresh commands or prove "
           "downstream artifact parity\"\n";
    out << child << "}";
    const bool is_last = i == projection.surfaces.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string surface_consumer_migration_plan_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.surfaces.size(); ++i) {
    const auto& surface = projection.surfaces[i];
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(surface.status.surface) << ",\n";
    out << child << "  \"source_of_truth\": "
        << inferlab_plan::json_string(surface.status.source_of_truth)
        << ",\n";
    out << child << "  \"fallback_location\": "
        << inferlab_plan::json_string(surface.status.fallback_location)
        << ",\n";
    out << child << "  \"fallback_retained\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"parity_refresh_required\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"fallback_removal_blocked\": "
        << inferlab_plan::json_bool(surface.fallback_retained) << ",\n";
    out << child << "  \"current_repo_action\": "
        << inferlab_plan::json_string(surface.status.next_action) << ",\n";
    out << child << "  \"target_machine_action\": "
        << inferlab_plan::json_string(surface.migration_target_machine_action)
        << ",\n";
    out << child << "  \"ordered_steps\": "
        << inferlab_plan::json_string_array(surface.migration_steps,
                                            child_indent)
        << ",\n";
    out << child << "  \"migration_decision\": "
        << inferlab_plan::json_string(surface.migration_decision)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"surface migration plan is cleanup "
           "guidance only; it does not execute refresh commands, remove "
           "fallback lists, or prove downstream parity\"\n";
    out << child << "}";
    const bool is_last = i == projection.surfaces.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace inferlab_catalog_single_source
