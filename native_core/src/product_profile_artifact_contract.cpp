#include "product_profile_artifact_contract.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "mature_product_profile_contract.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::mature_product_profiles {
namespace {

constexpr const char* kSchemaVersion = "product_profile.v1";
constexpr const char* kSchemaName = "product_profile";
constexpr const char* kSchemaObjectVersion = "v1";
constexpr const char* kAdapterType = "external_product_profile";
constexpr const char* kAdapterBoundary =
    "adapter/profile/config/artifact/report only; product core remains external";
constexpr const char* kClaimRole =
    "adapter/profile/config/artifact/report/contract; product core remains external";

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0U, prefix.size(), prefix) == 0;
}

bool contains(const std::string& value, const std::string& fragment) {
  return value.find(fragment) != std::string::npos;
}

bool top_level_value_matches_kind(
    const std::string& document,
    const ProductProfileFieldSpec& field) {
  std::size_t start = std::string::npos;
  std::size_t end = std::string::npos;
  if (!inferlab_artifact::find_top_level_json_value_fallback(
          document, field.name, start, end) ||
      start >= document.size() || end <= start) {
    return false;
  }
  switch (field.kind) {
    case ProductProfileFieldKind::string: {
      std::string value;
      return inferlab_artifact::extract_top_level_string_key_fallback(
                 document, field.name, value) &&
             !value.empty();
    }
    case ProductProfileFieldKind::object:
      return document[start] == '{' &&
             !inferlab_artifact::json_object_section_or_empty(
                  document, field.name)
                  .empty();
    case ProductProfileFieldKind::array:
      return document[start] == '[' &&
             !inferlab_artifact::json_array_section_or_empty(
                  document, field.name)
                  .empty();
    case ProductProfileFieldKind::boolean: {
      bool value = false;
      return inferlab_artifact::extract_bool_key_fallback(
          document, field.name, value);
    }
  }
  return false;
}

bool string_array(const std::string& document,
                  const std::string& key,
                  std::vector<std::string>& values) {
  const std::string section =
      inferlab_artifact::json_array_section_or_empty(document, key);
  return !section.empty() &&
         inferlab_artifact::parse_string_array_items_fallback(section,
                                                               values);
}

bool nonempty_string_array(const std::string& document,
                           const std::string& key) {
  std::vector<std::string> values;
  if (!string_array(document, key, values) || values.empty()) return false;
  return std::all_of(values.begin(), values.end(), [](const auto& value) {
    return !value.empty();
  });
}

bool official_sources_are_valid(const std::string& document) {
  std::vector<std::string> values;
  if (!string_array(document, "official_sources", values) || values.empty()) {
    return false;
  }
  return std::all_of(values.begin(), values.end(), [](const auto& value) {
    return starts_with(value, "https://") || starts_with(value, "http://");
  });
}

bool schema_contract_is_valid(const std::string& document) {
  std::string schema_version;
  if (!inferlab_artifact::extract_top_level_string_key_fallback(
          document, "schema_version", schema_version) ||
      schema_version != kSchemaVersion) {
    return false;
  }
  const std::string schema =
      inferlab_artifact::json_object_section_or_empty(document, "schema");
  std::string name;
  std::string version;
  return !schema.empty() &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             schema, "name", name) &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             schema, "version", version) &&
         name == kSchemaName && version == kSchemaObjectVersion;
}

bool adapter_contract_is_valid(const std::string& document,
                               const ProductProfileSpec& profile) {
  const std::string adapter =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "inferlab_adapter");
  if (adapter.empty()) return false;
  std::string adapter_type;
  std::string boundary;
  std::string script;
  std::string script_module;
  std::string entry_kind;
  bool paths_redacted = false;
  const std::string expected_module = profile_module(profile);
  return inferlab_artifact::extract_top_level_string_key_fallback(
             adapter, "adapter_type", adapter_type) &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             adapter, "boundary", boundary) &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             adapter, "script", script) &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             adapter, "script_module", script_module) &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             adapter, "script_entry_kind", entry_kind) &&
         inferlab_artifact::extract_bool_key_fallback(
             adapter, "paths_redacted", paths_redacted) &&
         adapter_type == kAdapterType && boundary == kAdapterBoundary &&
         script == expected_module && script_module == expected_module &&
         entry_kind == "python_module" && paths_redacted;
}

bool command_contract_is_valid(const std::string& commands,
                               const ProductProfileSpec& profile,
                               const std::string& key,
                               const std::string& flag) {
  std::string command;
  return inferlab_artifact::extract_top_level_string_key_fallback(
             commands, key, command) &&
         starts_with(command, profile_command_prefix(profile)) &&
         contains(command, flag);
}

bool planned_commands_are_valid(const std::string& document,
                                const ProductProfileSpec& profile) {
  const std::string commands =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "planned_commands");
  return !commands.empty() &&
         command_contract_is_valid(commands, profile, "dry_run",
                                   "--dry-run") &&
         command_contract_is_valid(commands, profile, "execute",
                                   "--execute") &&
         command_contract_is_valid(commands, profile, "replay",
                                   "--replay");
}

bool raw_metrics_contract_is_valid(const std::string& document) {
  const std::string metrics =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "raw_metrics");
  if (metrics.empty()) return false;
  std::vector<std::string> expected_metrics;
  return string_array(metrics, "expected_metrics", expected_metrics) &&
         !inferlab_artifact::json_object_section_or_empty(metrics,
                                                          "collected")
              .empty() &&
         inferlab_artifact::contains_top_level_key_fallback(
             metrics, "unavailable_reason");
}

bool supported_mode(const std::string& mode) {
  return mode == "dry_run" || mode == "execute" || mode == "replay";
}

bool mode_status_consistent(const std::string& mode,
                            const std::string& status) {
  if (mode == "dry_run") {
    return status == "planned" || status == "invalid_config";
  }
  if (mode == "execute" || mode == "replay") {
    return status == "ok" || status == "partial" || status == "failed" ||
           status == "invalid_config";
  }
  return false;
}

std::string expected_performance_claim(const std::string& mode,
                                       const std::string& status) {
  if (status == "invalid_config") {
    return "not_supported_from_invalid_config";
  }
  if (mode == "execute") return "allowed_from_execute_artifact";
  if (mode == "replay") {
    return status == "ok" ? "allowed_from_offline_replay_evidence"
                          : "not_supported_from_replay_without_metrics";
  }
  return "not_supported_from_dry_run";
}

bool claim_scope_is_consistent(const std::string& document,
                               const std::string& mode,
                               const std::string& status,
                               std::string& observed_claim) {
  const std::string claim_scope =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "claim_scope");
  if (claim_scope.empty()) return false;
  std::string claim_mode;
  std::string role;
  bool requires_target = false;
  bool requires_external = false;
  std::vector<std::string> notes;
  if (!inferlab_artifact::extract_top_level_string_key_fallback(
          claim_scope, "mode", claim_mode) ||
      !inferlab_artifact::extract_top_level_string_key_fallback(
          claim_scope, "real_performance_claim", observed_claim) ||
      !inferlab_artifact::extract_top_level_string_key_fallback(
          claim_scope, "inferlab_role", role) ||
      !inferlab_artifact::extract_bool_key_fallback(
          claim_scope, "requires_target_environment", requires_target) ||
      !inferlab_artifact::extract_bool_key_fallback(
          claim_scope, "requires_external_product_core", requires_external) ||
      !string_array(claim_scope, "notes", notes)) {
    return false;
  }
  return claim_mode == mode &&
         observed_claim == expected_performance_claim(mode, status) &&
         requires_target == (mode == "execute") && requires_external &&
         role == kClaimRole && !notes.empty() &&
         std::all_of(notes.begin(), notes.end(), [](const auto& note) {
           return !note.empty();
         });
}

bool replay_evidence_is_consistent(
    const std::string& document,
    const std::string& mode,
    const std::string& status,
    const bool replay_metric_evidence_present) {
  const bool replay_present =
      inferlab_artifact::contains_top_level_key_fallback(document,
                                                          "replay_evidence");
  if (mode != "replay") return !replay_present;
  if (!replay_present ||
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "replay_evidence")
          .empty()) {
    return false;
  }
  const std::string metrics =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "raw_metrics");
  std::string evidence_mode;
  const bool base_contract = !metrics.empty() &&
         inferlab_artifact::extract_top_level_string_key_fallback(
             metrics, "evidence_mode", evidence_mode) &&
         evidence_mode == "offline_replay";
  return base_contract &&
         (status != "ok" || replay_metric_evidence_present);
}

bool replay_metric_evidence_present(const std::string& document) {
  const std::string metrics =
      inferlab_artifact::json_object_section_or_empty(document,
                                                       "raw_metrics");
  const std::string collected =
      inferlab_artifact::json_object_section_or_empty(metrics, "collected");
  std::size_t evidence_file_count = 0U;
  std::size_t parsed_metric_count = 0U;
  std::size_t canonical_metric_count = 0U;
  const std::string rows = inferlab_artifact::json_array_section_or_empty(
      metrics, "normalized_metric_rows");
  const auto row_values = inferlab_artifact::top_level_json_values(rows);
  return !metrics.empty() && !collected.empty() && !rows.empty() &&
         !inferlab_artifact::json_object_section_or_empty(
              collected, "metrics_by_name")
              .empty() &&
         inferlab_artifact::extract_unsigned_key_fallback(
             collected, "evidence_file_count", evidence_file_count) &&
         inferlab_artifact::extract_unsigned_key_fallback(
             collected, "parsed_metric_count", parsed_metric_count) &&
         inferlab_artifact::extract_unsigned_key_fallback(
             collected, "canonical_metric_count", canonical_metric_count) &&
         evidence_file_count > 0U && parsed_metric_count > 0U &&
         canonical_metric_count > 0U && row_values.size() == parsed_metric_count;
}

void add_check(ProductProfileArtifactContract& contract,
               const bool satisfied,
               const std::string& issue) {
  contract.required_check_count = inferlab::internal::checked_add_size(
      contract.required_check_count, 1U,
      "product profile artifact required check count");
  if (satisfied) {
    contract.satisfied_check_count = inferlab::internal::checked_add_size(
        contract.satisfied_check_count, 1U,
        "product profile artifact satisfied check count");
    return;
  }
  inferlab_plan::append_unique_nonempty_string(contract.issues, issue);
}

}  // namespace

const ProductProfileSpec& contract_profile(
    const ProductProfileArtifactContract& contract) {
  if (contract.profile == nullptr) {
    throw std::logic_error("product profile artifact contract has no profile");
  }
  return *contract.profile;
}

ProductProfileArtifactContract evaluate_product_profile_artifact_contract(
    const ProductProfileSpec& profile,
    const std::filesystem::path& artifact_path,
    const inferlab_evidence::EvidenceSnapshot& snapshot) {
  ProductProfileArtifactContract contract;
  contract.profile = &profile;
  contract.present = snapshot.read_result.present;
  contract.path_probe_status =
      inferlab_artifact::path_probe_status(artifact_path);
  contract.regular_file = contract.path_probe_status == "file";
  contract.readable = snapshot.read_result.readable;
  contract.parse_ok = snapshot.metadata.parse_ok &&
                      inferlab_artifact::json_document_is_valid_object(
                          snapshot.read_result.text);
  contract.text_read_status = snapshot.read_result.status;
  contract.text_read_error = snapshot.read_result.error;
  contract.byte_size = inferlab_artifact::file_size_or_zero(artifact_path);

  const auto& field_specs = product_profile_required_artifact_field_specs();
  contract.required_field_count = field_specs.size();
  const std::string& document = snapshot.read_result.text;
  if (contract.parse_ok) {
    for (const auto& field : field_specs) {
      if (!inferlab_artifact::contains_top_level_key_fallback(
              document, field.name)) {
        contract.missing_fields.push_back(field.name);
      } else if (!top_level_value_matches_kind(document, field)) {
        contract.invalid_type_fields.push_back(field.name);
      } else {
        contract.present_field_count = inferlab::internal::checked_add_size(
            contract.present_field_count, 1U,
            "product profile artifact present field count");
      }
    }
  } else {
    for (const auto& field : field_specs) {
      contract.missing_fields.push_back(field.name);
    }
  }

  std::string product;
  std::string product_name;
  std::string category;
  contract.product_value_present =
      contract.parse_ok &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          document, "product", product) &&
      product == profile.id;
  contract.product_name_value_present =
      contract.parse_ok &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          document, "product_name", product_name) &&
      product_name == profile.display_name;
  contract.category_value_present =
      contract.parse_ok &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          document, "category", category) &&
      category == profile.category;
  contract.schema_profile_v1 =
      contract.parse_ok && schema_contract_is_valid(document);
  contract.official_sources_present =
      contract.parse_ok && official_sources_are_valid(document);
  contract.adapter_boundary_present =
      contract.parse_ok && adapter_contract_is_valid(document, profile);
  contract.config_snapshot_present =
      contract.parse_ok &&
      !inferlab_artifact::json_object_section_or_empty(
           document, "config_snapshot")
           .empty();
  contract.planned_commands_present =
      contract.parse_ok && planned_commands_are_valid(document, profile);
  contract.execute_gate_present = contract.planned_commands_present;
  contract.raw_metrics_present =
      contract.parse_ok && raw_metrics_contract_is_valid(document);
  contract.replay_metric_evidence_present =
      contract.parse_ok && replay_metric_evidence_present(document);
  contract.not_implemented_boundary_present =
      contract.parse_ok &&
      nonempty_string_array(document, "not_implemented_by_inferlab");
  contract.local_optimization_scope_present =
      contract.parse_ok &&
      nonempty_string_array(document, "local_optimization_scope");

  contract.mode_present =
      contract.parse_ok &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          document, "mode", contract.mode) &&
      supported_mode(contract.mode);
  contract.status_present =
      contract.parse_ok &&
      inferlab_artifact::extract_top_level_string_key_fallback(
          document, "status", contract.status) &&
      !contract.status.empty();
  contract.executed_present =
      contract.parse_ok &&
      inferlab_artifact::extract_bool_key_fallback(
          document, "executed", contract.executed);
  contract.replayed_present =
      contract.parse_ok &&
      inferlab_artifact::extract_bool_key_fallback(
          document, "replayed", contract.replayed);
  contract.mode_flags_consistent =
      contract.mode_present && contract.executed_present &&
      contract.replayed_present &&
      ((contract.mode == "dry_run" && !contract.executed &&
        !contract.replayed) ||
       (contract.mode == "execute" && contract.executed &&
        !contract.replayed) ||
       (contract.mode == "replay" && !contract.executed &&
        contract.replayed));
  contract.mode_status_consistent =
      contract.mode_present && contract.status_present &&
      mode_status_consistent(contract.mode, contract.status);
  contract.claim_scope_present =
      contract.parse_ok &&
      !inferlab_artifact::json_object_section_or_empty(document,
                                                        "claim_scope")
           .empty();
  contract.claim_scope_consistent =
      contract.claim_scope_present && contract.mode_present &&
      contract.status_present &&
      claim_scope_is_consistent(document, contract.mode, contract.status,
                                contract.real_performance_claim);
  contract.replay_evidence_consistent =
      contract.mode_present && contract.status_present &&
      replay_evidence_is_consistent(
          document, contract.mode, contract.status,
          contract.replay_metric_evidence_present);
  contract.product_boundary_present =
      contract.adapter_boundary_present && contract.claim_scope_consistent;

  add_check(contract, contract.present, "artifact_missing");
  add_check(contract, contract.regular_file,
            "artifact_path_is_not_a_regular_file");
  add_check(contract, contract.readable, "artifact_unreadable_or_empty");
  add_check(contract, contract.parse_ok,
            "artifact_json_malformed_or_has_duplicate_keys");
  for (const auto& field : field_specs) {
    const bool missing =
        std::find(contract.missing_fields.begin(),
                  contract.missing_fields.end(), field.name) !=
        contract.missing_fields.end();
    const bool invalid =
        std::find(contract.invalid_type_fields.begin(),
                  contract.invalid_type_fields.end(), field.name) !=
        contract.invalid_type_fields.end();
    add_check(contract, !missing && !invalid,
              missing ? "required_field_missing:" + field.name
                      : "required_field_wrong_type:" + field.name);
  }
  add_check(contract, contract.schema_profile_v1,
            "product_profile_schema_identity_mismatch");
  add_check(contract, contract.product_value_present,
            "product_profile_product_identity_mismatch");
  add_check(contract, contract.product_name_value_present,
            "product_profile_display_name_mismatch");
  add_check(contract, contract.category_value_present,
            "product_profile_category_mismatch");
  add_check(contract, contract.official_sources_present,
            "official_sources_missing_invalid_or_empty");
  add_check(contract, contract.adapter_boundary_present,
            "inferlab_adapter_contract_mismatch");
  add_check(contract, contract.config_snapshot_present,
            "config_snapshot_missing_or_wrong_type");
  add_check(contract, contract.planned_commands_present,
            "planned_product_profile_commands_invalid");
  add_check(contract, contract.raw_metrics_present,
            "raw_metrics_contract_invalid");
  add_check(contract, contract.not_implemented_boundary_present,
            "not_implemented_boundary_missing_or_empty");
  add_check(contract, contract.local_optimization_scope_present,
            "local_optimization_scope_missing_or_empty");
  add_check(contract, contract.mode_flags_consistent,
            "mode_executed_replayed_contract_inconsistent");
  add_check(contract, contract.mode_status_consistent,
            "mode_status_contract_inconsistent");
  add_check(contract, contract.claim_scope_consistent,
            "claim_scope_contract_inconsistent");
  add_check(contract, contract.replay_evidence_consistent,
            "replay_evidence_mode_contract_inconsistent");

  if (contract.required_field_count == 0U ||
      contract.present_field_count > contract.required_field_count ||
      contract.required_check_count == 0U ||
      contract.satisfied_check_count > contract.required_check_count) {
    throw std::logic_error(
        "product profile artifact contract accounting is invalid");
  }
  contract.required_field_pass_rate =
      static_cast<double>(contract.present_field_count) /
      static_cast<double>(contract.required_field_count);
  contract.contract_coverage =
      static_cast<double>(contract.satisfied_check_count) /
      static_cast<double>(contract.required_check_count);
  const double boundary_parts =
      (contract.official_sources_present ? 1.0 : 0.0) +
      (contract.adapter_boundary_present ? 1.0 : 0.0) +
      (contract.not_implemented_boundary_present ? 1.0 : 0.0) +
      (contract.execute_gate_present ? 1.0 : 0.0) +
      (contract.product_boundary_present ? 1.0 : 0.0);
  contract.boundary_score = boundary_parts / 5.0;
  if (!std::isfinite(contract.required_field_pass_rate) ||
      !std::isfinite(contract.contract_coverage) ||
      !std::isfinite(contract.boundary_score) ||
      contract.required_field_pass_rate < 0.0 ||
      contract.required_field_pass_rate > 1.0 ||
      contract.contract_coverage < 0.0 ||
      contract.contract_coverage > 1.0 ||
      contract.boundary_score < 0.0 || contract.boundary_score > 1.0) {
    throw std::logic_error(
        "product profile artifact contract ratio is invalid");
  }
  contract.contract_ready =
      contract.required_check_count == contract.satisfied_check_count;
  contract.product_evidence_eligible =
      contract.contract_ready && contract.status == "ok" &&
      ((contract.mode == "execute" && contract.executed &&
        !contract.replayed) ||
       (contract.mode == "replay" && !contract.executed &&
        contract.replayed));
  contract.can_claim_product_performance =
      contract.product_evidence_eligible && contract.mode == "replay" &&
      contract.replay_metric_evidence_present &&
      contract.real_performance_claim ==
          "allowed_from_offline_replay_evidence";
  return contract;
}

}  // namespace inferlab::mature_product_profiles
