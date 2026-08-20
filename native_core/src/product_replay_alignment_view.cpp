#include "product_replay_alignment_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "artifact_metadata_reader.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::product_replay_alignment {
namespace {

constexpr const char* kArtifactType = "native_product_replay_alignment";
constexpr const char* kTarget = "inferlab_product_replay_alignment";
constexpr const char* kArtifactPath =
    "results/raw/native_product_replay_alignment.json";
constexpr const char* kRefreshCommand =
    "./build/inferlab_product_replay_alignment --repo-root . --results results/raw --output results/raw/native_product_replay_alignment.json --format json";

std::string redacted_error(const std::string& error,
                           const std::filesystem::path& path) {
  return inferlab_plan::redact_error_message(error, {path.string()});
}

std::string resolved_path_label(const std::filesystem::path& path) {
  return inferlab_plan::redacted_path_label(path.generic_string());
}

bool resolved_path_redacted(const std::filesystem::path& path) {
  const std::string raw = path.generic_string();
  return inferlab_plan::path_was_redacted(raw,
                                          inferlab_plan::redacted_path_label(
                                              raw));
}

std::string cli_context_json(const AlignmentArtifactViewContext& context,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": "
      << inferlab_plan::json_string(context.format) << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, indent + 2) << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "repo_root", context.repo_root, indent + 2)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "results_root", context.results_root, indent + 2)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "output", context.output, indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"CLI context documents accepted product replay alignment inputs only; it does not execute products, refresh producers, run benchmarks, or claim product-core ownership\"\n";
  out << pad << "}";
  return out.str();
}

std::string product_state_json(const ProductArtifactState& state,
                               const int indent) {
  const auto& profile = product_profile(state);
  const auto& contract = state.contract;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string probe_error =
      contract.path_probe_status == "path_probe_error"
          ? redacted_error(
                contract.text_read_error.empty()
                    ? "failed to inspect artifact path"
                    : contract.text_read_error,
                state.resolved_path)
          : "";
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(profile.artifact) << ",\n";
  out << nested << "\"artifact_contract_ready\": "
      << inferlab_plan::json_bool(contract.contract_ready) << ",\n";
  out << nested << "\"boundary_score\": "
      << inferlab_plan::fixed6(contract.boundary_score) << ",\n";
  out << nested << "\"can_claim_product_performance\": "
      << inferlab_plan::json_bool(contract.can_claim_product_performance)
      << ",\n";
  out << nested << "\"claim_scope_present\": "
      << inferlab_plan::json_bool(contract.claim_scope_present) << ",\n";
  out << nested << "\"contract_coverage\": "
      << inferlab_plan::fixed6(contract.contract_coverage) << ",\n";
  out << nested << "\"deep_dive_type_present\": false,\n";
  out << nested << "\"dry_run\": "
      << inferlab_plan::json_bool(contract.mode == "dry_run") << ",\n";
  out << nested << "\"executed\": "
      << inferlab_plan::json_bool(contract.executed) << ",\n";
  out << nested << "\"family\": \"product_profile\",\n";
  out << nested << "\"invalid_type_fields\": "
      << inferlab_plan::json_string_array(contract.invalid_type_fields,
                                           indent + 2)
      << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(contract.issues, indent + 2)
      << ",\n";
  out << nested << "\"metric_rows_present\": "
      << inferlab_plan::json_bool(contract.replay_metric_evidence_present)
      << ",\n";
  out << nested << "\"missing_fields\": "
      << inferlab_plan::json_string_array(contract.missing_fields, indent + 2)
      << ",\n";
  out << nested << "\"mode\": "
      << inferlab_plan::json_string(contract.mode) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(profile.id) << ",\n";
  out << nested << "\"path_probe_error\": "
      << inferlab_plan::json_string(probe_error) << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(contract.path_probe_status) << ",\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(contract.present) << ",\n";
  out << nested << "\"product_evidence_eligible\": "
      << inferlab_plan::json_bool(contract.product_evidence_eligible)
      << ",\n";
  out << nested << "\"raw_metrics_present\": "
      << inferlab_plan::json_bool(contract.raw_metrics_present) << ",\n";
  out << nested << "\"readable\": "
      << inferlab_plan::json_bool(contract.readable) << ",\n";
  out << nested << "\"replayed\": "
      << inferlab_plan::json_bool(contract.replayed) << ",\n";
  out << nested << "\"resolved_path\": "
      << inferlab_plan::json_string(resolved_path_label(state.resolved_path))
      << ",\n";
  out << nested << "\"resolved_path_redacted\": "
      << inferlab_plan::json_bool(resolved_path_redacted(state.resolved_path))
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(contract.status) << ",\n";
  out << nested << "\"text_read_error\": "
      << inferlab_plan::json_string(
             redacted_error(contract.text_read_error, state.resolved_path))
      << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(contract.text_read_status) << "\n";
  out << pad << "}";
  return out.str();
}

std::string native_state_json(const NativeArtifactState& state,
                              const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(state.identity.artifact) << ",\n";
  out << nested << "\"artifact_contract_ready\": "
      << inferlab_plan::json_bool(state.contract_ready) << ",\n";
  out << nested << "\"claim_scope_present\": "
      << inferlab_plan::json_bool(state.contract.claim_scope_present)
      << ",\n";
  out << nested << "\"deep_dive_type_present\": "
      << inferlab_plan::json_bool(state.deep_dive_type_present) << ",\n";
  out << nested << "\"dry_run\": "
      << inferlab_plan::json_bool(state.contract.mode == "dry_run")
      << ",\n";
  out << nested << "\"exact_artifact_type\": "
      << inferlab_plan::json_bool(state.exact_artifact_type) << ",\n";
  out << nested << "\"executed\": "
      << inferlab_plan::json_bool(state.contract.executed) << ",\n";
  out << nested << "\"expected_artifact_type\": "
      << inferlab_plan::json_string(state.identity.expected_artifact_type)
      << ",\n";
  out << nested << "\"family\": "
      << inferlab_plan::json_string(state.identity.family) << ",\n";
  out << nested << "\"identity_owner\": "
      << inferlab_plan::json_string(state.identity.identity_owner) << ",\n";
  out << nested << "\"identity_source\": "
      << inferlab_plan::json_string(
             native_identity_source_name(state.identity.source))
      << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(state.issues, indent + 2) << ",\n";
  out << nested << "\"metric_rows_present\": "
      << inferlab_plan::json_bool(state.required_metrics_present) << ",\n";
  out << nested << "\"missing_required_fields\": "
      << inferlab_plan::json_string_array(state.missing_required_fields,
                                           indent + 2)
      << ",\n";
  out << nested << "\"missing_required_metrics\": "
      << inferlab_plan::json_string_array(state.missing_required_metrics,
                                           indent + 2)
      << ",\n";
  out << nested << "\"mode\": "
      << inferlab_plan::json_string(state.contract.mode) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(state.identity.role) << ",\n";
  out << nested << "\"observed_artifact_type\": "
      << inferlab_plan::json_string(state.contract.artifact_type) << ",\n";
  out << nested << "\"path_probe_error\": "
      << inferlab_plan::json_string(
             redacted_error(state.path_probe_error, state.resolved_path))
      << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(state.path_probe_status) << ",\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(state.contract.present) << ",\n";
  out << nested << "\"readable\": "
      << inferlab_plan::json_bool(state.contract.readable) << ",\n";
  out << nested << "\"replayed\": "
      << inferlab_plan::json_bool(state.contract.replayed) << ",\n";
  out << nested << "\"required_fields_present\": "
      << inferlab_plan::json_bool(state.required_fields_present) << ",\n";
  out << nested << "\"required_metrics_present\": "
      << inferlab_plan::json_bool(state.required_metrics_present) << ",\n";
  out << nested << "\"resolved_path\": "
      << inferlab_plan::json_string(resolved_path_label(state.resolved_path))
      << ",\n";
  out << nested << "\"resolved_path_redacted\": "
      << inferlab_plan::json_bool(resolved_path_redacted(state.resolved_path))
      << ",\n";
  out << nested << "\"text_read_error\": "
      << inferlab_plan::json_string(
             redacted_error(state.text_read_error, state.resolved_path))
      << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(state.text_read_status) << "\n";
  out << pad << "}";
  return out.str();
}

std::string selected_product_states_json(const AlignmentPlan& plan,
                                         const AlignmentRecord& record,
                                         const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < record.product_artifact_indices.size(); ++i) {
    const auto index = record.product_artifact_indices[i];
    if (index >= plan.product_artifacts.size()) {
      throw std::logic_error(
          "product replay alignment product view index is invalid");
    }
    out << product_state_json(plan.product_artifacts[index], indent + 2)
        << (i + 1U == record.product_artifact_indices.size() ? "\n"
                                                             : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string selected_native_states_json(const AlignmentPlan& plan,
                                        const AlignmentRecord& record,
                                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < record.native_artifact_indices.size(); ++i) {
    const auto index = record.native_artifact_indices[i];
    if (index >= plan.native_artifacts.size()) {
      throw std::logic_error(
          "product replay alignment native view index is invalid");
    }
    out << native_state_json(plan.native_artifacts[index], indent + 2)
        << (i + 1U == record.native_artifact_indices.size() ? "\n"
                                                            : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string record_json(const AlignmentPlan& plan,
                        const AlignmentRecord& record,
                        const int indent) {
  const auto& rule = alignment_rule(record);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string evidence_scope =
      record.product_evidence_count == 0U
          ? "none"
          : (record.full_product_evidence_coverage ? "all_products"
                                                   : "subset_of_products");
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"alignment_score\": "
      << inferlab_plan::fixed6(record.alignment_score) << ",\n";
  out << nested << "\"alignment_score_formula\": "
      << inferlab_plan::json_string(
             "mean(product_contract_coverage,native_contract_coverage,replay_or_execute_evidence_coverage)")
      << ",\n";
  out << nested << "\"alignment_type\": "
      << inferlab_plan::json_string(rule.alignment_type) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(rule.claim_boundary) << ",\n";
  out << nested << "\"evidence_product_scope\": "
      << inferlab_plan::json_string(evidence_scope) << ",\n";
  out << nested << "\"full_product_evidence_coverage\": "
      << inferlab_plan::json_bool(record.full_product_evidence_coverage)
      << ",\n";
  out << nested << "\"native_artifacts\": "
      << selected_native_states_json(plan, record, indent + 2) << ",\n";
  out << nested << "\"native_contract_complete\": "
      << inferlab_plan::json_bool(record.native_contract_complete) << ",\n";
  out << nested << "\"native_contract_ready_count\": "
      << record.native_contract_ready_count << ",\n";
  out << nested << "\"native_coverage\": "
      << inferlab_plan::fixed6(record.native_coverage) << ",\n";
  out << nested << "\"native_explainers\": "
      << inferlab_plan::json_string_array(rule.native_explainers, indent + 2)
      << ",\n";
  out << nested << "\"native_present_count\": "
      << record.native_present_count << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(rule.name) << ",\n";
  out << nested << "\"product_artifacts\": "
      << selected_product_states_json(plan, record, indent + 2) << ",\n";
  out << nested << "\"product_contract_complete\": "
      << inferlab_plan::json_bool(record.product_contract_complete) << ",\n";
  out << nested << "\"product_contract_coverage\": "
      << inferlab_plan::fixed6(record.product_contract_coverage) << ",\n";
  out << nested << "\"product_contract_ready_count\": "
      << record.product_contract_ready_count << ",\n";
  out << nested << "\"product_coverage\": "
      << inferlab_plan::fixed6(record.product_coverage) << ",\n";
  out << nested << "\"product_evidence_count\": "
      << record.product_evidence_count << ",\n";
  out << nested << "\"product_executed_count\": "
      << record.product_executed_count << ",\n";
  out << nested << "\"product_present_count\": "
      << record.product_present_count << ",\n";
  out << nested << "\"product_replayed_count\": "
      << record.product_replayed_count << ",\n";
  out << nested << "\"read_issue_count\": " << record.read_issue_count
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(record.recommendation) << ",\n";
  out << nested << "\"replay_evidence_coverage\": "
      << inferlab_plan::fixed6(record.replay_evidence_coverage) << ",\n";
  out << nested << "\"replay_signals\": "
      << inferlab_plan::json_string_array(rule.replay_signals, indent + 2)
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record.status) << ",\n";
  out << nested << "\"structural_alignment_coverage\": "
      << inferlab_plan::fixed6(record.structural_alignment_coverage) << "\n";
  out << pad << "}";
  return out.str();
}

void validate_view_input(const AlignmentPlan& plan,
                         const AlignmentArtifactViewContext& context) {
  if (context.format != "json" || plan.records.empty() ||
      plan.product_artifacts.empty() || plan.native_artifacts.empty() ||
      plan.product_display_names.empty() ||
      plan.recommended_next_commands.empty() || plan.claim_boundary.empty() ||
      plan.summary.rule_count != plan.records.size() ||
      plan.summary.unique_product_artifact_count !=
          plan.product_artifacts.size() ||
      plan.summary.unique_native_artifact_count !=
          plan.native_artifacts.size() ||
      plan.summary.best_record_index >= plan.records.size() ||
      plan.summary.best_alignment_rule !=
          alignment_rule(plan.records[plan.summary.best_record_index]).name) {
    throw std::logic_error(
        "product replay alignment view input is inconsistent");
  }
}

}  // namespace

std::string alignment_artifact_json(
    const AlignmentPlan& plan,
    const AlignmentArtifactViewContext& context) {
  validate_view_input(plan, context);
  const auto& summary = plan.summary;
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, kArtifactType, plan.claim_boundary, plan.product_display_names,
      kTarget, kArtifactPath, kRefreshCommand);
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_product_replay_alignment\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"audit_type\": \"product_replay_alignment\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(plan.claim_boundary) << ",\n";
  out << "  \"research_basis\": [\"canonical mature-product profile artifacts\", \"canonical native deep-dive, mature-product dispatch, and General Artifact Contract identities\", \"single-read structural and semantic artifact contracts\"],\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"product_identity_and_commands\": \"mature_product_profile_registry.hpp\",\n";
  out << "    \"product_artifact_semantics\": \"product_profile_artifact_contract.hpp/.cpp\",\n";
  out << "    \"native_identity\": \"deep_dive_core_artifact_registry.hpp plus deep_dive_artifact_identity.hpp/.cpp plus mature_product_artifact_dispatch.hpp and General Artifact Contract\",\n";
  out << "    \"native_structural_contract\": \"artifact_evidence_contract.hpp/.cpp\",\n";
  out << "    \"alignment_rules\": \"product_replay_alignment_fixture.hpp/.cpp\",\n";
  out << "    \"request_and_decision\": \"product_replay_alignment_model.hpp/.cpp\",\n";
  out << "    \"artifact_projection\": \"product_replay_alignment_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"cli_context\": " << cli_context_json(context, 2) << ",\n";
  out << "  \"not_implemented_by_inferlab\": [\"production vLLM scheduler\", \"production SGLang scheduler\", \"production TensorRT-LLM runtime\", \"production FlashInfer kernels\", \"production LMCache/Mooncake KV fabric\", \"production Dynamo/KServe/Ray control plane\"],\n";
  out << "  \"results_root\": "
      << inferlab_plan::json_string(resolved_path_label(context.results_root))
      << ",\n";
  out << "  \"results_root_redacted\": "
      << inferlab_plan::json_bool(
             resolved_path_redacted(context.results_root))
      << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << ",\n";
  out << "  \"best_selection_policy\": \"status_rank_then_alignment_score_with_stable_rule_order\",\n";
  out << "  \"summary\": {\n";
  out << "    \"aligned_rule_count\": " << summary.aligned_rule_count
      << ",\n";
  out << "    \"alignment_decision\": "
      << inferlab_plan::json_string(summary.alignment_decision) << ",\n";
  out << "    \"best_alignment_rule\": "
      << inferlab_plan::json_string(summary.best_alignment_rule) << ",\n";
  out << "    \"best_alignment_score\": "
      << inferlab_plan::fixed6(summary.best_alignment_score) << ",\n";
  out << "    \"blocked_rule_count\": " << summary.blocked_rule_count
      << ",\n";
  out << "    \"dry_run_rule_count\": " << summary.dry_run_rule_count
      << ",\n";
  out << "    \"execute_evidence_product_count\": "
      << summary.execute_evidence_product_count << ",\n";
  out << "    \"fully_aligned_rule_count\": "
      << summary.fully_aligned_rule_count << ",\n";
  out << "    \"native_contract_ready_count\": "
      << summary.native_contract_ready_count << ",\n";
  out << "    \"pending_rule_count\": " << summary.pending_rule_count
      << ",\n";
  out << "    \"physical_artifact_read_count\": "
      << summary.physical_artifact_read_count << ",\n";
  out << "    \"product_contract_ready_count\": "
      << summary.product_contract_ready_count << ",\n";
  out << "    \"read_issue_count\": " << summary.read_issue_count
      << ",\n";
  out << "    \"replay_evidence_product_count\": "
      << summary.replay_evidence_product_count << ",\n";
  out << "    \"rule_count\": " << summary.rule_count << ",\n";
  out << "    \"unique_native_artifact_count\": "
      << summary.unique_native_artifact_count << ",\n";
  out << "    \"unique_product_artifact_count\": "
      << summary.unique_product_artifact_count << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t i = 0U; i < plan.records.size(); ++i) {
    out << record_json(plan, plan.records[i], 4)
        << (i + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::product_replay_alignment
