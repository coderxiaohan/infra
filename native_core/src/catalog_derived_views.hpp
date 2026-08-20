#pragma once

#include "high_value_artifact_catalog.hpp"

#include <string>
#include <vector>

namespace inferlab_catalog {

struct ContractView {
  std::string name;
  std::string artifact_path;
  std::string producer;
  std::string producer_target;
  std::string evidence_level;
  std::string refresh_command;
  std::string claim_boundary;
  bool requires_execute = false;
  bool requires_hardware = false;
  bool requires_product_replay = false;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
  std::vector<std::string> blockers;
};

struct FreshnessView {
  std::string name;
  std::string artifact;
  std::string evidence_class;
  std::string consumer;
  int max_age_hours = 168;
  bool requires_execute = false;
  bool requires_hardware = false;
  std::string refresh_command;
  std::string stale_policy;
};

struct SummarySourceView {
  std::string name;
  std::string artifact;
  std::string target;
  std::string report;
  std::string summary_group;
  std::string evidence_level;
  std::string claim_boundary;
  bool requires_execute = false;
  bool requires_product_replay = false;
};

struct PrometheusSourceView {
  std::string name;
  std::string artifact;
  std::string category;
  std::string evidence_level;
  std::string claim_boundary;
  bool export_source_status = true;
  bool export_bounded_numeric_metrics = true;
};

struct FinalReportSourceView {
  std::string name;
  std::string artifact;
  std::string report;
  std::string final_report_group;
  std::string report_kind;
  std::string evidence_level;
  std::string claim_boundary;
};

struct SurfaceConsumerStatus {
  std::string surface;
  std::string consumer_status;
  std::string source_of_truth;
  std::string fallback_location;
  std::string migration_status;
  std::string next_action;
};

struct ConsumerFieldCoverageView {
  std::string surface;
  std::string consumer_kind;
  std::string catalog_view;
  std::string source_of_truth;
  std::string fallback_location;
  bool consumes_catalog = false;
  bool fallback_retained = false;
  std::string field_coverage_status;
  std::vector<std::string> required_catalog_fields;
  std::vector<std::string> covered_catalog_fields;
  std::vector<std::string> missing_catalog_fields;
  std::string current_repo_action;
  std::string target_machine_action;
  std::string claim_boundary;
};

struct BundleComponentView {
  std::string name;
  std::string artifact;
  std::string source_path;
  std::string report_path;
  std::string cmake_target;
  std::string matrix_entry;
  std::string command;
  std::string claim_boundary;
};

std::string raw_artifact_path(const std::string& artifact);
std::string strip_inferlab_prefix(const std::string& target);
std::string benchmark_entry_name(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_artifact_type(
    const inferlab_high_value::ManagedArtifact& artifact);
const std::vector<std::string>& registry_allowed_modes();
std::string source_path_for(
    const inferlab_high_value::ManagedArtifact& artifact);
bool registry_requires_hardware(
    const inferlab_high_value::ManagedArtifact& artifact);
bool registry_requires_execute(
    const inferlab_high_value::ManagedArtifact& artifact);
bool registry_requires_product_replay(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_claim_boundary(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_stale_policy(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_freshness_policy(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_summary_group(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_final_report_group(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_prometheus_category(
    const inferlab_high_value::ManagedArtifact& artifact);
std::string registry_report_kind(
    const inferlab_high_value::ManagedArtifact& artifact);
std::vector<std::string> registry_required_fields(
    const inferlab_high_value::ManagedArtifact& artifact);
std::vector<std::string> registry_required_metrics(
    const inferlab_high_value::ManagedArtifact& artifact);

std::vector<ContractView> contract_views();
std::vector<FreshnessView> freshness_views();
std::vector<SummarySourceView> summary_source_views();
std::vector<PrometheusSourceView> prometheus_source_views();
std::vector<FinalReportSourceView> final_report_source_views();
std::vector<BundleComponentView> bundle_component_views();
std::vector<SurfaceConsumerStatus> surface_consumer_statuses();
std::vector<std::string> consumer_required_fields(
    const std::string& surface);
std::string consumer_catalog_view(const std::string& surface);
std::string consumer_kind_for_surface(const std::string& surface);
std::vector<ConsumerFieldCoverageView> consumer_field_coverage_views();

}  // namespace inferlab_catalog
