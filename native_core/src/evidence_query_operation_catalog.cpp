#include "evidence_query_operation_catalog.hpp"

#include "checked_index.hpp"
#include "evidence_domain_utils.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_selection.hpp"

#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

QueryOperation operation_for_spec(const EvidenceQueryOperationSpec& spec) {
  QueryOperation operation;
  operation.operation = spec.operation;
  if (spec.supported_targets.size() == 1U) {
    operation.target = spec.supported_targets.front();
  }
  return operation;
}

void validate_operation_catalog(
    const std::vector<EvidenceQueryOperationSpec>& specs) {
  if (specs.empty()) {
    throw std::logic_error("Evidence Query operation catalog is empty");
  }

  std::set<std::pair<std::string, std::string>> identities;
  std::set<std::string> ux_forms;
  for (const auto& spec : specs) {
    if (spec.operation.empty() || spec.positional_form.empty() ||
        spec.ux_form.empty() ||
        spec.selector_hint.empty() || spec.target_hint.empty() ||
        spec.catalog_current_repo_action.empty() ||
        spec.catalog_target_machine_action.empty() ||
        spec.operation_current_repo_action.empty() ||
        spec.operation_target_machine_action.empty()) {
      throw std::logic_error(
          "Evidence Query operation catalog contains an empty field");
    }
    const QueryOperation operation = operation_for_spec(spec);
    const bool target_required = operation_target_required(operation);
    if (target_required != (spec.supported_targets.size() == 1U)) {
      throw std::logic_error(
          "Evidence Query operation target metadata does not match policy");
    }
    if (!spec.supported_targets.empty() &&
        !inferlab_evidence::is_readiness_target(
            spec.supported_targets.front())) {
      throw std::logic_error(
          "Evidence Query operation catalog has an invalid readiness target");
    }
    const std::string target =
        spec.supported_targets.empty() ? "" : spec.supported_targets.front();
    if (!identities.emplace(spec.operation, target).second) {
      throw std::logic_error(
          "Evidence Query operation catalog contains a duplicate identity");
    }
    if (!ux_forms.emplace(spec.ux_form).second) {
      throw std::logic_error(
          "Evidence Query operation catalog contains a duplicate UX form");
    }
  }
}

}  // namespace

const std::vector<EvidenceQueryOperationSpec>&
evidence_query_operation_catalog() {
  static const std::vector<EvidenceQueryOperationSpec> specs = {
      {"list_artifacts", "list artifacts", "list_artifacts", "none", "none", {},
       "inspect artifact inventory and choose a focused show/explain/suggest query",
       "rerun after target-machine artifact refresh if inventory changed",
       "scan artifact status and choose a focused artifact or blocker query",
       "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled"},
      {"show_artifact", "show artifact <artifact>", "show_artifact <id>",
       "artifact id or artifact filename", "none", {},
       "inspect the selected artifact metadata, wiring status, and blocker category",
       "refresh the selected artifact on the target machine only when validation is enabled",
       "inspect the selected artifact contract, blockers, and exact wiring locations",
       "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled"},
      {"explain_blocker", "explain blocker <category|artifact>",
       "explain_blocker <category|artifact>",
       "blocker category or artifact id", "none", {},
       "inspect why the blocker exists and which wiring locations are affected",
       "rerun replay/execute validation only after current-repo blockers are closed",
       "review blocker category, affected claims, and repair cards before changing code",
       "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled"},
      {"suggest_repair", "suggest repair <artifact|category|decision>",
       "suggest_repair <artifact|decision>",
       "artifact id, blocker category, or release decision", "none", {},
       "route repair cards to current repo, current server, target machine, or product replay owners",
       "perform target-machine repair only after repository wiring and artifacts are refreshed",
       "use repair cards as guidance only; apply source changes separately",
       "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled"},
      {"repair_group_summary", "repair groups <artifact|category>",
       "repair_group_summary <artifact|category>",
       "artifact id, blocker category, or release decision", "none", {},
       "inspect the compact repair-group summary before expanding detailed repair cards",
       "perform target-machine or product replay actions only after current-repo groups are closed",
       "inspect the primary repair group and route current-repo blockers before opening detailed repair cards",
       "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled"},
      {"diff_snapshots", "diff snapshots --before <old> --after <new>",
       "diff_snapshots",
       "none", "before and after snapshot paths", {},
       "configure --before/--after snapshot paths; this artifact only probes path status",
       "compare refreshed snapshots on a validation machine when snapshot diff execution is enabled",
       "provide --before and --after evidence snapshots when comparing review states",
       "compare refreshed snapshots after target-machine validation is explicitly enabled"},
      {"path", "path report_ready", "path_report_ready", "none", "report_ready",
       {"report_ready"},
       "close current-repo wiring, Catalog, and Native Audit blockers needed for report-ready handoff",
       "render summary, Prometheus, and final report after artifacts are refreshed on the validation machine",
       "inspect the selected readiness path and repair current-repo blockers first",
       "run replay or execute validation on the target machine only after current-repo blockers are resolved"},
      {"path", "path replay_ready", "path_replay_ready", "none", "replay_ready",
       {"replay_ready"},
       "close current-repo blockers before importing product replay evidence",
       "run or import product replay evidence on the target product/replay environment",
       "inspect the selected readiness path and repair current-repo blockers first",
       "run replay or execute validation on the target machine only after current-repo blockers are resolved"},
      {"path", "path execute_ready", "path_execute_ready", "none", "execute_ready",
       {"execute_ready"},
       "close repo/server/replay blockers before claiming execute readiness",
       "run CMake/native/CUDA/product execute validation on the target machine only when explicitly enabled",
       "inspect the selected readiness path and repair current-repo blockers first",
       "run replay or execute validation on the target machine only after current-repo blockers are resolved"},
  };
  static const bool validated = [] {
    validate_operation_catalog(specs);
    return true;
  }();
  (void)validated;
  return specs;
}

const std::vector<std::string>& evidence_query_operation_ux_forms() {
  static const std::vector<std::string> forms = [] {
    std::vector<std::string> values;
    values.reserve(evidence_query_operation_catalog().size());
    for (const auto& spec : evidence_query_operation_catalog()) {
      values.push_back(spec.ux_form);
    }
    return values;
  }();
  return forms;
}

const std::vector<std::string>& evidence_query_operation_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> values;
    values.reserve(evidence_query_operation_catalog().size());
    for (const auto& spec : evidence_query_operation_catalog()) {
      const std::size_t separator = spec.ux_form.find(' ');
      values.push_back(spec.ux_form.substr(0, separator));
    }
    return values;
  }();
  return names;
}

const EvidenceQueryOperationSpec* find_evidence_query_operation_spec(
    const std::string& operation,
    const std::string& target) {
  const EvidenceQueryOperationSpec* operation_fallback = nullptr;
  for (const auto& spec : evidence_query_operation_catalog()) {
    if (spec.operation != operation) continue;
    if (operation_fallback == nullptr) operation_fallback = &spec;
    if (spec.supported_targets.empty()) return &spec;
    if (!target.empty() && spec.supported_targets.front() == target) {
      return &spec;
    }
  }
  return target.empty() ? operation_fallback : nullptr;
}

EvidenceQueryOperationCatalogSummary
evidence_query_operation_catalog_summary() {
  EvidenceQueryOperationCatalogSummary summary;
  const auto& specs = evidence_query_operation_catalog();
  summary.supported_operation_count = specs.size();
  summary.read_only_operation_count = specs.size();
  for (const auto& spec : specs) {
    const QueryOperation operation = operation_for_spec(spec);
    if (operation_snapshot_inputs_required(operation)) {
      summary.snapshot_required_operation_count =
          inferlab::internal::checked_add_size(
              summary.snapshot_required_operation_count, 1U,
              "Evidence Query snapshot-required operation count");
    }
    if (operation_selector_recommended(operation)) {
      summary.selector_recommended_operation_count =
          inferlab::internal::checked_add_size(
              summary.selector_recommended_operation_count, 1U,
              "Evidence Query selector-recommended operation count");
    }
    if (operation_target_required(operation)) {
      summary.target_required_operation_count =
          inferlab::internal::checked_add_size(
              summary.target_required_operation_count, 1U,
              "Evidence Query target-required operation count");
    }
  }
  return summary;
}

std::string evidence_query_usage_summary() {
  std::ostringstream out;
  out << "supported positional forms: ";
  bool first = true;
  for (const auto& spec : evidence_query_operation_catalog()) {
    if (!first) out << " | ";
    first = false;
    out << spec.positional_form;
  }
  out << "; single-token artifact selectors remain supported";
  return out.str();
}

}  // namespace inferlab_evidence_query
