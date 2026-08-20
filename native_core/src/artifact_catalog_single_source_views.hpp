#pragma once

#include "artifact_catalog_single_source_model.hpp"
#include "checked_index.hpp"

#include <filesystem>
#include <string>

namespace inferlab_catalog_single_source {

inline int catalog_view_indent(const int indent, const int delta = 2) {
  return inferlab::internal::checked_add_int(
      indent, delta, "catalog single-source view indentation");
}

std::string cli_context_json(const std::filesystem::path& repo_root,
                             const std::filesystem::path& output,
                             const std::string& format,
                             int indent);
std::string artifacts_json(const CatalogSingleSourceProjection& projection,
                           int indent);
std::string registry_sources_json(int indent);
std::string derived_entries_json(
    const CatalogSingleSourceProjection& projection,
    const std::string& surface,
    int indent);
std::string summary_source_views_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string prometheus_source_views_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string final_report_source_views_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string metadata_completeness_review_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string metadata_policy_cards_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string wiring_policies_json(int indent);

std::string downstream_wiring_matrix_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string catalog_drift_review_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string single_source_parity_review_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string single_source_repair_cards_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string hardcoded_duplicate_locations_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string consumer_statuses_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string surface_migration_readiness_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string surface_consumer_migration_plan_json(
    const CatalogSingleSourceProjection& projection, int indent);

std::string fallback_removal_queue_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string fallback_removal_shortest_path_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string fallback_removal_parity_gate_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string consumer_field_coverage_summary_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string consumer_field_coverage_json(
    const CatalogSingleSourceProjection& projection, int indent);
std::string migration_readiness_json(
    const CatalogSingleSourceProjection& projection, int indent);

std::string render_catalog_single_source_artifact(
    const CatalogSingleSourceProjection& projection,
    const std::string& output,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& output_path,
    const std::string& format);

}  // namespace inferlab_catalog_single_source
