#include "artifact_catalog_single_source_views.hpp"

#include "artifact_cli_path_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace inferlab_catalog_single_source {
namespace {

std::string policy_source(const bool explicit_field) {
  return explicit_field ? "explicit_registry" : "catalog_derived_default";
}

std::string metadata_policy_json(const ArtifactProjection& artifact,
                                 const int indent) {
  const auto& spec = artifact_spec(artifact);
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "{\n";
  out << child << "\"missing_explicit_policy_fields\": "
      << inferlab_plan::json_string_array(
             artifact.missing_explicit_policy_fields, child_indent)
      << ",\n";
  out << child << "\"policy_source_decision\": "
      << inferlab_plan::json_string(
             artifact.missing_explicit_policy_fields.empty()
                 ? "explicit_registry_policy_complete"
                 : "catalog_derived_policy_in_use")
      << ",\n";
  out << child << "\"claim_boundary_source\": "
      << inferlab_plan::json_string(policy_source(!spec.claim_boundary.empty()))
      << ",\n";
  out << child << "\"freshness_policy_source\": "
      << inferlab_plan::json_string(
             policy_source(!spec.freshness_policy.empty()))
      << ",\n";
  out << child << "\"summary_group_source\": "
      << inferlab_plan::json_string(policy_source(!spec.summary_group.empty()))
      << ",\n";
  out << child << "\"final_report_group_source\": "
      << inferlab_plan::json_string(
             policy_source(!spec.final_report_group.empty()))
      << ",\n";
  out << child << "\"prometheus_category_source\": "
      << inferlab_plan::json_string(
             policy_source(!spec.prometheus_category.empty()))
      << ",\n";
  out << child << "\"report_kind_source\": "
      << inferlab_plan::json_string(policy_source(!spec.report_kind.empty()))
      << ",\n";
  out << child << "\"execution_requirement_source\": "
      << inferlab_plan::json_string(
             (spec.requires_execute || spec.requires_hardware ||
              spec.requires_product_replay)
                 ? "explicit_registry_bool_or_derived_overlay"
                 : "default_false_with_catalog_derived_heuristics")
      << ",\n";
  out << child
      << "\"claim_boundary\": \"metadata policy source review only; "
         "derived defaults do not prove downstream artifact refresh or "
         "benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

std::string artifact_json(const ArtifactProjection& artifact,
                          const std::vector<std::string>& allowed_modes,
                          const int indent) {
  const auto& spec = artifact_spec(artifact);
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << child << "\"id\": " << inferlab_plan::json_string(spec.id)
      << ",\n";
  out << child << "\"artifact\": "
      << inferlab_plan::json_string(spec.artifact) << ",\n";
  out << child << "\"artifact_type\": "
      << inferlab_plan::json_string(artifact.artifact_type) << ",\n";
  out << child << "\"producer_target\": "
      << inferlab_plan::json_string(spec.target) << ",\n";
  out << child << "\"report\": " << inferlab_plan::json_string(spec.report)
      << ",\n";
  out << child << "\"report_kind\": "
      << inferlab_plan::json_string(artifact.report_kind) << ",\n";
  out << child << "\"summary_group\": "
      << inferlab_plan::json_string(artifact.summary_group) << ",\n";
  out << child << "\"final_report_group\": "
      << inferlab_plan::json_string(artifact.final_report_group) << ",\n";
  out << child << "\"prometheus_category\": "
      << inferlab_plan::json_string(artifact.prometheus_category) << ",\n";
  out << child << "\"freshness_policy\": "
      << inferlab_plan::json_string(artifact.freshness_policy) << ",\n";
  out << child << "\"benchmark_matrix_entry\": "
      << inferlab_plan::json_string(artifact.benchmark_matrix_entry) << ",\n";
  out << child << "\"refresh_command\": "
      << inferlab_plan::json_string(spec.command) << ",\n";
  out << child << "\"required_fields\": "
      << inferlab_plan::json_string_array(artifact.required_fields,
                                          child_indent)
      << ",\n";
  out << child << "\"optional_fields\": "
      << inferlab_plan::json_string_array(
             {"environment", "official_sources", "refresh_commands",
              "migration_commands", "not_run", "claim_scope_detail",
              "recommended_next_commands"},
             child_indent)
      << ",\n";
  out << child << "\"summary_metrics\": "
      << inferlab_plan::json_string_array(artifact.required_metrics,
                                          child_indent)
      << ",\n";
  out << child << "\"summary_required\": true,\n";
  out << child << "\"prometheus_source_required\": true,\n";
  out << child << "\"final_report_required\": true,\n";
  out << child << "\"contract_required\": true,\n";
  out << child << "\"freshness_required\": true,\n";
  out << child << "\"evidence_bundle_required\": true,\n";
  out << child << "\"allowed_modes\": "
      << inferlab_plan::json_string_array(allowed_modes, child_indent)
      << ",\n";
  out << child << "\"claim_boundary\": "
      << inferlab_plan::json_string(artifact.claim_boundary) << ",\n";
  out << child << "\"metadata_policy\": "
      << metadata_policy_json(artifact, child_indent) << ",\n";
  out << child
      << "\"external_product_boundary\": \"mature product features stay "
         "product-owned; InferLab owns adapters, schemas, evidence, local "
         "mechanism slices, and explain/repair surfaces\",\n";
  out << child << "\"requires_execute\": "
      << inferlab_plan::json_bool(artifact.requires_execute) << ",\n";
  out << child << "\"requires_hardware\": "
      << inferlab_plan::json_bool(artifact.requires_hardware) << ",\n";
  out << child << "\"requires_product_replay\": "
      << inferlab_plan::json_bool(artifact.requires_product_replay) << ",\n";
  out << child << "\"target_machine_required\": "
      << inferlab_plan::json_bool(artifact.requires_execute ||
                                  artifact.requires_hardware ||
                                  artifact.requires_product_replay)
      << ",\n";
  out << child << "\"validation_order\": "
      << inferlab_plan::json_string_array(
             {"CMake configure/build", "native artifact refresh",
              "schema/contract/freshness gate", "evidence query/release gate",
              "summary/Prometheus/final report",
              "GPU/product replay or execute when making runtime claims"},
             child_indent)
      << "\n";
  out << pad << "}";
  return out.str();
}

template <typename View, typename Writer>
std::string source_views_json(const std::vector<View>& views,
                              const int indent,
                              Writer writer) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < views.size(); ++i) {
    writer(out, views[i], child_indent);
    const bool is_last = i == views.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace

std::string cli_context_json(const std::filesystem::path& repo_root,
                             const std::filesystem::path& output,
                             const std::string& format,
                             const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << child << "\"format\": " << inferlab_plan::json_string(format)
      << ",\n";
  out << child << "\"strict_cli_validation\": true,\n";
  out << child << "\"unsupported_positionals_rejected\": true,\n";
  out << child << "\"unsupported_format_rejected\": true,\n";
  out << child << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, child_indent) << ",\n";
  out << inferlab_artifact::cli_path_field_json("repo_root", repo_root,
                                                child_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json("output", output,
                                                child_indent)
      << ",\n";
  out << child
      << "\"claim_boundary\": \"CLI context documents accepted "
         "catalog-registry inputs only; it does not refresh artifacts, remove "
         "fallback lists, or prove downstream parity\"\n";
  out << pad << "}";
  return out.str();
}

std::string artifacts_json(const CatalogSingleSourceProjection& projection,
                           const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.artifacts.size(); ++i) {
    out << artifact_json(projection.artifacts[i], projection.allowed_modes,
                         child_indent);
    const bool is_last = i == projection.artifacts.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string registry_sources_json(const int indent) {
  return inferlab_plan::json_string_array(
      {"native_core/src/high_value_artifact_catalog.hpp", "configs/schemas/",
       "configs/benchmark_matrix.yaml", "scripts/summarize_benchmarks.py",
       "scripts/export_prometheus_metrics.py",
       "scripts/render_final_report.py",
       "native_core/src/artifact_contract_model.cpp",
       "native_core/src/runtime_evidence_freshness_gate_model.cpp",
       "native_core/src/evidence_query_native_trace_projection.cpp"},
      indent);
}

std::string derived_entries_json(
    const CatalogSingleSourceProjection& projection,
    const std::string& surface,
    const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < projection.artifacts.size(); ++i) {
    const auto& artifact = projection.artifacts[i];
    const auto& spec = artifact_spec(artifact);
    out << child << "{\n";
    out << child << "  \"surface\": " << inferlab_plan::json_string(surface)
        << ",\n";
    out << child << "  \"artifact\": "
        << inferlab_plan::json_string(spec.artifact) << ",\n";
    out << child << "  \"target\": "
        << inferlab_plan::json_string(spec.target) << ",\n";
    out << child << "  \"report\": "
        << inferlab_plan::json_string(spec.report) << ",\n";
    out << child << "  \"refresh_command\": "
        << inferlab_plan::json_string(spec.command) << ",\n";
    out << child << "  \"summary_group\": "
        << inferlab_plan::json_string(artifact.summary_group) << ",\n";
    out << child << "  \"final_report_group\": "
        << inferlab_plan::json_string(artifact.final_report_group) << ",\n";
    out << child << "  \"prometheus_category\": "
        << inferlab_plan::json_string(artifact.prometheus_category) << ",\n";
    out << child << "  \"report_kind\": "
        << inferlab_plan::json_string(artifact.report_kind) << ",\n";
    out << child << "  \"freshness_policy\": "
        << inferlab_plan::json_string(artifact.freshness_policy) << ",\n";
    out << child << "  \"claim_boundary\": "
        << inferlab_plan::json_string(artifact.claim_boundary) << ",\n";
    out << child << "  \"requires_execute\": "
        << inferlab_plan::json_bool(artifact.requires_execute) << ",\n";
    out << child << "  \"requires_hardware\": "
        << inferlab_plan::json_bool(artifact.requires_hardware) << ",\n";
    out << child << "  \"requires_product_replay\": "
        << inferlab_plan::json_bool(artifact.requires_product_replay) << ",\n";
    out << child
        << "  \"source_of_truth\": \"high_value_artifact_catalog.hpp\"\n";
    out << child << "}";
    const bool is_last = i == projection.artifacts.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string summary_source_views_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  return source_views_json(
      projection.summary_sources, indent,
      [](std::ostringstream& out,
         const inferlab_catalog::SummarySourceView& view,
         const int child_indent) {
        const std::string child = inferlab_plan::json_indent(child_indent);
        out << child << "{\n";
        out << child << "  \"surface\": \"summary\",\n";
        out << child << "  \"id\": " << inferlab_plan::json_string(view.name)
            << ",\n";
        out << child << "  \"artifact\": "
            << inferlab_plan::json_string(view.artifact) << ",\n";
        out << child << "  \"target\": "
            << inferlab_plan::json_string(view.target) << ",\n";
        out << child << "  \"report\": "
            << inferlab_plan::json_string(view.report) << ",\n";
        out << child << "  \"summary_group\": "
            << inferlab_plan::json_string(view.summary_group) << ",\n";
        out << child << "  \"evidence_level\": "
            << inferlab_plan::json_string(view.evidence_level) << ",\n";
        out << child << "  \"claim_boundary\": "
            << inferlab_plan::json_string(view.claim_boundary) << ",\n";
        out << child << "  \"requires_execute\": "
            << inferlab_plan::json_bool(view.requires_execute) << ",\n";
        out << child << "  \"requires_product_replay\": "
            << inferlab_plan::json_bool(view.requires_product_replay) << ",\n";
        out << child
            << "  \"source_of_truth\": \"catalog_derived_views.summary_source_views\"\n";
        out << child << "}";
      });
}

std::string prometheus_source_views_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  return source_views_json(
      projection.prometheus_sources, indent,
      [](std::ostringstream& out,
         const inferlab_catalog::PrometheusSourceView& view,
         const int child_indent) {
        const std::string child = inferlab_plan::json_indent(child_indent);
        out << child << "{\n";
        out << child << "  \"surface\": \"prometheus\",\n";
        out << child << "  \"id\": " << inferlab_plan::json_string(view.name)
            << ",\n";
        out << child << "  \"artifact\": "
            << inferlab_plan::json_string(view.artifact) << ",\n";
        out << child << "  \"category\": "
            << inferlab_plan::json_string(view.category) << ",\n";
        out << child << "  \"evidence_level\": "
            << inferlab_plan::json_string(view.evidence_level) << ",\n";
        out << child << "  \"claim_boundary\": "
            << inferlab_plan::json_string(view.claim_boundary) << ",\n";
        out << child << "  \"export_source_status\": "
            << inferlab_plan::json_bool(view.export_source_status) << ",\n";
        out << child << "  \"export_bounded_numeric_metrics\": "
            << inferlab_plan::json_bool(view.export_bounded_numeric_metrics)
            << ",\n";
        out << child
            << "  \"source_of_truth\": \"catalog_derived_views.prometheus_source_views\"\n";
        out << child << "}";
      });
}

std::string final_report_source_views_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  return source_views_json(
      projection.final_report_sources, indent,
      [](std::ostringstream& out,
         const inferlab_catalog::FinalReportSourceView& view,
         const int child_indent) {
        const std::string child = inferlab_plan::json_indent(child_indent);
        out << child << "{\n";
        out << child << "  \"surface\": \"final_report\",\n";
        out << child << "  \"id\": " << inferlab_plan::json_string(view.name)
            << ",\n";
        out << child << "  \"artifact\": "
            << inferlab_plan::json_string(view.artifact) << ",\n";
        out << child << "  \"report\": "
            << inferlab_plan::json_string(view.report) << ",\n";
        out << child << "  \"final_report_group\": "
            << inferlab_plan::json_string(view.final_report_group) << ",\n";
        out << child << "  \"report_kind\": "
            << inferlab_plan::json_string(view.report_kind) << ",\n";
        out << child << "  \"evidence_level\": "
            << inferlab_plan::json_string(view.evidence_level) << ",\n";
        out << child << "  \"claim_boundary\": "
            << inferlab_plan::json_string(view.claim_boundary) << ",\n";
        out << child
            << "  \"source_of_truth\": \"catalog_derived_views.final_report_source_views\"\n";
        out << child << "}";
      });
}

std::string metadata_completeness_review_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"artifact_count\": " << projection.artifacts.size()
      << ",\n";
  out << pad << "  \"explicit_policy_complete_artifact_count\": "
      << projection.explicit_policy_complete_artifact_count << ",\n";
  out << pad << "  \"derived_policy_artifact_count\": "
      << projection.derived_policy_artifact_count << ",\n";
  out << pad << "  \"derived_policy_field_count\": "
      << projection.derived_policy_field_count << ",\n";
  out << pad << "  \"metadata_policy_card_count\": "
      << projection.derived_policy_artifact_count << ",\n";
  out << pad << "  \"policy_fields\": "
      << inferlab_plan::json_string_array(
             {"claim_boundary", "freshness_policy", "summary_group",
              "final_report_group", "prometheus_category", "report_kind"},
             catalog_view_indent(indent))
      << ",\n";
  out << pad << "  \"metadata_decision\": "
      << inferlab_plan::json_string(projection.metadata_decision) << ",\n";
  out << pad
      << "  \"next_action\": \"promote high-value artifact metadata from "
         "catalog-derived defaults into explicit ManagedArtifact fields in "
         "small batches, then refresh downstream contract/freshness/report "
         "artifacts when validation is enabled\",\n";
  out << pad
      << "  \"claim_boundary\": \"metadata completeness review explains "
         "registry policy source quality only; it does not execute refresh "
         "commands or prove benchmark/replay/execute readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string metadata_policy_cards_json(
    const CatalogSingleSourceProjection& projection, const int indent) {
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[";
  bool first = true;
  for (const auto& artifact : projection.artifacts) {
    if (artifact.missing_explicit_policy_fields.empty()) continue;
    const auto& spec = artifact_spec(artifact);
    if (!first) out << ",";
    first = false;
    out << "\n" << child << "{\n";
    out << child << "  \"card_id\": "
        << inferlab_plan::json_string("catalog_metadata_policy_" + spec.id)
        << ",\n";
    out << child << "  \"category\": \"catalog_metadata_policy_gap\",\n";
    out << child << "  \"artifact\": "
        << inferlab_plan::json_string(spec.artifact) << ",\n";
    out << child << "  \"target\": "
        << inferlab_plan::json_string(spec.target) << ",\n";
    out << child << "  \"severity\": \"info\",\n";
    out << child
        << "  \"repair_environment\": \"current_repo_fixable\",\n";
    out << child << "  \"missing_explicit_policy_fields\": "
        << inferlab_plan::json_string_array(
               artifact.missing_explicit_policy_fields, child_indent)
        << ",\n";
    out << child << "  \"derived_policy_field_count\": "
        << artifact.missing_explicit_policy_fields.size() << ",\n";
    out << child
        << "  \"current_repo_action\": \"fill explicit metadata fields on "
           "this ManagedArtifact entry when promoting it from catalog-derived "
           "defaults\",\n";
    out << child << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(
               {"native_core/src/high_value_artifact_catalog.hpp",
                "native_core/src/catalog_derived_views.hpp"},
               child_indent)
        << ",\n";
    out << child
        << "  \"claim_boundary\": \"metadata repair card improves catalog "
           "maintainability only; it does not run producers or validate "
           "runtime evidence\"\n";
    out << child << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string wiring_policies_json(const int indent) {
  const std::vector<std::vector<std::string>> policies = {
      {"schema_contract",
       "configs/schemas/ and artifact contract gate must discover required fields from the registry"},
      {"freshness_gate",
       "runtime freshness gate must track the same artifacts and refresh commands"},
      {"benchmark_matrix",
       "benchmark matrix keeps refresh commands only; current server does not execute them"},
      {"summary",
       "benchmark summary loads every registered artifact as an evidence_governance section"},
      {"prometheus",
       "Prometheus exporter exposes source presence and bounded numeric metadata only"},
      {"final_report",
       "final report lists evidence confidence and preserves dry-run/replay/execute boundaries"},
  };
  const int child_indent = catalog_view_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < policies.size(); ++i) {
    out << child << "{\n";
    out << child << "  \"surface\": "
        << inferlab_plan::json_string(policies[i][0]) << ",\n";
    out << child << "  \"policy\": "
        << inferlab_plan::json_string(policies[i][1]) << "\n";
    out << child << "}";
    const bool is_last = i == policies.size() - 1U;
    out << (is_last ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace inferlab_catalog_single_source
