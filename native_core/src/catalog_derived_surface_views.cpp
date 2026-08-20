#include "catalog_derived_views.hpp"

#include "plan_json_utils.hpp"

#include <string>
#include <vector>

namespace inferlab_catalog {

using inferlab_plan::contains;

std::string registry_stale_policy(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (contains(artifact.id, "microbench")) {
    return "microbenchmark timing claims remain blocked until target-machine timing artifacts exist";
  }
  if (contains(artifact.id, "fixture")) {
    return "fixture evidence is synthetic/offline and must be refreshed before offline acceptance demos";
  }
  if (contains(artifact.id, "release")) {
    return "release gate claims must remain pending until release evidence artifacts are refreshed";
  }
  return "catalog-managed evidence claims must remain pending until the registered artifact is refreshed";
}

std::vector<ContractView> contract_views() {
  std::vector<ContractView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back(
        {"catalog_" + artifact.id,
         raw_artifact_path(artifact.artifact),
         "C++ native_core",
         artifact.target,
         artifact.evidence_level,
         artifact.command,
         registry_claim_boundary(artifact),
         registry_requires_execute(artifact),
         registry_requires_hardware(artifact),
         registry_requires_product_replay(artifact),
         registry_required_fields(artifact),
         registry_required_metrics(artifact),
         {}});
  }
  return views;
}

std::vector<FreshnessView> freshness_views() {
  std::vector<FreshnessView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back(
        {artifact.id,
         artifact.artifact,
         artifact.evidence_level,
         artifact.report,
         168,
         registry_requires_execute(artifact),
         registry_requires_hardware(artifact),
         artifact.command,
         registry_freshness_policy(artifact)});
  }
  return views;
}

std::vector<SummarySourceView> summary_source_views() {
  std::vector<SummarySourceView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back({artifact.id,
                     artifact.artifact,
                     artifact.target,
                     artifact.report,
                     registry_summary_group(artifact),
                     artifact.evidence_level,
                     registry_claim_boundary(artifact),
                     registry_requires_execute(artifact),
                     registry_requires_product_replay(artifact)});
  }
  return views;
}

std::vector<PrometheusSourceView> prometheus_source_views() {
  std::vector<PrometheusSourceView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back({artifact.id,
                     artifact.artifact,
                     registry_prometheus_category(artifact),
                     artifact.evidence_level,
                     registry_claim_boundary(artifact),
                     true,
                     true});
  }
  return views;
}

std::vector<FinalReportSourceView> final_report_source_views() {
  std::vector<FinalReportSourceView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back({artifact.id,
                     artifact.artifact,
                     artifact.report,
                     registry_final_report_group(artifact),
                     registry_report_kind(artifact),
                     artifact.evidence_level,
                     registry_claim_boundary(artifact)});
  }
  return views;
}

std::vector<BundleComponentView> bundle_component_views() {
  std::vector<BundleComponentView> views;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    views.push_back(
        {artifact.id,
         artifact.artifact,
         source_path_for(artifact),
         artifact.report,
         artifact.target,
         benchmark_entry_name(artifact),
         artifact.command,
         registry_claim_boundary(artifact)});
  }
  return views;
}

}  // namespace inferlab_catalog
