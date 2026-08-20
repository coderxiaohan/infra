#include "artifact_catalog_single_source_model.hpp"

#include "checked_index.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_catalog_single_source {
namespace {

std::vector<std::string> missing_explicit_policy_fields(
    const inferlab_high_value::ManagedArtifact& spec) {
  std::vector<std::string> missing;
  if (spec.claim_boundary.empty()) missing.push_back("claim_boundary");
  if (spec.freshness_policy.empty()) missing.push_back("freshness_policy");
  if (spec.summary_group.empty()) missing.push_back("summary_group");
  if (spec.final_report_group.empty()) missing.push_back("final_report_group");
  if (spec.prometheus_category.empty()) {
    missing.push_back("prometheus_category");
  }
  if (spec.report_kind.empty()) missing.push_back("report_kind");
  return missing;
}

std::size_t duplicate_registration_count(
    const std::vector<inferlab_high_value::ManagedArtifact>& artifacts) {
  std::set<std::string> seen;
  std::size_t duplicates = 0;
  for (const auto& artifact : artifacts) {
    if (!seen.insert(artifact.artifact).second) {
      duplicates = inferlab::internal::checked_add_size(
          duplicates, 1U, "catalog duplicate registration count");
    }
  }
  return duplicates;
}

bool has_wiring_policy_issue(
    const inferlab_high_value::ManagedArtifact& spec,
    const ArtifactProjection& projection) {
  if (spec.evidence_level.empty() || spec.required_fields.empty() ||
      projection.artifact_type.empty() || projection.report_kind.empty() ||
      projection.summary_group.empty() ||
      projection.final_report_group.empty() ||
      projection.prometheus_category.empty() ||
      projection.freshness_policy.empty() ||
      projection.benchmark_matrix_entry.empty() ||
      projection.claim_boundary.empty() || projection.required_fields.empty()) {
    return true;
  }
  std::set<std::string> seen;
  for (const auto& field : spec.required_fields) {
    if (field.empty() || !seen.insert(field).second) return true;
  }
  return false;
}

ArtifactProjection project_artifact(
    const inferlab_high_value::ManagedArtifact& spec) {
  ArtifactProjection projection;
  projection.spec = &spec;
  projection.artifact_type = inferlab_catalog::registry_artifact_type(spec);
  projection.report_kind = inferlab_catalog::registry_report_kind(spec);
  projection.summary_group = inferlab_catalog::registry_summary_group(spec);
  projection.final_report_group =
      inferlab_catalog::registry_final_report_group(spec);
  projection.prometheus_category =
      inferlab_catalog::registry_prometheus_category(spec);
  projection.freshness_policy =
      inferlab_catalog::registry_freshness_policy(spec);
  projection.benchmark_matrix_entry =
      inferlab_catalog::benchmark_entry_name(spec);
  projection.claim_boundary = inferlab_catalog::registry_claim_boundary(spec);
  projection.required_fields =
      inferlab_catalog::registry_required_fields(spec);
  projection.required_metrics =
      inferlab_catalog::registry_required_metrics(spec);
  projection.missing_explicit_policy_fields =
      missing_explicit_policy_fields(spec);
  projection.requires_execute =
      inferlab_catalog::registry_requires_execute(spec);
  projection.requires_hardware =
      inferlab_catalog::registry_requires_hardware(spec);
  projection.requires_product_replay =
      inferlab_catalog::registry_requires_product_replay(spec);
  return projection;
}

const inferlab_catalog::ConsumerFieldCoverageView& require_coverage_view(
    const std::vector<inferlab_catalog::ConsumerFieldCoverageView>& views,
    const std::string& surface) {
  const inferlab_catalog::ConsumerFieldCoverageView* match = nullptr;
  for (const auto& view : views) {
    if (view.surface != surface) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "duplicate catalog field-coverage surface: " + surface);
    }
    match = &view;
  }
  if (match == nullptr) {
    throw std::logic_error(
        "catalog surface is missing field-coverage projection: " + surface);
  }
  return *match;
}

const SurfaceProjection& indexed_surface(
    const CatalogSingleSourceProjection& projection,
    const std::vector<std::size_t>& indices,
    const std::size_t index,
    const std::string& label) {
  if (index >= indices.size()) {
    throw std::logic_error("catalog " + label + " list index is out of range");
  }
  const std::size_t surface_index = indices[index];
  if (surface_index >= projection.surfaces.size()) {
    throw std::logic_error("catalog " + label + " surface index is out of range");
  }
  return projection.surfaces[surface_index];
}

std::string expected_consumer_status(const bool consumes_catalog,
                                     const bool fallback_retained) {
  if (consumes_catalog) {
    return fallback_retained ? "registry_consumed_with_fallback"
                             : "registry_consumed";
  }
  if (fallback_retained) {
    throw std::logic_error(
        "catalog surface retains fallback without catalog consumption");
  }
  return "registry_not_consumed";
}

void validate_summary_sources(
    const std::vector<inferlab_high_value::ManagedArtifact>& registry,
    const std::vector<ArtifactProjection>& artifacts,
    const std::vector<inferlab_catalog::SummarySourceView>& views) {
  if (registry.size() != views.size()) {
    throw std::logic_error(
        "summary source view count disagrees with managed catalog");
  }
  for (std::size_t i = 0; i < registry.size(); ++i) {
    if (views[i].name != registry[i].id ||
        views[i].artifact != registry[i].artifact ||
        views[i].target != registry[i].target ||
        views[i].report != registry[i].report ||
        views[i].summary_group != artifacts[i].summary_group ||
        views[i].evidence_level != registry[i].evidence_level ||
        views[i].claim_boundary != artifacts[i].claim_boundary ||
        views[i].requires_execute != artifacts[i].requires_execute ||
        views[i].requires_product_replay !=
            artifacts[i].requires_product_replay) {
      throw std::logic_error(
          "summary source view disagrees with managed artifact: " +
          registry[i].id);
    }
  }
}

void validate_prometheus_sources(
    const std::vector<inferlab_high_value::ManagedArtifact>& registry,
    const std::vector<ArtifactProjection>& artifacts,
    const std::vector<inferlab_catalog::PrometheusSourceView>& views) {
  if (registry.size() != views.size()) {
    throw std::logic_error(
        "Prometheus source view count disagrees with managed catalog");
  }
  for (std::size_t i = 0; i < registry.size(); ++i) {
    if (views[i].name != registry[i].id ||
        views[i].artifact != registry[i].artifact ||
        views[i].category != artifacts[i].prometheus_category ||
        views[i].evidence_level != registry[i].evidence_level ||
        views[i].claim_boundary != artifacts[i].claim_boundary ||
        !views[i].export_source_status ||
        !views[i].export_bounded_numeric_metrics) {
      throw std::logic_error(
          "Prometheus source view disagrees with managed artifact: " +
          registry[i].id);
    }
  }
}

void validate_final_report_sources(
    const std::vector<inferlab_high_value::ManagedArtifact>& registry,
    const std::vector<ArtifactProjection>& artifacts,
    const std::vector<inferlab_catalog::FinalReportSourceView>& views) {
  if (registry.size() != views.size()) {
    throw std::logic_error(
        "final-report source view count disagrees with managed catalog");
  }
  for (std::size_t i = 0; i < registry.size(); ++i) {
    if (views[i].name != registry[i].id ||
        views[i].artifact != registry[i].artifact ||
        views[i].report != registry[i].report ||
        views[i].final_report_group != artifacts[i].final_report_group ||
        views[i].report_kind != artifacts[i].report_kind ||
        views[i].evidence_level != registry[i].evidence_level ||
        views[i].claim_boundary != artifacts[i].claim_boundary) {
      throw std::logic_error(
          "final-report source view disagrees with managed artifact: " +
          registry[i].id);
    }
  }
}

}  // namespace

const inferlab_high_value::ManagedArtifact& artifact_spec(
    const ArtifactProjection& artifact) {
  if (artifact.spec == nullptr) {
    throw std::logic_error("catalog artifact projection has no registry owner");
  }
  return *artifact.spec;
}

const SurfaceProjection* first_fallback_surface(
    const CatalogSingleSourceProjection& projection) {
  if (projection.fallback_surface_indices.empty()) return nullptr;
  return &fallback_surface_at(projection, 0U);
}

const SurfaceProjection& fallback_surface_at(
    const CatalogSingleSourceProjection& projection,
    const std::size_t fallback_index) {
  return indexed_surface(projection, projection.fallback_surface_indices,
                         fallback_index, "fallback");
}

const SurfaceProjection& unconsumed_surface_at(
    const CatalogSingleSourceProjection& projection,
    const std::size_t unconsumed_index) {
  return indexed_surface(projection, projection.unconsumed_surface_indices,
                         unconsumed_index, "unconsumed");
}

const SurfaceProjection& field_gap_surface_at(
    const CatalogSingleSourceProjection& projection,
    const std::size_t field_gap_index) {
  return indexed_surface(projection, projection.field_gap_surface_indices,
                         field_gap_index, "field-gap");
}

const SurfaceProjection& require_surface(
    const CatalogSingleSourceProjection& projection,
    const std::string& surface) {
  const SurfaceProjection* match = nullptr;
  for (const auto& candidate : projection.surfaces) {
    if (candidate.status.surface != surface) continue;
    if (match != nullptr) {
      throw std::logic_error("duplicate projected catalog surface: " +
                             surface);
    }
    match = &candidate;
  }
  if (match == nullptr) {
    throw std::logic_error("missing projected catalog surface: " + surface);
  }
  return *match;
}

std::string expected_surface_entry(const ArtifactProjection& artifact,
                                   const SurfaceProjection& surface) {
  const auto& spec = artifact_spec(artifact);
  const std::string& name = surface.status.surface;
  if (name == "summary" || name == "prometheus" ||
      name == "final_report") {
    return spec.artifact;
  }
  if (name == "artifact_contract" || name == "freshness_gate") {
    return spec.target + " -> " + spec.artifact;
  }
  if (name == "evidence_bundle") return "catalog_" + spec.id;
  throw std::logic_error("unknown catalog surface in wiring matrix: " + name);
}

CatalogSingleSourceProjection build_catalog_single_source_projection() {
  inferlab_high_value::validate_managed_artifact_registry();

  CatalogSingleSourceProjection projection;
  const auto& registry = inferlab_high_value::managed_artifacts();
  if (registry.empty()) {
    throw std::logic_error("managed artifact registry is empty");
  }
  projection.allowed_modes = inferlab_catalog::registry_allowed_modes();
  if (projection.allowed_modes.empty()) {
    throw std::logic_error("catalog allowed-mode registry is empty");
  }

  projection.artifacts.reserve(registry.size());
  for (const auto& spec : registry) {
    auto artifact = project_artifact(spec);
    if (has_wiring_policy_issue(spec, artifact)) {
      projection.missing_wiring_policy_count = inferlab::internal::checked_add_size(
          projection.missing_wiring_policy_count, 1U,
          "catalog missing wiring policy count");
    }
    if (artifact.missing_explicit_policy_fields.empty()) {
      projection.explicit_policy_complete_artifact_count =
          inferlab::internal::checked_add_size(
              projection.explicit_policy_complete_artifact_count, 1U,
              "catalog explicit policy complete artifact count");
    } else {
      projection.derived_policy_artifact_count = inferlab::internal::checked_add_size(
          projection.derived_policy_artifact_count, 1U,
          "catalog derived policy artifact count");
      projection.derived_policy_field_count = inferlab::internal::checked_add_size(
          projection.derived_policy_field_count,
          artifact.missing_explicit_policy_fields.size(),
          "catalog derived policy field count");
    }
    projection.artifacts.push_back(std::move(artifact));
  }

  projection.duplicate_registration_count =
      duplicate_registration_count(registry);

  const auto statuses = inferlab_catalog::surface_consumer_statuses();
  const auto coverage_views =
      inferlab_catalog::consumer_field_coverage_views();
  if (statuses.empty()) {
    throw std::logic_error("catalog consumer surface registry is empty");
  }
  if (statuses.size() != coverage_views.size()) {
    throw std::logic_error(
        "catalog surface and field-coverage registries have different sizes");
  }
  projection.surfaces.reserve(statuses.size());
  for (const auto& status : statuses) {
    const auto& coverage = require_coverage_view(coverage_views, status.surface);
    if (coverage.source_of_truth != status.source_of_truth ||
        coverage.fallback_location != status.fallback_location ||
        coverage.current_repo_action != status.next_action) {
      throw std::logic_error(
          "catalog surface status disagrees with field-coverage metadata: " +
          status.surface);
    }
    if (status.consumer_status !=
        expected_consumer_status(coverage.consumes_catalog,
                                 coverage.fallback_retained)) {
      throw std::logic_error(
          "catalog surface status disagrees with field-coverage contract: " +
          status.surface);
    }
    SurfaceProjection surface;
    surface.status = status;
    surface.field_coverage = coverage;
    surface.consumes_catalog = coverage.consumes_catalog;
    surface.fallback_retained = coverage.fallback_retained;
    surface.drift_status =
        surface.fallback_retained
            ? "registry_consumed_with_fallback_pending_parity_refresh"
            : surface.consumes_catalog ? "registry_consumed"
                                       : "registry_consumption_missing";
    surface.readiness_decision =
        surface.consumes_catalog && surface.fallback_retained
            ? "registry_consumed_with_fallback_pending_parity_refresh"
        : surface.consumes_catalog ? "registry_only_ready"
                                   : "registry_consumption_missing";
    surface.migration_decision =
        surface.fallback_retained
            ? "fallback_removal_blocked_until_parity_refresh"
            : surface.consumes_catalog ? "registry_only_refresh_ready"
                                       : "registry_consumption_missing";
    surface.migration_target_machine_action =
        surface.fallback_retained
            ? "refresh this surface, compare registry-derived output against the retained fallback list, and record parity before fallback removal"
            : "refresh this surface when validation is re-enabled to keep catalog-derived output current";
    if (!surface.consumes_catalog) {
      surface.migration_steps = {
          "implement registry-derived consumption for the surface",
          "confirm all required catalog fields are projected",
          "refresh the downstream surface when validation is enabled",
          "record catalog-consumption evidence before readiness review"};
    } else if (surface.fallback_retained) {
      surface.migration_steps = {
          "confirm registry-derived entries are emitted for the surface",
          "refresh the downstream surface when validation is enabled",
          "compare registry-derived output with retained fallback output",
          "remove fallback only after refreshed parity evidence exists"};
    } else {
      surface.migration_steps = {
          "confirm registry-derived entries remain the only source",
          "refresh the downstream surface when validation is enabled",
          "record the catalog-only refresh state"};
    }
    const std::size_t surface_index = projection.surfaces.size();
    if (surface.consumes_catalog) {
      projection.consumed_surface_count = inferlab::internal::checked_add_size(
          projection.consumed_surface_count, 1U,
          "catalog consumed surface count");
      projection.field_coverage_catalog_managed_surface_count =
          inferlab::internal::checked_add_size(
              projection.field_coverage_catalog_managed_surface_count, 1U,
              "catalog field coverage managed surface count");
    } else {
      projection.unconsumed_surface_indices.push_back(surface_index);
      projection.missing_surface_count = inferlab::internal::checked_add_size(
          projection.missing_surface_count, 1U,
          "catalog missing surface count");
    }
    if (surface.fallback_retained) {
      projection.fallback_surface_indices.push_back(surface_index);
      projection.fallback_surface_count = inferlab::internal::checked_add_size(
          projection.fallback_surface_count, 1U,
          "catalog fallback surface count");
      projection.field_coverage_fallback_surface_count =
          inferlab::internal::checked_add_size(
              projection.field_coverage_fallback_surface_count, 1U,
              "catalog field coverage fallback surface count");
      if (surface.status.fallback_location.empty()) {
        projection.fallback_source_location_gap_count =
            inferlab::internal::checked_add_size(
                projection.fallback_source_location_gap_count, 1U,
                "catalog fallback source location gap count");
      }
    }
    if (!coverage.missing_catalog_fields.empty()) {
      projection.field_gap_surface_indices.push_back(surface_index);
      projection.field_coverage_gap_surface_count =
          inferlab::internal::checked_add_size(
              projection.field_coverage_gap_surface_count, 1U,
              "catalog field coverage gap surface count");
    }
    if (surface.consumes_catalog && coverage.missing_catalog_fields.empty()) {
      projection.migration_ready_surface_count = inferlab::internal::checked_add_size(
          projection.migration_ready_surface_count, 1U,
          "catalog migration-ready surface count");
    }
    if (surface.fallback_retained || !surface.consumes_catalog ||
        !coverage.missing_catalog_fields.empty()) {
      projection.surface_current_repo_action_count =
          inferlab::internal::checked_add_size(
              projection.surface_current_repo_action_count, 1U,
              "catalog current-repo action surface count");
    }
    if (surface.fallback_retained) {
      projection.surface_target_machine_action_count =
          inferlab::internal::checked_add_size(
              projection.surface_target_machine_action_count, 1U,
              "catalog target-machine action surface count");
    }
    if (surface.fallback_retained ||
        !coverage.missing_catalog_fields.empty()) {
      projection.field_coverage_current_repo_action_count =
          inferlab::internal::checked_add_size(
              projection.field_coverage_current_repo_action_count, 1U,
              "catalog field-coverage current-repo action count");
    }
    if (surface.fallback_retained) {
      projection.field_coverage_target_machine_action_count =
          inferlab::internal::checked_add_size(
              projection.field_coverage_target_machine_action_count, 1U,
              "catalog field-coverage target-machine action count");
    }
    projection.field_coverage_required_field_count =
        inferlab::internal::checked_add_size(
            projection.field_coverage_required_field_count,
            coverage.required_catalog_fields.size(),
            "catalog field coverage required field count");
    projection.field_coverage_covered_field_count =
        inferlab::internal::checked_add_size(
            projection.field_coverage_covered_field_count,
            coverage.covered_catalog_fields.size(),
            "catalog field coverage covered field count");
    projection.field_coverage_field_gap_count = inferlab::internal::checked_add_size(
        projection.field_coverage_field_gap_count,
        coverage.missing_catalog_fields.size(),
        "catalog field coverage gap count");
    projection.surfaces.push_back(std::move(surface));
  }
  if (inferlab::internal::checked_add_size(
          projection.consumed_surface_count,
          projection.missing_surface_count,
          "catalog classified surface count") != projection.surfaces.size()) {
    throw std::logic_error(
        "catalog surface consumption counts do not cover every surface");
  }
  if (projection.fallback_surface_count !=
          projection.fallback_surface_indices.size() ||
      projection.missing_surface_count !=
          projection.unconsumed_surface_indices.size() ||
      projection.field_coverage_gap_surface_count !=
          projection.field_gap_surface_indices.size()) {
    throw std::logic_error(
        "catalog surface index lists disagree with projected counts");
  }

  projection.summary_sources = inferlab_catalog::summary_source_views();
  projection.prometheus_sources =
      inferlab_catalog::prometheus_source_views();
  projection.final_report_sources =
      inferlab_catalog::final_report_source_views();
  validate_summary_sources(registry, projection.artifacts,
                           projection.summary_sources);
  validate_prometheus_sources(registry, projection.artifacts,
                              projection.prometheus_sources);
  validate_final_report_sources(registry, projection.artifacts,
                                projection.final_report_sources);

  projection.matrix_entry_count = inferlab::internal::checked_product_size(
      projection.artifacts.size(), projection.surfaces.size(),
      "catalog downstream wiring matrix entry count");
  projection.fallback_matrix_entry_count = inferlab::internal::checked_product_size(
      projection.artifacts.size(), projection.fallback_surface_count,
      "catalog fallback matrix entry count");
  projection.single_source_repair_card_count =
      projection.fallback_surface_count;
  projection.single_source_repair_card_count = inferlab::internal::checked_add_size(
      projection.single_source_repair_card_count,
      projection.missing_surface_count,
      "catalog single-source repair card count");
  projection.single_source_repair_card_count = inferlab::internal::checked_add_size(
      projection.single_source_repair_card_count,
      projection.field_coverage_gap_surface_count,
      "catalog single-source repair card count");
  if (projection.duplicate_registration_count > 0) {
    projection.single_source_repair_card_count = inferlab::internal::checked_add_size(
        projection.single_source_repair_card_count, 1U,
        "catalog single-source repair card count");
  }
  if (projection.missing_wiring_policy_count > 0) {
    projection.single_source_repair_card_count = inferlab::internal::checked_add_size(
        projection.single_source_repair_card_count, 1U,
        "catalog single-source repair card count");
  }
  projection.single_source_current_repo_action_count =
      projection.single_source_repair_card_count;
  projection.single_source_target_machine_action_count =
      projection.fallback_surface_count;

  projection.fallback_removal_blocked =
      projection.fallback_surface_count > 0;
  projection.fallback_shortest_path_current_repo_step_count =
      projection.fallback_removal_blocked ? 2U : 0U;
  projection.fallback_shortest_path_target_machine_step_count =
      projection.fallback_removal_blocked ? 2U : 0U;
  projection.fallback_shortest_path_step_count = inferlab::internal::checked_add_size(
      projection.fallback_shortest_path_current_repo_step_count,
      projection.fallback_shortest_path_target_machine_step_count,
      "catalog fallback shortest path step count");

  const bool all_surfaces_consumed =
      projection.consumed_surface_count == projection.surfaces.size();
  const bool static_catalog_gap =
      projection.duplicate_registration_count > 0 ||
      projection.missing_wiring_policy_count > 0 ||
      projection.missing_surface_count > 0 ||
      projection.field_coverage_field_gap_count > 0;
  projection.registry_decision =
      !all_surfaces_consumed
          ? "catalog_surface_consumption_missing"
      : projection.fallback_removal_blocked
          ? "all_catalog_surfaces_consuming_registry_with_fallback"
          : "all_catalog_surfaces_consuming_registry";
  projection.drift_decision =
      static_catalog_gap
          ? "static_catalog_gap_detected"
          : "no_static_catalog_gap_detected";
  projection.drift_next_action =
      static_catalog_gap
          ? "repair current-repository catalog registration, surface-consumption, and field-coverage gaps before downstream parity review"
          : "keep canonical non-catalog extension inventories separate from catalog ownership and refresh consumers during normal validation";
  projection.single_source_parity_decision =
      static_catalog_gap
          ? "static_catalog_gap_detected"
      : projection.fallback_removal_blocked
          ? "registry_consumed_with_fallback_pending_parity_refresh"
          : "registry_only_ready";
  projection.single_source_next_action =
      static_catalog_gap
          ? "use single_source_repair_cards to close current-repository catalog gaps before downstream parity refresh"
      : projection.fallback_removal_blocked
          ? "use single_source_repair_cards to dedupe fallback surfaces after refreshed downstream artifacts prove parity"
          : "no catalog repair card remains; refresh catalog-derived consumers during normal validation";
  projection.metadata_decision =
      projection.derived_policy_artifact_count == 0
          ? "explicit_registry_policy_complete"
          : "catalog_derived_policy_in_use";
  projection.field_coverage_decision =
      projection.field_coverage_field_gap_count == 0
          ? (projection.fallback_removal_blocked
                 ? "catalog_fields_covered_with_fallback_pending_parity_refresh"
                 : "catalog_fields_covered")
          : "catalog_field_consumption_gaps_present";
  projection.migration_readiness_decision =
      static_catalog_gap ? "catalog_registry_blocked"
                         : projection.registry_decision;
  projection.fallback_queue_state =
      projection.fallback_removal_blocked
          ? "blocked_until_downstream_parity_refresh"
          : "empty";
  projection.fallback_shortest_path_state =
      projection.fallback_removal_blocked
          ? "blocked_until_downstream_parity_refresh"
          : "no_catalog_fallback_retained";
  projection.fallback_parity_gate_decision =
      projection.fallback_removal_blocked
          ? "blocked_until_downstream_parity_refresh"
          : "no_catalog_fallback_retained";
  if (projection.duplicate_registration_count > 0) {
    projection.blockers.push_back("catalog_duplicate_registration");
  }
  if (projection.missing_wiring_policy_count > 0) {
    projection.blockers.push_back("catalog_missing_wiring_policy");
  }
  if (!all_surfaces_consumed) {
    projection.blockers.push_back("catalog_surface_consumption_missing");
  }
  if (projection.field_coverage_field_gap_count > 0) {
    projection.blockers.push_back("catalog_field_consumption_gaps_present");
  }
  projection.artifact_status = projection.blockers.empty()
                                   ? "catalog_registry_ready"
                                   : "catalog_registry_blocked";
  projection.evidence_confidence = projection.blockers.empty()
                                       ? "schema_only"
                                       : "stale_or_blocked";
  return projection;
}

}  // namespace inferlab_catalog_single_source
