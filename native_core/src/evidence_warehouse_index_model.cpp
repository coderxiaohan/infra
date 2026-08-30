#include "evidence_warehouse_index_model.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "repository_wiring_text.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace inferlab_evidence_warehouse {
namespace {

namespace fs = std::filesystem;

constexpr char kWarehouseArtifactId[] = "evidence_warehouse_index";
constexpr char kEvidenceQueryArtifactId[] = "evidence_query_result";

std::size_t add_count(const std::size_t current, const std::size_t increment,
                      const char* name) {
  return inferlab::internal::checked_add_size(current, increment, name);
}

std::string safe_path_status(const fs::path& path) {
  if (path.empty()) return "not_configured";
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error) return "path_probe_error";
  if (!fs::exists(status)) return "missing";
  if (fs::is_symlink(status)) return "symlink";
  if (fs::is_regular_file(status)) return "file";
  if (fs::is_directory(status)) return "directory";
  return "other";
}

const inferlab_artifact::TextSource& source_by_id(
    const std::vector<inferlab_artifact::TextSource>& sources,
    const std::string& id) {
  return inferlab_artifact::require_unique_text_source_by_id(sources, id);
}

bool source_ready(
    const std::vector<inferlab_artifact::TextSource>& sources,
    const std::string& id) {
  const auto& source = source_by_id(sources, id);
  return source.present && source.readable && source.status == "ok" &&
         safe_path_status(source.path) == "file";
}

const inferlab_catalog::ConsumerFieldCoverageView& require_surface(
    const std::vector<inferlab_catalog::ConsumerFieldCoverageView>& views,
    const std::string& surface) {
  const inferlab_catalog::ConsumerFieldCoverageView* found = nullptr;
  for (const auto& view : views) {
    if (view.surface != surface) continue;
    if (found != nullptr) {
      throw std::logic_error("duplicate catalog consumer surface: " +
                             surface);
    }
    found = &view;
  }
  if (found == nullptr) {
    throw std::logic_error("missing catalog consumer surface: " + surface);
  }
  return *found;
}

bool surface_ready(
    const std::vector<inferlab_catalog::ConsumerFieldCoverageView>& views,
    const std::string& surface) {
  const auto& view = require_surface(views, surface);
  return view.consumes_catalog && view.missing_catalog_fields.empty();
}

template <typename View, typename Match>
const View* unique_matching_view(const std::vector<View>& views,
                                 Match matches,
                                 const std::string& description) {
  const View* found = nullptr;
  for (const auto& view : views) {
    if (!matches(view)) continue;
    if (found != nullptr) {
      throw std::logic_error("duplicate " + description);
    }
    found = &view;
  }
  return found;
}

const inferlab_high_value::DependencySpec& require_dependency(
    const std::string& name) {
  inferlab_high_value::validate_dependency_spec_registry(
      inferlab_high_value::dependency_specs());
  const inferlab_high_value::DependencySpec* found = nullptr;
  for (const auto& dependency : inferlab_high_value::dependency_specs()) {
    if (dependency.name != name) continue;
    if (found != nullptr) {
      throw std::logic_error("duplicate dependency specification: " + name);
    }
    found = &dependency;
  }
  if (found == nullptr) {
    throw std::logic_error("missing dependency specification: " + name);
  }
  return *found;
}

void add_edge(ManagedArtifactLineage& record, const std::string& relation,
              const std::string& target, const std::string& authority,
              const bool present, const std::string& gap_category,
              const std::string& repair_action) {
  if (record.spec == nullptr || relation.empty() || target.empty() ||
      authority.empty() || gap_category.empty() || repair_action.empty()) {
    throw std::logic_error("warehouse lineage edge contains an empty field");
  }
  for (const auto& edge : record.edges) {
    if (edge.relation == relation) {
      throw std::logic_error("duplicate warehouse lineage relation for " +
                             record.spec->artifact + ": " + relation);
    }
  }
  record.edges.push_back({record.spec->artifact, relation, target, authority,
                          present, gap_category, repair_action});
  record.required_edge_count = add_count(
      record.required_edge_count, 1U,
      "warehouse managed artifact required edge count");
  if (present) {
    record.ready_edge_count = add_count(
        record.ready_edge_count, 1U,
        "warehouse managed artifact ready edge count");
  }
}

inferlab_artifact::Metadata deferred_output_metadata(
    const inferlab_high_value::ManagedArtifact& spec) {
  inferlab_artifact::Metadata metadata;
  metadata.source = spec.artifact;
  metadata.present = false;
  metadata.mode = "current_output_deferred";
  metadata.warnings.push_back(
      "current warehouse output is deferred and cannot prove its own input state");
  return metadata;
}

bool report_path_ready(const fs::path& path) {
  return safe_path_status(path) == "file";
}

RawArtifactScan scan_raw_artifacts(
    const fs::path& results_root, const fs::path& output_path,
    const std::string& canonical_output_artifact,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  RawArtifactScan scan;
  scan.directory_status = safe_path_status(results_root);
  if (scan.directory_status != "directory") {
    scan.issues.push_back(
        {"results_raw", "raw_scan_directory_" + scan.directory_status,
         "raw artifact directory is not a readable non-symlink directory"});
    return scan;
  }

  const fs::path normalized_output = output_path.empty()
                                         ? fs::path{}
                                         : output_path.lexically_normal();
  std::error_code iterator_error;
  fs::directory_iterator iterator(
      results_root, fs::directory_options::skip_permission_denied,
      iterator_error);
  const fs::directory_iterator end;
  if (iterator_error) {
    scan.issues.push_back(
        {"results_raw", "raw_scan_open_failed",
         "raw artifact directory iteration could not be opened"});
    return scan;
  }

  while (iterator != end) {
    const fs::path path = iterator->path();
    const std::string filename = path.filename().string();
    if (path.extension() != ".json") {
      scan.skipped_non_json_count = add_count(
          scan.skipped_non_json_count, 1U,
          "warehouse skipped non-JSON entry count");
    } else if (filename == canonical_output_artifact ||
               (!normalized_output.empty() &&
                path.lexically_normal() == normalized_output)) {
      scan.skipped_current_output_count = add_count(
          scan.skipped_current_output_count, 1U,
          "warehouse skipped current-output entry count");
    } else {
      const std::string status = safe_path_status(path);
      if (status != "file") {
        inferlab_artifact::Metadata metadata;
        metadata.source = filename;
        metadata.present = false;
        metadata.warnings.push_back("raw artifact path rejected: " + status);
        scan.artifacts.push_back(std::move(metadata));
        scan.issues.push_back(
            {filename, "raw_artifact_" + status,
             "raw JSON entry is not a safe regular file"});
      } else {
        const auto& snapshot = snapshots.get(path);
        scan.artifacts.push_back(snapshot.metadata);
        if (!snapshot.read_result.readable) {
          scan.issues.push_back(
              {filename, "raw_artifact_unreadable",
               "raw JSON entry exists but could not be read"});
        }
      }
    }

    iterator.increment(iterator_error);
    if (iterator_error) {
      scan.issues.push_back(
          {"results_raw", "raw_scan_iteration_failed",
           "raw artifact directory iteration stopped before completion"});
      break;
    }
  }

  std::sort(scan.artifacts.begin(), scan.artifacts.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.source < rhs.source;
            });
  std::sort(scan.issues.begin(), scan.issues.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.artifact != rhs.artifact) {
                return lhs.artifact < rhs.artifact;
              }
              return lhs.category < rhs.category;
            });
  return scan;
}

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  if (value.empty()) {
    throw std::logic_error("warehouse command must not be empty");
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void validate_summary(const WarehousePlan& plan) {
  WarehouseSummary expected;
  expected.artifact_count = plan.raw_scan.artifacts.size();
  expected.managed_artifact_count = plan.lineage.records.size();
  expected.source_file_count = plan.lineage.sources.size();
  expected.source_read_issue_count = plan.lineage.source_read_issue_count;
  expected.raw_scan_issue_count = plan.raw_scan.issues.size();
  expected.physical_artifact_read_count =
      plan.summary.physical_artifact_read_count;
  expected.ready_edge_count = plan.lineage.ready_edge_count;
  expected.required_edge_count = plan.lineage.required_edge_count;
  expected.blocker_count = plan.blockers.size();

  for (const auto& record : plan.lineage.records) {
    expected.indexed_artifact_count = add_count(
        expected.indexed_artifact_count,
        record.warehouse_record.present ? 1U : 0U,
        "warehouse indexed artifact validation count");
    expected.parse_ready_artifact_count = add_count(
        expected.parse_ready_artifact_count,
        record.warehouse_record.metadata.parse_ok ? 1U : 0U,
        "warehouse parse-ready artifact validation count");
    expected.structured_parser_artifact_count = add_count(
        expected.structured_parser_artifact_count,
        record.warehouse_record.metadata.used_structured_parser ? 1U : 0U,
        "warehouse structured-parser artifact validation count");
    expected.current_output_deferred_count = add_count(
        expected.current_output_deferred_count,
        record.current_output_deferred ? 1U : 0U,
        "warehouse deferred-output validation count");
    expected.missing_target_count = add_count(
        expected.missing_target_count,
        record.producer_target_present ? 0U : 1U,
        "warehouse missing target validation count");
    expected.missing_report_count = add_count(
        expected.missing_report_count,
        record.warehouse_record.report_present ? 0U : 1U,
        "warehouse missing report validation count");
    expected.missing_matrix_count = add_count(
        expected.missing_matrix_count,
        record.warehouse_record.matrix_present ? 0U : 1U,
        "warehouse missing matrix validation count");
    expected.missing_summary_count = add_count(
        expected.missing_summary_count,
        record.warehouse_record.summary_present ? 0U : 1U,
        "warehouse missing summary validation count");
    expected.missing_prometheus_count = add_count(
        expected.missing_prometheus_count,
        record.warehouse_record.prometheus_present ? 0U : 1U,
        "warehouse missing Prometheus validation count");
    expected.missing_final_report_count = add_count(
        expected.missing_final_report_count,
        record.warehouse_record.final_report_present ? 0U : 1U,
        "warehouse missing final-report validation count");
    expected.missing_contract_count = add_count(
        expected.missing_contract_count,
        record.warehouse_record.contract_present ? 0U : 1U,
        "warehouse missing contract validation count");
    expected.missing_freshness_count = add_count(
        expected.missing_freshness_count,
        record.warehouse_record.freshness_present ? 0U : 1U,
        "warehouse missing freshness validation count");
    expected.missing_bundle_count = add_count(
        expected.missing_bundle_count,
        record.evidence_bundle_present ? 0U : 1U,
        "warehouse missing bundle validation count");
  }

  const auto& actual = plan.summary;
  if (expected.ready_edge_count > expected.required_edge_count) {
    throw std::logic_error(
        "warehouse ready edge count exceeds required edge count");
  }
  const double expected_lineage_score =
      expected.required_edge_count == 0U
          ? 0.0
          : static_cast<double>(expected.ready_edge_count) /
                static_cast<double>(expected.required_edge_count);
  if (expected.current_output_deferred_count >
      expected.managed_artifact_count) {
    throw std::logic_error(
        "warehouse validation deferred-output count exceeds inventory");
  }
  const std::size_t inventory_obligations =
      expected.managed_artifact_count -
      expected.current_output_deferred_count;
  const InventoryState expected_inventory_state =
      expected.indexed_artifact_count == 0U
          ? InventoryState::kEmpty
      : expected.indexed_artifact_count == inventory_obligations
          ? InventoryState::kComplete
          : InventoryState::kPartial;
  if (expected.artifact_count != actual.artifact_count ||
      expected.managed_artifact_count != actual.managed_artifact_count ||
      expected.indexed_artifact_count != actual.indexed_artifact_count ||
      expected.parse_ready_artifact_count !=
          actual.parse_ready_artifact_count ||
      expected.structured_parser_artifact_count !=
          actual.structured_parser_artifact_count ||
      expected.current_output_deferred_count !=
          actual.current_output_deferred_count ||
      expected.missing_target_count != actual.missing_target_count ||
      expected.missing_report_count != actual.missing_report_count ||
      expected.missing_matrix_count != actual.missing_matrix_count ||
      expected.missing_summary_count != actual.missing_summary_count ||
      expected.missing_prometheus_count !=
          actual.missing_prometheus_count ||
      expected.missing_final_report_count !=
          actual.missing_final_report_count ||
      expected.missing_contract_count != actual.missing_contract_count ||
      expected.missing_freshness_count != actual.missing_freshness_count ||
      expected.missing_bundle_count != actual.missing_bundle_count ||
      expected.source_file_count != actual.source_file_count ||
      expected.source_read_issue_count != actual.source_read_issue_count ||
      expected.raw_scan_issue_count != actual.raw_scan_issue_count ||
      expected.ready_edge_count != actual.ready_edge_count ||
      expected.required_edge_count != actual.required_edge_count ||
      expected.blocker_count != actual.blocker_count ||
      expected_lineage_score != actual.lineage_score ||
      expected_inventory_state != actual.inventory_state) {
    throw std::logic_error("warehouse summary does not reconcile with records");
  }
  if ((actual.decision == WarehouseDecision::kLineageReady) !=
      plan.blockers.empty()) {
    throw std::logic_error("warehouse decision does not reconcile with blockers");
  }
}

}  // namespace

const char* warehouse_decision_name(const WarehouseDecision decision) {
  switch (decision) {
    case WarehouseDecision::kLineageReady:
      return "lineage_ready";
    case WarehouseDecision::kBlocked:
      return "blocked";
  }
  return "blocked";
}

const char* inventory_state_name(const InventoryState state) {
  switch (state) {
    case InventoryState::kEmpty:
      return "empty";
    case InventoryState::kPartial:
      return "partial";
    case InventoryState::kComplete:
      return "complete";
  }
  return "empty";
}

std::vector<inferlab_artifact::TextSource> load_lineage_sources(
    const fs::path& repo_root) {
  using inferlab_artifact::read_text_source;
  return {
      read_text_source("cmake", repo_root / "CMakeLists.txt"),
      read_text_source("benchmark_matrix",
                       repo_root / "configs" / "benchmark_matrix.yaml"),
      read_text_source("summary_consumer",
                       repo_root / "scripts" / "summarize_benchmarks.py"),
      read_text_source(
          "prometheus_consumer",
          repo_root / "scripts" / "export_prometheus_metrics.py"),
      read_text_source("final_report_consumer",
                       repo_root / "scripts" / "summarize_benchmarks.py"),
      read_text_source(
          "catalog_surface_views",
          repo_root / "native_core" / "src" /
              "catalog_derived_surface_views.cpp"),
      read_text_source(
          "catalog_surface_contract",
          repo_root / "native_core" / "src" /
              "catalog_derived_surface_contract.cpp"),
  };
}

ManagedLineageProjection project_managed_artifact_lineage(
    const fs::path& repo_root, const fs::path& results_root,
    const std::vector<inferlab_artifact::TextSource>& sources,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots,
    const std::string& current_output_artifact_id) {
  inferlab_high_value::validate_managed_artifact_registry();
  const auto contract_views = inferlab_catalog::contract_views();
  const auto freshness_views = inferlab_catalog::freshness_views();
  const auto summary_views = inferlab_catalog::summary_source_views();
  const auto prometheus_views = inferlab_catalog::prometheus_source_views();
  const auto final_report_views =
      inferlab_catalog::final_report_source_views();
  const auto bundle_views = inferlab_catalog::bundle_component_views();
  const auto surface_views =
      inferlab_catalog::consumer_field_coverage_views();

  const auto& cmake_source = source_by_id(sources, "cmake");
  const auto& matrix_source = source_by_id(sources, "benchmark_matrix");
  const bool summary_source_ready = source_ready(sources, "summary_consumer");
  const bool prometheus_source_ready =
      source_ready(sources, "prometheus_consumer");
  const bool final_report_source_ready =
      source_ready(sources, "final_report_consumer");
  const bool catalog_source_ready =
      source_ready(sources, "catalog_surface_views") &&
      source_ready(sources, "catalog_surface_contract");

  ManagedLineageProjection projection;
  projection.sources.reserve(sources.size());
  for (const auto& source : sources) {
    projection.sources.push_back(source);
    projection.sources.back().text.clear();
  }
  projection.source_read_issue_count =
      inferlab_artifact::source_read_issue_count(projection.sources);
  projection.records.reserve(inferlab_high_value::managed_artifacts().size());

  for (const auto& spec : inferlab_high_value::managed_artifacts()) {
    ManagedArtifactLineage record;
    record.spec = &spec;
    record.current_output_deferred =
        !current_output_artifact_id.empty() &&
        spec.id == current_output_artifact_id;
    record.warehouse_record.artifact = spec.artifact;

    if (record.current_output_deferred) {
      record.artifact_path_status = "current_output_deferred";
      record.warehouse_record.metadata = deferred_output_metadata(spec);
      record.warehouse_record.present = false;
    } else {
      const fs::path artifact_path = results_root / spec.artifact;
      record.artifact_path_status = safe_path_status(artifact_path);
      if (record.artifact_path_status == "file") {
        const auto& snapshot = snapshots.get(artifact_path);
        record.warehouse_record.metadata = snapshot.metadata;
        record.warehouse_record.present = true;
        record.warehouse_record.metadata.present = true;
      } else {
        record.warehouse_record.metadata.source = spec.artifact;
        record.warehouse_record.metadata.present = false;
        record.warehouse_record.present = false;
        record.warehouse_record.metadata.warnings.push_back(
            "managed artifact path status: " + record.artifact_path_status);
      }
    }

    const auto cmake_match =
        inferlab_repository_wiring::cmake_target_source_match(
            cmake_source.text, spec.target,
            inferlab_catalog::source_path_for(spec));
    record.producer_target_present =
        source_ready(sources, "cmake") && cmake_match.registered();

    const auto matrix_match =
        inferlab_repository_wiring::benchmark_matrix_entry_match(
            matrix_source.text, inferlab_catalog::benchmark_entry_name(spec),
            spec.target, spec.artifact, spec.command);
    record.warehouse_record.matrix_present =
        source_ready(sources, "benchmark_matrix") &&
        matrix_match.registered();
    record.warehouse_record.report_present =
        report_path_ready(repo_root / spec.report);

    const std::string expected_claim =
        inferlab_catalog::registry_claim_boundary(spec);
    const auto* contract = unique_matching_view(
        contract_views,
        [&](const auto& view) {
          return view.name == "catalog_" + spec.id &&
                 view.artifact_path ==
                     inferlab_catalog::raw_artifact_path(spec.artifact) &&
                 view.producer_target == spec.target &&
                 view.refresh_command == spec.command &&
                 view.claim_boundary == expected_claim;
        },
        "catalog contract view for " + spec.id);
    record.warehouse_record.contract_present =
        catalog_source_ready && surface_ready(surface_views, "artifact_contract") &&
        contract != nullptr;

    const auto* freshness = unique_matching_view(
        freshness_views,
        [&](const auto& view) {
          return view.name == spec.id &&
                 view.artifact == spec.artifact &&
                 view.refresh_command == spec.command;
        },
        "catalog freshness view for " + spec.id);
    record.warehouse_record.freshness_present =
        catalog_source_ready && surface_ready(surface_views, "freshness_gate") &&
        freshness != nullptr;

    const auto* summary = unique_matching_view(
        summary_views,
        [&](const auto& view) {
          return view.name == spec.id && view.artifact == spec.artifact &&
                 view.target == spec.target && view.report == spec.report &&
                 view.claim_boundary == expected_claim;
        },
        "catalog summary view for " + spec.id);
    record.warehouse_record.summary_present =
        summary_source_ready && surface_ready(surface_views, "summary") &&
        summary != nullptr;

    const auto* prometheus = unique_matching_view(
        prometheus_views,
        [&](const auto& view) {
          return view.name == spec.id && view.artifact == spec.artifact &&
                 view.claim_boundary == expected_claim;
        },
        "catalog Prometheus view for " + spec.id);
    record.warehouse_record.prometheus_present =
        prometheus_source_ready && surface_ready(surface_views, "prometheus") &&
        prometheus != nullptr;

    const auto* final_report = unique_matching_view(
        final_report_views,
        [&](const auto& view) {
          return view.name == spec.id && view.artifact == spec.artifact &&
                 view.report == spec.report &&
                 view.claim_boundary == expected_claim;
        },
        "catalog final-report view for " + spec.id);
    record.warehouse_record.final_report_present =
        final_report_source_ready &&
        surface_ready(surface_views, "final_report") &&
        final_report != nullptr;

    const auto* bundle = unique_matching_view(
        bundle_views,
        [&](const auto& view) {
          return view.name == spec.id && view.artifact == spec.artifact &&
                 view.report_path == spec.report &&
                 view.cmake_target == spec.target &&
                 view.matrix_entry ==
                     inferlab_catalog::benchmark_entry_name(spec) &&
                 view.command == spec.command &&
                 view.claim_boundary == expected_claim;
        },
        "catalog bundle view for " + spec.id);
    record.evidence_bundle_present =
        catalog_source_ready && surface_ready(surface_views, "evidence_bundle") &&
        bundle != nullptr;

    add_edge(record, "produced_by", spec.target, "CMake target declaration",
             record.producer_target_present, "producer_target_wiring_missing",
             "register exactly one producer target in CMakeLists.txt");
    add_edge(record, "benchmark_matrix", "configs/benchmark_matrix.yaml",
             "scoped benchmark-matrix entry",
             record.warehouse_record.matrix_present,
             "benchmark_matrix_wiring_missing",
             "repair the exact catalog-derived benchmark-matrix entry");
    add_edge(record, "summary", "scripts/summarize_benchmarks.py",
             "catalog summary_source_views",
             record.warehouse_record.summary_present,
             "summary_wiring_missing",
             "restore catalog-derived Summary consumption and source readability");
    add_edge(record, "prometheus",
             "scripts/export_prometheus_metrics.py",
             "catalog prometheus_source_views",
             record.warehouse_record.prometheus_present,
             "prometheus_wiring_missing",
             "restore catalog-derived Prometheus consumption and source readability");
    add_edge(record, "report", spec.report, "repository report file",
             record.warehouse_record.report_present,
             "report_wiring_missing",
             "restore the catalog-owned report as a safe regular file");
    add_edge(record, "structured_summary", "scripts/summarize_benchmarks.py",
             "catalog structured-summary source views",
             record.warehouse_record.final_report_present,
             "final_report_wiring_missing",
             "restore catalog-derived Final Report consumption and source readability");
    add_edge(record, "artifact_contract", "catalog contract_views",
             "catalog-derived native contract surface",
             record.warehouse_record.contract_present,
             "artifact_contract_wiring_missing",
             "repair the catalog-derived artifact contract surface");
    add_edge(record, "freshness_gate", "catalog freshness_views",
             "catalog-derived native freshness surface",
             record.warehouse_record.freshness_present,
             "freshness_wiring_missing",
             "repair the catalog-derived freshness-gate surface");
    add_edge(record, "evidence_bundle", "catalog bundle_component_views",
             "catalog-derived evidence-bundle surface",
             record.evidence_bundle_present,
             "evidence_bundle_wiring_missing",
             "repair the catalog-derived evidence-bundle surface");

    projection.ready_edge_count = add_count(
        projection.ready_edge_count, record.ready_edge_count,
        "warehouse projection ready edge count");
    projection.required_edge_count = add_count(
        projection.required_edge_count, record.required_edge_count,
        "warehouse projection required edge count");
    projection.records.push_back(std::move(record));
  }
  return projection;
}

std::vector<inferlab_artifact::WarehouseArtifactRecord>
copy_warehouse_records(const ManagedLineageProjection& projection) {
  std::vector<inferlab_artifact::WarehouseArtifactRecord> records;
  records.reserve(projection.records.size());
  for (const auto& record : projection.records) {
    records.push_back(record.warehouse_record);
  }
  return records;
}

WarehousePlan build_warehouse_plan(const fs::path& repo_root,
                                   const fs::path& results_root,
                                   const fs::path& output_path) {
  WarehousePlan plan;
  plan.spec = &inferlab_high_value::require_managed_artifact(
      kWarehouseArtifactId);
  plan.json_dependency = &require_dependency("nlohmann/json");
  plan.sqlite_dependency = &require_dependency("SQLite3");

  inferlab_evidence::EvidenceSnapshotRepository snapshots;
  plan.raw_scan = scan_raw_artifacts(
      results_root, output_path, plan.spec->artifact, snapshots);
  const auto sources = load_lineage_sources(repo_root);
  plan.lineage = project_managed_artifact_lineage(
      repo_root, results_root, sources, snapshots, plan.spec->id);

  auto& summary = plan.summary;
  summary.artifact_count = plan.raw_scan.artifacts.size();
  summary.managed_artifact_count = plan.lineage.records.size();
  summary.source_file_count = plan.lineage.sources.size();
  summary.source_read_issue_count = plan.lineage.source_read_issue_count;
  summary.raw_scan_issue_count = plan.raw_scan.issues.size();
  summary.physical_artifact_read_count = snapshots.size();
  summary.ready_edge_count = plan.lineage.ready_edge_count;
  summary.required_edge_count = plan.lineage.required_edge_count;

  for (const auto& record : plan.lineage.records) {
    const auto& warehouse = record.warehouse_record;
    summary.indexed_artifact_count = add_count(
        summary.indexed_artifact_count, warehouse.present ? 1U : 0U,
        "warehouse indexed artifact count");
    summary.parse_ready_artifact_count = add_count(
        summary.parse_ready_artifact_count,
        warehouse.metadata.parse_ok ? 1U : 0U,
        "warehouse parse-ready artifact count");
    summary.structured_parser_artifact_count = add_count(
        summary.structured_parser_artifact_count,
        warehouse.metadata.used_structured_parser ? 1U : 0U,
        "warehouse structured-parser artifact count");
    summary.current_output_deferred_count = add_count(
        summary.current_output_deferred_count,
        record.current_output_deferred ? 1U : 0U,
        "warehouse current-output deferred count");
    summary.missing_target_count = add_count(
        summary.missing_target_count,
        record.producer_target_present ? 0U : 1U,
        "warehouse missing target count");
    summary.missing_report_count = add_count(
        summary.missing_report_count,
        warehouse.report_present ? 0U : 1U,
        "warehouse missing report count");
    summary.missing_matrix_count = add_count(
        summary.missing_matrix_count,
        warehouse.matrix_present ? 0U : 1U,
        "warehouse missing matrix count");
    summary.missing_summary_count = add_count(
        summary.missing_summary_count,
        warehouse.summary_present ? 0U : 1U,
        "warehouse missing summary count");
    summary.missing_prometheus_count = add_count(
        summary.missing_prometheus_count,
        warehouse.prometheus_present ? 0U : 1U,
        "warehouse missing Prometheus count");
    summary.missing_final_report_count = add_count(
        summary.missing_final_report_count,
        warehouse.final_report_present ? 0U : 1U,
        "warehouse missing final-report count");
    summary.missing_contract_count = add_count(
        summary.missing_contract_count,
        warehouse.contract_present ? 0U : 1U,
        "warehouse missing contract count");
    summary.missing_freshness_count = add_count(
        summary.missing_freshness_count,
        warehouse.freshness_present ? 0U : 1U,
        "warehouse missing freshness count");
    summary.missing_bundle_count = add_count(
        summary.missing_bundle_count,
        record.evidence_bundle_present ? 0U : 1U,
        "warehouse missing bundle count");

    if (!record.current_output_deferred &&
        (!warehouse.present || !warehouse.metadata.parse_ok)) {
      append_unique(plan.pending_artifact_refresh_commands,
                    record.spec->command);
    }
    for (const auto& edge : record.edges) {
      if (edge.present) continue;
      plan.blockers.push_back(
          {record.spec->artifact,
           edge.gap_category,
           "catalog-managed lineage edge is incomplete: " + edge.relation,
           "current_repo_fixable",
           edge.target,
           edge.repair_action});
    }
  }

  summary.blocker_count = plan.blockers.size();
  summary.lineage_score =
      summary.required_edge_count == 0U
          ? 0.0
          : static_cast<double>(summary.ready_edge_count) /
                static_cast<double>(summary.required_edge_count);
  summary.decision = plan.blockers.empty()
                         ? WarehouseDecision::kLineageReady
                         : WarehouseDecision::kBlocked;

  if (summary.current_output_deferred_count >
      summary.managed_artifact_count) {
    throw std::logic_error(
        "warehouse deferred-output count exceeds managed artifact count");
  }
  const std::size_t inventory_obligations =
      summary.managed_artifact_count - summary.current_output_deferred_count;
  if (summary.indexed_artifact_count == 0U) {
    summary.inventory_state = InventoryState::kEmpty;
  } else if (summary.indexed_artifact_count == inventory_obligations) {
    summary.inventory_state = InventoryState::kComplete;
  } else {
    summary.inventory_state = InventoryState::kPartial;
  }

  append_unique(plan.recommended_next_commands, plan.spec->command);
  append_unique(
      plan.recommended_next_commands,
      inferlab_high_value::require_managed_artifact(kEvidenceQueryArtifactId)
          .command);
  validate_summary(plan);
  return plan;
}

}  // namespace inferlab_evidence_warehouse
