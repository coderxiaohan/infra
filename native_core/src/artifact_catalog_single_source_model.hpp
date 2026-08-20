#pragma once

#include "catalog_derived_views.hpp"
#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_catalog_single_source {

struct ArtifactProjection {
  const inferlab_high_value::ManagedArtifact* spec = nullptr;
  std::string artifact_type;
  std::string report_kind;
  std::string summary_group;
  std::string final_report_group;
  std::string prometheus_category;
  std::string freshness_policy;
  std::string benchmark_matrix_entry;
  std::string claim_boundary;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
  std::vector<std::string> missing_explicit_policy_fields;
  bool requires_execute = false;
  bool requires_hardware = false;
  bool requires_product_replay = false;
};

struct SurfaceProjection {
  inferlab_catalog::SurfaceConsumerStatus status;
  inferlab_catalog::ConsumerFieldCoverageView field_coverage;
  bool consumes_catalog = false;
  bool fallback_retained = false;
  std::string drift_status;
  std::string readiness_decision;
  std::string migration_decision;
  std::string migration_target_machine_action;
  std::vector<std::string> migration_steps;
};

struct CatalogSingleSourceProjection {
  std::vector<ArtifactProjection> artifacts;
  std::vector<SurfaceProjection> surfaces;
  std::vector<std::size_t> fallback_surface_indices;
  std::vector<std::size_t> unconsumed_surface_indices;
  std::vector<std::size_t> field_gap_surface_indices;
  std::vector<std::string> allowed_modes;
  std::vector<inferlab_catalog::SummarySourceView> summary_sources;
  std::vector<inferlab_catalog::PrometheusSourceView> prometheus_sources;
  std::vector<inferlab_catalog::FinalReportSourceView> final_report_sources;

  std::size_t duplicate_registration_count = 0;
  std::size_t missing_wiring_policy_count = 0;
  std::size_t consumed_surface_count = 0;
  std::size_t migration_ready_surface_count = 0;
  std::size_t missing_surface_count = 0;
  std::size_t fallback_surface_count = 0;
  std::size_t surface_current_repo_action_count = 0;
  std::size_t surface_target_machine_action_count = 0;
  std::size_t matrix_entry_count = 0;
  std::size_t fallback_matrix_entry_count = 0;
  std::size_t explicit_policy_complete_artifact_count = 0;
  std::size_t derived_policy_artifact_count = 0;
  std::size_t derived_policy_field_count = 0;
  std::size_t single_source_repair_card_count = 0;
  std::size_t single_source_current_repo_action_count = 0;
  std::size_t single_source_target_machine_action_count = 0;
  std::size_t field_coverage_required_field_count = 0;
  std::size_t field_coverage_covered_field_count = 0;
  std::size_t field_coverage_field_gap_count = 0;
  std::size_t field_coverage_gap_surface_count = 0;
  std::size_t field_coverage_catalog_managed_surface_count = 0;
  std::size_t field_coverage_fallback_surface_count = 0;
  std::size_t field_coverage_current_repo_action_count = 0;
  std::size_t field_coverage_target_machine_action_count = 0;
  std::size_t fallback_source_location_gap_count = 0;
  std::size_t fallback_shortest_path_current_repo_step_count = 0;
  std::size_t fallback_shortest_path_target_machine_step_count = 0;
  std::size_t fallback_shortest_path_step_count = 0;

  std::string registry_decision;
  std::string drift_decision;
  std::string drift_next_action;
  std::string single_source_parity_decision;
  std::string single_source_next_action;
  std::string metadata_decision;
  std::string field_coverage_decision;
  std::string migration_readiness_decision;
  std::string fallback_queue_state;
  std::string fallback_shortest_path_state;
  std::string fallback_parity_gate_decision;
  std::string artifact_status;
  std::string evidence_confidence;
  std::vector<std::string> blockers;
  bool fallback_removal_blocked = false;
};

CatalogSingleSourceProjection build_catalog_single_source_projection();

const inferlab_high_value::ManagedArtifact& artifact_spec(
    const ArtifactProjection& artifact);

const SurfaceProjection* first_fallback_surface(
    const CatalogSingleSourceProjection& projection);

const SurfaceProjection& fallback_surface_at(
    const CatalogSingleSourceProjection& projection,
    std::size_t fallback_index);

const SurfaceProjection& unconsumed_surface_at(
    const CatalogSingleSourceProjection& projection,
    std::size_t unconsumed_index);

const SurfaceProjection& field_gap_surface_at(
    const CatalogSingleSourceProjection& projection,
    std::size_t field_gap_index);

const SurfaceProjection& require_surface(
    const CatalogSingleSourceProjection& projection,
    const std::string& surface);

std::string expected_surface_entry(const ArtifactProjection& artifact,
                                   const SurfaceProjection& surface);

}  // namespace inferlab_catalog_single_source
