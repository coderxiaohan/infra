#include "product_integration_inventory_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "python_source_contract.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace inferlab::product_integration_inventory {
namespace {

struct CachedTextFile {
  std::string normalized_path;
  inferlab_artifact::TextSource source;
};

bool contains_markdown_code_span(const std::string& document,
                                 const std::string& value) {
  return document.find("`" + value + "`") != std::string::npos;
}

const inferlab_artifact::TextSource& cached_text_source(
    std::vector<CachedTextFile>& cache,
    const std::string& id,
    const fs::path& path) {
  const std::string normalized = path.lexically_normal().generic_string();
  for (const auto& cached : cache) {
    if (cached.normalized_path == normalized) return cached.source;
  }
  cache.push_back(
      {normalized, inferlab_artifact::read_text_source(id, path)});
  return cache.back().source;
}

FileAudit make_file_audit(const std::string& repository_path,
                          const fs::path& resolved_path,
                          const inferlab_artifact::TextSource& source) {
  FileAudit audit;
  audit.repository_path = repository_path;
  audit.path_status = inferlab_artifact::path_probe_status(resolved_path);
  audit.read_status = source.status;
  audit.regular_file = audit.path_status == "file";
  audit.readable = audit.regular_file && source.readable;
  audit.read_issue_count = source.read_issue_count;
  return audit;
}

void add_check(InventoryRecord& record,
               const bool passed,
               const std::string& missing_surface) {
  record.total_checks = inferlab::internal::checked_add_int(
      record.total_checks, 1, "product integration total check count");
  if (passed) {
    record.passed_checks = inferlab::internal::checked_add_int(
        record.passed_checks, 1,
        "product integration passed check count");
    return;
  }
  if (std::find(record.missing_surfaces.begin(),
                record.missing_surfaces.end(), missing_surface) !=
      record.missing_surfaces.end()) {
    throw std::logic_error(
        "duplicate product integration missing surface: " +
        missing_surface);
  }
  record.missing_surfaces.push_back(missing_surface);
}

bool config_contains_report(const std::string& text,
                            const std::string& expected_report) {
  std::vector<std::string> reports;
  return inferlab_artifact::parse_string_array_items_fallback(
             inferlab_artifact::json_array_section_or_empty(
                 text, "report_sections"),
             reports) &&
         std::count(reports.begin(), reports.end(), expected_report) == 1;
}

CommonRegistryAudit inspect_common_registry(
    const std::string& common_text,
    const mature_product_profiles::ProductProfileSpec& profile) {
  CommonRegistryAudit audit;
  const auto product = inferlab_python_source::mapping_object(
      common_text, "PRODUCTS", profile.id);
  audit.product_entry_count = product.object_count;
  audit.product_present = product.usable();
  if (!audit.product_present) return audit;
  audit.display_name_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "name", profile.display_name);
  audit.category_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "category", profile.category);
  audit.artifact_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "artifact",
          mature_product_profiles::profile_output_path(profile));
  audit.report_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "report", profile.report);
  audit.config_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "config",
          mature_product_profiles::profile_config_path(profile));
  audit.scope_matches =
      inferlab_python_source::object_string_field_equals(
          product.object_text, "scope",
          mature_product_profiles::profile_scope_name(profile.scope));
  audit.boundary_lists_present =
      inferlab_python_source::object_nonempty_string_list_field(
          product.object_text, "not_implemented") &&
      inferlab_python_source::object_nonempty_string_list_field(
          product.object_text, "local_optimization_scope");
  return audit;
}

FinalReportAudit inspect_final_report(
    const std::string& final_report_text,
    const mature_product_profiles::ProductProfileSpec& profile) {
  FinalReportAudit audit;
  const auto module =
      inferlab_python_source::sequence_object_by_string_field(
          final_report_text, "MODULES", '(', "name",
          "Mature Product Profiles");
  audit.module_count = module.object_count;
  if (!module.usable()) return audit;
  audit.result_occurrence_count =
      inferlab_python_source::object_list_string_count(
          module.object_text, "results", profile.artifact);
  audit.report_occurrence_count =
      inferlab_python_source::object_list_string_count(
          module.object_text, "reports", profile.report);
  audit.result_present = audit.result_occurrence_count == 1U;
  audit.report_present = audit.report_occurrence_count == 1U;
  audit.code_wiring_present =
      inferlab_python_source::object_list_string_count(
          module.object_text, "code",
          "native_core/src/python_source_contract.cpp") == 1U &&
      inferlab_python_source::object_list_string_count(
          module.object_text, "code",
          "native_core/src/product_integration_inventory_model.cpp") == 1U &&
      inferlab_python_source::object_list_string_count(
          module.object_text, "code",
          "native_core/src/product_integration_inventory_view.cpp") == 1U &&
      inferlab_python_source::object_list_string_count(
          module.object_text, "code",
          "native_core/src/product_integration_inventory_main.cpp") == 1U;
  return audit;
}

InventoryRecord inspect_product(
    const mature_product_profiles::ProductProfileSpec& profile,
    const fs::path& repo_root,
    const std::string& common_text,
    const std::string& matrix_text,
    const std::string& catalog_glue_text,
    const std::string& summary_text,
    const bool summary_catalog_consumer_present,
    const bool prometheus_catalog_consumer_present,
    const std::string& final_report_text,
    std::vector<CachedTextFile>& adapter_cache,
    std::vector<CachedTextFile>& config_cache,
    std::vector<CachedTextFile>& report_cache) {
  InventoryRecord record;
  record.profile = &profile;
  record.default_wiring_expected =
      mature_product_profiles::profile_is_default_required(profile);

  const std::string adapter_path =
      mature_product_profiles::profile_source_file(profile);
  const std::string config_path =
      mature_product_profiles::profile_config_path(profile);
  const auto& adapter_source = cached_text_source(
      adapter_cache, "adapter_" + profile.id, repo_root / adapter_path);
  const auto& config_source = cached_text_source(
      config_cache, "config_" + profile.id, repo_root / config_path);
  const auto& report_source = cached_text_source(
      report_cache, "report_" + profile.id, repo_root / profile.report);
  record.adapter = make_file_audit(
      adapter_path, repo_root / adapter_path, adapter_source);
  record.config = make_file_audit(
      config_path, repo_root / config_path, config_source);
  record.report = make_file_audit(
      profile.report, repo_root / profile.report, report_source);

  record.adapter_product_binding_present =
      record.adapter.readable &&
      inferlab_python_source::matching_call_count(
          adapter_source.text, "main_for_product", profile.id, 1U) == 1U;
  record.config_document_valid =
      record.config.readable &&
      inferlab_artifact::json_document_is_valid_object(config_source.text);
  std::string config_product;
  record.config_product_binding_present =
      record.config_document_valid &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          config_source.text, "product", config_product) &&
      config_product == profile.id;
  record.config_report_binding_present =
      record.config_document_valid &&
      config_contains_report(config_source.text, profile.report);

  record.common_registry = inspect_common_registry(common_text, profile);
  record.artifact_catalog_source_occurrence_count =
      inferlab_python_source::assignment_string_literal_count(
          catalog_glue_text, "COMMON_EVIDENCE_SOURCE_EXTENSIONS", '(',
          profile.artifact);
  record.artifact_catalog_source_present =
      record.artifact_catalog_source_occurrence_count == 1U;
  record.artifact_catalog_scope_matches =
      record.default_wiring_expected
          ? record.artifact_catalog_source_present
          : record.artifact_catalog_source_occurrence_count == 0U;

  record.matrix =
      inferlab_repository_wiring::product_profile_matrix_entry_match(
          matrix_text, profile.benchmark_entry, adapter_path,
          mature_product_profiles::profile_module(profile), config_path,
          profile.artifact, profile.report,
          mature_product_profiles::profile_dry_run_command(profile),
          mature_product_profiles::profile_replay_command(profile));
  record.benchmark_matrix_present =
      record.matrix.structural_wiring_registered();
  record.benchmark_dry_run_command_present =
      record.matrix.dry_run_command_registered();
  record.benchmark_replay_command_present =
      record.matrix.replay_command_registered();
  record.benchmark_matrix_scope_matches =
      record.default_wiring_expected
          ? (record.benchmark_matrix_present &&
             record.benchmark_dry_run_command_present &&
             record.benchmark_replay_command_present)
          : record.matrix.entry_count == 0U;

  record.summary_profile_call_count =
      inferlab_python_source::matching_call_count(
          summary_text, "product_profile_section", profile.artifact, 2U,
          "sources");
  record.summary_source_present =
      summary_catalog_consumer_present &&
      record.artifact_catalog_source_present &&
      record.summary_profile_call_count == 1U;
  record.summary_scope_matches =
      record.default_wiring_expected
          ? record.summary_source_present
          : record.summary_profile_call_count == 0U;
  record.prometheus_source_present =
      prometheus_catalog_consumer_present &&
      record.artifact_catalog_source_present;
  record.prometheus_scope_matches =
      record.default_wiring_expected
          ? record.prometheus_source_present
          : !record.artifact_catalog_source_present;
  record.final_report = inspect_final_report(final_report_text, profile);
  record.final_report_result_scope_matches =
      record.default_wiring_expected
          ? record.final_report.result_present
          : record.final_report.result_occurrence_count == 0U;
  record.report_artifact_reference_present =
      record.report.readable &&
      contains_markdown_code_span(
          report_source.text,
          mature_product_profiles::profile_output_path(profile));
  record.report_boundary_present =
      record.report.readable &&
      report_source.text.find("not_implemented_by_inferlab") !=
          std::string::npos;

  add_check(record, record.adapter.regular_file, "adapter_script");
  add_check(record, record.adapter.readable, "adapter_readable");
  add_check(record, record.adapter_product_binding_present,
            "adapter_product_binding");
  add_check(record, record.config.regular_file, "config");
  add_check(record, record.config.readable, "config_readable");
  add_check(record, record.config_document_valid, "config_document");
  add_check(record, record.config_product_binding_present,
            "config_product_binding");
  add_check(record, record.config_report_binding_present,
            "config_report_binding");
  add_check(record, record.report.regular_file, "report");
  add_check(record, record.report.readable, "report_readable");
  add_check(record, record.common_registry.product_present,
            "common_registry_product");
  add_check(record, record.common_registry.display_name_matches,
            "common_registry_display_name");
  add_check(record, record.common_registry.category_matches,
            "common_registry_category");
  add_check(record, record.common_registry.artifact_matches,
            "common_registry_artifact");
  add_check(record, record.common_registry.report_matches,
            "common_registry_report");
  add_check(record, record.common_registry.config_matches,
            "common_registry_config");
  add_check(record, record.common_registry.scope_matches,
            "common_registry_scope");
  add_check(record, record.common_registry.boundary_lists_present,
            "common_registry_boundary");
  add_check(record, record.artifact_catalog_scope_matches,
            "artifact_catalog_scope");
  add_check(record, record.benchmark_matrix_scope_matches,
            "benchmark_matrix_scope");
  add_check(record, record.summary_scope_matches,
            "benchmark_summary_scope");
  add_check(record, record.prometheus_scope_matches,
            "prometheus_scope");
  add_check(record, record.final_report_result_scope_matches,
            "final_report_result_scope");
  if (record.default_wiring_expected) {
    add_check(record, record.final_report.report_present,
              "final_report_report");
  }
  add_check(record, record.final_report.code_wiring_present,
            "final_report_code_wiring");
  add_check(record, record.report_artifact_reference_present,
            "report_artifact_reference");
  add_check(record, record.report_boundary_present,
            "report_product_boundary");

  const int missing_count = inferlab::internal::checked_int_from_size(
      record.missing_surfaces.size(),
      "product integration missing surface count");
  if (inferlab::internal::checked_add_int(
          record.passed_checks, missing_count,
          "product integration record check reconciliation") !=
      record.total_checks) {
    throw std::logic_error(
        "product integration record check reconciliation failed");
  }
  record.integration_score =
      record.total_checks == 0
          ? 1.0
          : static_cast<double>(record.passed_checks) /
                static_cast<double>(record.total_checks);
  if (!std::isfinite(record.integration_score) ||
      record.integration_score < 0.0 || record.integration_score > 1.0) {
    throw std::logic_error("invalid product integration record score");
  }
  record.ready = record.missing_surfaces.empty();
  if (record.ready != (record.passed_checks == record.total_checks)) {
    throw std::logic_error(
        "product integration record readiness reconciliation failed");
  }
  return record;
}

std::size_t cache_read_issue_count(
    const std::vector<CachedTextFile>& cache,
    const char* context) {
  std::size_t count = 0U;
  for (const auto& file : cache) {
    count = inferlab::internal::checked_add_size(
        count, file.source.read_issue_count, context);
  }
  return count;
}

void validate_plan(const InventoryPlan& plan) {
  const int record_count = inferlab::internal::checked_int_from_size(
      plan.records.size(), "product integration record count validation");
  const int scope_count = inferlab::internal::checked_add_int(
      plan.default_required_product_count,
      plan.optional_on_demand_product_count,
      "product integration scope count validation");
  if (record_count != plan.product_count ||
      scope_count != plan.product_count ||
      plan.default_required_product_count <= 0 ||
      plan.optional_on_demand_product_count <= 0 ||
      inferlab::internal::checked_add_int(
          plan.passed_records, plan.failed_records,
          "product integration record summary validation") !=
          plan.product_count) {
    throw std::logic_error(
        "product integration record summary reconciliation failed");
  }
  int passed_records = 0;
  int passed_checks = 0;
  int total_checks = 0;
  for (const auto& record : plan.records) {
    (void)record_profile(record);
    if (record.ready) {
      passed_records = inferlab::internal::checked_add_int(
          passed_records, 1,
          "product integration validated passed record count");
    }
    passed_checks = inferlab::internal::checked_add_int(
        passed_checks, record.passed_checks,
        "product integration validated passed check count");
    total_checks = inferlab::internal::checked_add_int(
        total_checks, record.total_checks,
        "product integration validated total check count");
  }
  if (passed_records != plan.passed_records ||
      passed_checks != plan.passed_checks ||
      total_checks != plan.total_checks) {
    throw std::logic_error(
        "product integration aggregate reconstruction failed");
  }
  const std::size_t expected_total_issues =
      inferlab::internal::checked_sum_size(
          {plan.source_read_issue_count, plan.adapter_read_issue_count,
           plan.config_read_issue_count, plan.report_read_issue_count},
          "product integration total read issue validation");
  if (expected_total_issues != plan.total_read_issue_count) {
    throw std::logic_error(
        "product integration read issue reconciliation failed");
  }
  const Decision expected_decision =
      plan.total_read_issue_count > 0U
          ? Decision::source_read_issues
          : (plan.failed_records == 0 ? Decision::ready
                                      : Decision::wiring_gaps);
  if (plan.decision != expected_decision) {
    throw std::logic_error(
        "product integration decision reconciliation failed");
  }
}

}  // namespace

const ArtifactSpec& artifact_spec() {
  static const ArtifactSpec spec = {
      "native_product_integration_inventory",
      "inferlab_product_integration_inventory",
      "native_product_integration_inventory.json",
      "reports/58_product_integration_inventory.md",
      "native_product_integration_inventory_refresh",
      "./build/inferlab_product_integration_inventory --repo-root . --output results/raw/native_product_integration_inventory.json",
      "Static repository-wiring audit for mature-product profile adapters, configs, registry entries, matrix rows, report chapters and report/export consumers only; it does not execute products, profile runners, replay, benchmarks, or GPU workloads."};
  return spec;
}

const mature_product_profiles::ProductProfileSpec& record_profile(
    const InventoryRecord& record) {
  if (record.profile == nullptr) {
    throw std::logic_error("product integration record has no profile");
  }
  return *record.profile;
}

const char* decision_status(const Decision decision) {
  switch (decision) {
    case Decision::source_read_issues:
      return "source_read_issues";
    case Decision::wiring_gaps:
      return "failed";
    case Decision::ready:
      return "ok";
  }
  throw std::invalid_argument("unknown product integration decision");
}

const char* record_status(const InventoryRecord& record) {
  return record.ready ? "pass" : "fail";
}

const char* record_recommendation(const InventoryRecord& record) {
  if (!record.ready) {
    return "restore_missing_adapter_config_report_or_scope_wiring_before_claims";
  }
  return record.default_wiring_expected
             ? "default_profile_integration_ready_for_profile_generation"
             : "optional_profile_manual_entry_ready_and_default_consumers_excluded";
}

InventoryPlan build_inventory(const fs::path& repo_root) {
  (void)artifact_spec();
  const auto& products = mature_product_profiles::product_profiles();
  std::vector<inferlab_artifact::TextSource> source_files = {
      inferlab_artifact::read_text_source(
          "product_profile_common",
          repo_root / "scripts" / "product_profiles" / "common.py"),
      inferlab_artifact::read_text_source(
          "benchmark_matrix",
          repo_root / "configs" / "benchmark_matrix.yaml"),
      inferlab_artifact::read_text_source(
          "artifact_catalog_glue",
          repo_root / "scripts" / "artifact_catalog_glue.py"),
      inferlab_artifact::read_text_source(
          "summary", repo_root / "scripts" / "summarize_benchmarks.py"),
      inferlab_artifact::read_text_source(
          "prometheus",
          repo_root / "scripts" / "export_prometheus_metrics.py"),
      inferlab_artifact::read_text_source(
          "final_report", repo_root / "scripts" / "render_final_report.py")};

  const std::string& common_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "product_profile_common")
          .text;
  const std::string& matrix_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "benchmark_matrix")
          .text;
  const std::string& catalog_glue_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "artifact_catalog_glue")
          .text;
  const std::string& summary_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "summary")
          .text;
  const std::string& prometheus_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "prometheus")
          .text;
  const std::string& final_report_text =
      inferlab_artifact::require_unique_text_source_by_id(
          source_files, "final_report")
          .text;
  const bool summary_catalog_consumer_present =
      inferlab_python_source::call_count(
          summary_text, "resolve_catalog_sources") == 1U;
  const bool prometheus_catalog_consumer_present =
      inferlab_python_source::call_count(
          prometheus_text, "compose_legacy_sources") == 1U &&
      inferlab_python_source::call_count(
          prometheus_text, "resolve_catalog_sources") == 1U;

  std::vector<CachedTextFile> adapter_cache;
  std::vector<CachedTextFile> config_cache;
  std::vector<CachedTextFile> report_cache;
  adapter_cache.reserve(products.size());
  config_cache.reserve(products.size());
  report_cache.reserve(products.size());

  InventoryPlan plan;
  plan.records.reserve(products.size());
  for (const auto& product : products) {
    plan.records.push_back(inspect_product(
        product, repo_root, common_text, matrix_text, catalog_glue_text,
        summary_text, summary_catalog_consumer_present,
        prometheus_catalog_consumer_present, final_report_text,
        adapter_cache, config_cache, report_cache));
  }

  plan.product_count = inferlab::internal::checked_int_from_size(
      plan.records.size(), "product integration product count");
  for (const auto& record : plan.records) {
    if (record.default_wiring_expected) {
      plan.default_required_product_count =
          inferlab::internal::checked_add_int(
              plan.default_required_product_count, 1,
              "product integration default product count");
    } else {
      plan.optional_on_demand_product_count =
          inferlab::internal::checked_add_int(
              plan.optional_on_demand_product_count, 1,
              "product integration optional product count");
    }
    if (record.ready) {
      plan.passed_records = inferlab::internal::checked_add_int(
          plan.passed_records, 1,
          "product integration passed record count");
    } else {
      plan.failed_records = inferlab::internal::checked_add_int(
          plan.failed_records, 1,
          "product integration failed record count");
    }
    plan.passed_checks = inferlab::internal::checked_add_int(
        plan.passed_checks, record.passed_checks,
        "product integration passed checks");
    plan.total_checks = inferlab::internal::checked_add_int(
        plan.total_checks, record.total_checks,
        "product integration total checks");
  }
  plan.integration_score =
      plan.total_checks == 0
          ? 1.0
          : static_cast<double>(plan.passed_checks) /
                static_cast<double>(plan.total_checks);
  if (!std::isfinite(plan.integration_score) ||
      plan.integration_score < 0.0 || plan.integration_score > 1.0) {
    throw std::logic_error("invalid product integration aggregate score");
  }

  plan.source_read_issue_count =
      inferlab_artifact::source_read_issue_count(source_files);
  plan.adapter_read_issue_count = cache_read_issue_count(
      adapter_cache, "product integration adapter read issue count");
  plan.config_read_issue_count = cache_read_issue_count(
      config_cache, "product integration config read issue count");
  plan.report_read_issue_count = cache_read_issue_count(
      report_cache, "product integration report read issue count");
  plan.total_read_issue_count = inferlab::internal::checked_sum_size(
      {plan.source_read_issue_count, plan.adapter_read_issue_count,
       plan.config_read_issue_count, plan.report_read_issue_count},
      "product integration total read issue count");
  plan.unique_report_file_count = report_cache.size();
  plan.physical_text_read_count = inferlab::internal::checked_sum_size(
      {source_files.size(), adapter_cache.size(), config_cache.size(),
       report_cache.size()},
      "product integration physical text read count");
  plan.decision =
      plan.total_read_issue_count > 0U
          ? Decision::source_read_issues
          : (plan.failed_records == 0 ? Decision::ready
                                      : Decision::wiring_gaps);

  plan.source_files = std::move(source_files);
  for (auto& source : plan.source_files) source.text.clear();
  validate_plan(plan);
  return plan;
}

}  // namespace inferlab::product_integration_inventory
