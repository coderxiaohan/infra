#include "release_evidence_pipeline_repair_group_projection.hpp"

#include "checked_index.hpp"
#include "evidence_domain_utils.hpp"

#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

void validate_group_specs(const std::vector<RepairPathGroupSpec>& specs) {
  if (specs.empty()) {
    throw std::logic_error("release pipeline repair group registry is empty");
  }
  std::set<std::string> ids;
  std::set<std::string> grouping_keys;
  for (const auto& spec : specs) {
    if (spec.id.empty() || spec.grouping.empty() || spec.key.empty() ||
        spec.label.empty() || spec.priority.empty() ||
        spec.claim_boundary.empty()) {
      throw std::logic_error(
          "release pipeline repair group metadata must be nonempty");
    }
    if (spec.grouping != "environment" && spec.grouping != "category" &&
        spec.grouping != "category_family") {
      throw std::logic_error("unsupported release pipeline repair grouping: " +
                             spec.grouping);
    }
    if (spec.grouping == "environment" &&
        inferlab_evidence::parse_repair_environment(spec.key) ==
            inferlab_evidence::RepairEnvironment::kUnknown) {
      throw std::logic_error(
          "release pipeline repair environment group has invalid key: " +
          spec.key);
    }
    if (spec.grouping == "category_family" &&
        spec.key != "native_core_audit") {
      throw std::logic_error(
          "unsupported release pipeline repair category family: " +
          spec.key);
    }
    if (!ids.insert(spec.id).second) {
      throw std::logic_error("duplicate release pipeline repair group id: " +
                             spec.id);
    }
    const std::string grouping_key = spec.grouping + ":" + spec.key;
    if (!grouping_keys.insert(grouping_key).second) {
      throw std::logic_error(
          "duplicate release pipeline repair grouping key: " + grouping_key);
    }
  }
}

bool repair_path_matches_group(const NonGpuReviewRepairPath& path,
                               const RepairPathGroupSpec& spec) {
  if (spec.grouping == "environment") {
    const auto expected =
        inferlab_evidence::parse_repair_environment(spec.key);
    return expected != inferlab_evidence::RepairEnvironment::kUnknown &&
           path.repair_environment == expected;
  }
  if (spec.grouping == "category") {
    return path.category == spec.key;
  }
  return spec.grouping == "category_family" &&
         spec.key == "native_core_audit" &&
         is_native_core_audit_repair_category(path.category);
}

}  // namespace

const std::vector<RepairPathGroupSpec>& repair_path_group_specs() {
  static const std::vector<RepairPathGroupSpec> specs = [] {
    std::vector<RepairPathGroupSpec> result = {
        {"environment_current_repo_fixable", "environment",
         "current_repo_fixable", "Current repo fixes", "P0",
         "groups repair paths that can be fixed in the current repository without claiming validation was run"},
        {"environment_current_repo_then_validation_machine", "environment",
         "current_repo_then_validation_machine",
         "Current repo then validation machine", "P0",
         "groups repair paths that need repository changes first and artifact refresh only when validation is enabled"},
        {"environment_current_server_when_validation_enabled", "environment",
         "current_server_when_validation_enabled",
         "Current server when validation is enabled", "P1",
         "groups repair paths that require validation to be explicitly enabled before refresh commands are evidence"},
        {"environment_target_or_validation_enabled_machine", "environment",
         "target_or_validation_enabled_machine",
         "Target or validation-enabled machine", "P1",
         "groups repair paths that should be closed on the target GPU/product or validation-enabled machine"},
        {"category_catalog_flow_discrepancy", "category",
         "catalog_flow_discrepancy", "Catalog-flow discrepancies", "P0",
         "groups catalog and new-artifact-flow repair paths; does not remove fallback lists or prove parity"},
        {"category_catalog_flow_field_coverage_gap", "category",
         "catalog_flow_field_coverage_gap",
         "Catalog-flow consumer field coverage gaps", "P0",
         "groups catalog consumer required-field coverage repair paths; does not refresh artifacts or prove downstream parity"},
        {"category_catalog_flow_fallback_removal_shortest_path", "category",
         "catalog_flow_fallback_removal_shortest_path",
         "Catalog fallback-removal shortest path", "P0",
         "groups root-cause fallback-removal shortest-path blockers; does not delete fallback lists or prove downstream parity"},
        {"category_catalog_flow_fallback_removal_parity_gate", "category",
         "catalog_flow_fallback_removal_parity_gate",
         "Catalog fallback-removal parity gate", "P0",
         "groups fallback-removal parity blockers; does not remove fallback lists, refresh consumers, or prove downstream parity"},
        {"category_evidence_query_snapshot_diff_review", "category",
         "evidence_query_snapshot_diff_review",
         "Evidence Query snapshot diff review", "P1",
         "groups bounded snapshot-diff review paths; does not refresh snapshots or execute producers"},
        {"category_product_replay_semantic_linkage", "category",
         "product_replay_semantic_linkage",
         "Product replay semantic-linkage blockers", "P0",
         "groups product replay semantic-linkage repair paths; does not import logs, replay products, execute products, or prove benchmark performance"},
        {"category_native_core_audit_scope_blocked", "category",
         "native_core_audit_scope_blocked", "Native-core audit scope blockers",
         "P0",
         "groups static audit scope blockers; does not compile native code or prove runtime correctness"},
        {"category_native_core_audit_backlog", "category",
         "native_core_audit_backlog", "Native-core audit backlog", "P0",
         "groups static audit backlog blockers; does not compile native code or prove runtime correctness"},
        {"category_native_core_audit_review_coverage_gate", "category",
         "native_core_audit_review_coverage_gate",
         "Native-core audit review coverage gate", "P0",
         "groups static audit coverage blockers; does not mark files reviewed or prove runtime correctness"},
        {"category_native_core_audit_review_manifest", "category",
         "native_core_audit_review_manifest",
         "Native-core audit review manifest", "P0",
         "groups static audit review-manifest input blockers; does not mark files reviewed or prove runtime correctness"},
        {"category_family_native_core_audit", "category_family",
         "native_core_audit", "Native-core audit all", "P0",
         "groups all native-core static audit repair paths without claiming build or runtime validation"}};
    validate_group_specs(result);
    return result;
  }();
  return specs;
}

const RepairPathGroupSpec& require_repair_path_group_spec(
    const std::string& group_id) {
  const RepairPathGroupSpec* match = nullptr;
  for (const auto& spec : repair_path_group_specs()) {
    if (spec.id != group_id) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "duplicate release pipeline repair group spec id: " + group_id);
    }
    match = &spec;
  }
  if (match == nullptr) {
    throw std::logic_error(
        "missing release pipeline repair group spec id: " + group_id);
  }
  return *match;
}

bool is_native_core_audit_repair_category(const std::string& category) {
  return category == "native_core_audit_scope_blocked" ||
         category == "native_core_audit_backlog" ||
         category == "native_core_audit_review_coverage_gate" ||
         category == "native_core_audit_review_manifest";
}

std::vector<ActiveRepairPathGroup> active_repair_path_groups(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  std::vector<ActiveRepairPathGroup> groups;
  std::size_t step = 1U;
  for (const auto& spec : repair_path_group_specs()) {
    ActiveRepairPathGroup group;
    group.spec = spec;
    for (std::size_t path_index = 0; path_index < paths.size(); ++path_index) {
      const auto& path = paths[path_index];
      if (!repair_path_matches_group(path, spec)) continue;
      if (group.first_path_index == kNoRepairPathIndex) {
        group.first_path_index = path_index;
      }
      group.path_count = inferlab::internal::checked_add_size(
          group.path_count, 1U, "repair path group path count");
      if (path.source_locations.empty()) {
        group.source_location_gap_count = inferlab::internal::checked_add_size(
            group.source_location_gap_count, 1U,
            "repair path group source-location gap count");
      }
      group.evidence_counter_count = inferlab::internal::checked_add_size(
          group.evidence_counter_count, path.evidence_counters.size(),
          "repair path group evidence counter count");
      group.current_repo_action_required =
          group.current_repo_action_required ||
          inferlab_evidence::repair_environment_is_current_repo(
              path.repair_environment);
      group.target_or_validation_action_required =
          group.target_or_validation_action_required ||
          inferlab_evidence::repair_environment_is_target_or_validation(
              path.repair_environment);
    }
    if (group.path_count == 0U) continue;
    group.step = step;
    step = inferlab::internal::checked_add_size(
        step, 1U, "release pipeline repair group step");
    if (group.current_repo_action_required &&
        group.target_or_validation_action_required) {
      group.action_owner = "current_repo_then_target_or_validation";
    } else if (group.current_repo_action_required) {
      group.action_owner = "current_repo";
    } else if (group.target_or_validation_action_required) {
      group.action_owner = "target_or_validation_machine";
    }
    groups.push_back(std::move(group));
  }
  return groups;
}

const NonGpuReviewRepairPath* first_repair_path_for_group(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const ActiveRepairPathGroup& group) {
  if (group.first_path_index == kNoRepairPathIndex) return nullptr;
  if (group.first_path_index >= paths.size()) {
    throw std::logic_error(
        "release pipeline repair group first-path index is out of range");
  }
  return &paths[group.first_path_index];
}

const ActiveRepairPathGroup* active_repair_path_group_for_id(
    const std::vector<ActiveRepairPathGroup>& groups,
    const std::string& group_id) {
  const ActiveRepairPathGroup* match = nullptr;
  for (const auto& group : groups) {
    if (group.spec.id != group_id) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "duplicate active release pipeline repair group id: " + group_id);
    }
    match = &group;
  }
  return match;
}

}  // namespace inferlab_release_pipeline
