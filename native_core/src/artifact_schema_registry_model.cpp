#include "artifact_schema_registry_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "catalog_derived_views.hpp"
#include "evidence_query_cli_contract.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::artifact_schema_registry {
namespace {

constexpr std::size_t kMaxRegistryConfigBytes = 1024U * 1024U;

const char* kArtifactSchemaConfig =
    "configs/schemas/artifact_schema_registry.json";
const char* kConfidenceConfig =
    "configs/schemas/evidence_confidence_levels.json";
const char* kClaimBoundaryConfig =
    "configs/schemas/claim_boundary_rules.json";
const char* kReleaseDecisionConfig =
    "configs/schemas/release_decision_rules.json";
const char* kSelectorExportSchema =
    "configs/schemas/evidence_query_selector_repair_card_export.json";

std::string read_config(const std::filesystem::path& repo_root,
                        const char* relative_path,
                        const char* source_name) {
  const auto path = inferlab_plan::resolve_repo_relative_path(
      repo_root, relative_path);
  const auto read_result = inferlab_artifact::read_text_with_status(
      path, kMaxRegistryConfigBytes);
  if (!read_result.readable ||
      !inferlab_artifact::json_document_is_valid_object(
          read_result.text)) {
    throw std::runtime_error(
        std::string("artifact schema registry ") + source_name +
        " config is missing, unreadable, oversized, or malformed");
  }
  std::size_t schema_version = 0U;
  if (!inferlab_artifact::extract_unsigned_key_fallback(
          read_result.text, "schema_version", schema_version) ||
      schema_version != 1U) {
    throw std::runtime_error(
        std::string("artifact schema registry ") + source_name +
        " schema_version must equal 1");
  }
  return read_result.text;
}

std::vector<std::string> required_string_array(
    const std::string& document, const char* key,
    const char* source_name) {
  std::vector<std::string> values;
  if (!inferlab_artifact::parse_string_array_items_fallback(
          inferlab_artifact::json_array_section_or_empty(document, key),
          values) ||
      values.empty()) {
    throw std::runtime_error(
        std::string("artifact schema registry ") + source_name +
        " requires a nonempty " + key + " string array");
  }
  return values;
}

std::vector<std::string> required_object_array(
    const std::string& document, const char* key,
    const char* source_name) {
  const auto section =
      inferlab_artifact::json_array_section_or_empty(document, key);
  const auto raw_values =
      inferlab_artifact::top_level_json_values(section);
  const auto values =
      inferlab_artifact::top_level_json_objects(section);
  if (values.empty() || values.size() != raw_values.size()) {
    throw std::runtime_error(
        std::string("artifact schema registry ") + source_name +
        " requires a nonempty " + key + " object array");
  }
  return values;
}

std::string required_string(const std::string& object, const char* key,
                            const char* source_name) {
  std::string value;
  if (!inferlab_artifact::extract_top_level_string_key_fallback(
          object, key, value) ||
      value.empty()) {
    throw std::runtime_error(
        std::string("artifact schema registry ") + source_name +
        " object requires nonempty " + key);
  }
  return value;
}

std::vector<ConfidenceSpec> parse_confidence_levels(
    const std::string& document) {
  std::vector<ConfidenceSpec> specs;
  for (const auto& object :
       required_object_array(document, "levels", "confidence")) {
    const auto name = required_string(object, "level", "confidence");
    inferlab_evidence::EvidenceLevel level =
        inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
    if (!inferlab_evidence::try_parse_evidence_level(name, level)) {
      throw std::runtime_error(
          "artifact schema registry confidence level is unknown");
    }
    specs.push_back(
        {level, required_string(object, "meaning", "confidence")});
  }
  return specs;
}

std::vector<ClaimBoundaryRule> parse_claim_rules(
    const std::string& document) {
  std::vector<ClaimBoundaryRule> rules;
  for (const auto& object :
       required_object_array(document, "rules", "claim-boundary")) {
    rules.push_back(
        {required_string(object, "rule", "claim-boundary"),
         required_string(object, "decision", "claim-boundary")});
  }
  return rules;
}

std::vector<ReleaseDecisionRule> parse_release_rules(
    const std::string& document) {
  std::vector<ReleaseDecisionRule> rules;
  for (const auto& object :
       required_object_array(document, "decisions", "release-decision")) {
    const auto name =
        required_string(object, "decision", "release-decision");
    inferlab_evidence::ReleaseDecision decision =
        inferlab_evidence::ReleaseDecision::kBlocked;
    if (!inferlab_evidence::try_parse_release_decision(name, decision)) {
      throw std::runtime_error(
          "artifact schema registry release decision is unknown");
    }
    rules.push_back(
        {decision,
         required_string(object, "requires", "release-decision")});
  }
  return rules;
}

template <typename Value, typename Name>
void require_unique_names(const std::vector<Value>& values, Name name,
                          const char* label) {
  std::unordered_set<std::string> names;
  names.reserve(values.size());
  for (const auto& value : values) {
    const std::string identity = name(value);
    if (identity.empty() || !names.insert(identity).second) {
      throw std::logic_error(
          std::string("artifact schema registry ") + label +
          " identities must be nonempty and unique");
    }
  }
}

void validate_registry_files(const std::vector<std::string>& files) {
  require_unique_names(
      files, [](const auto& value) { return value; }, "schema-file");
  constexpr char kPrefix[] = "configs/schemas/";
  constexpr char kSuffix[] = ".json";
  for (const auto& file : files) {
    if (file.rfind(kPrefix, 0U) != 0U ||
        file.size() <= sizeof(kSuffix) - 1U ||
        file.compare(file.size() - (sizeof(kSuffix) - 1U),
                     sizeof(kSuffix) - 1U, kSuffix) != 0 ||
        file.find("..") != std::string::npos) {
      throw std::logic_error(
          "artifact schema registry file identity is unsafe");
    }
  }
}

void validate_confidence_levels(
    const std::vector<ConfidenceSpec>& levels) {
  require_unique_names(
      levels,
      [](const auto& value) {
        return std::string(
            inferlab_evidence::evidence_level_name(value.level));
      },
      "confidence");
  constexpr inferlab_evidence::EvidenceLevel kRequired[] = {
      inferlab_evidence::EvidenceLevel::kSchemaOnly,
      inferlab_evidence::EvidenceLevel::kDryRunReady,
      inferlab_evidence::EvidenceLevel::kReplaySupported,
      inferlab_evidence::EvidenceLevel::kExecuteVerified,
      inferlab_evidence::EvidenceLevel::kStaleOrBlocked,
  };
  if (levels.size() != sizeof(kRequired) / sizeof(kRequired[0])) {
    throw std::logic_error(
        "artifact schema registry confidence vocabulary is incomplete");
  }
  for (const auto required : kRequired) {
    const auto found = std::find_if(
        levels.begin(), levels.end(),
        [required](const auto& spec) {
          return spec.level == required && !spec.meaning.empty();
        });
    if (found == levels.end()) {
      throw std::logic_error(
          "artifact schema registry confidence vocabulary is incomplete");
    }
  }
}

void validate_release_rules(
    const std::vector<ReleaseDecisionRule>& rules) {
  require_unique_names(
      rules,
      [](const auto& value) {
        return std::string(
            inferlab_evidence::release_decision_name(value.decision));
      },
      "release-decision");
  constexpr inferlab_evidence::ReleaseDecision kRequired[] = {
      inferlab_evidence::ReleaseDecision::kBlocked,
      inferlab_evidence::ReleaseDecision::kReportReady,
      inferlab_evidence::ReleaseDecision::kReplayReady,
      inferlab_evidence::ReleaseDecision::kExecuteReady,
  };
  if (rules.size() != sizeof(kRequired) / sizeof(kRequired[0])) {
    throw std::logic_error(
        "artifact schema registry release vocabulary is incomplete");
  }
  for (const auto required : kRequired) {
    const auto found = std::find_if(
        rules.begin(), rules.end(),
        [required](const auto& rule) {
          return rule.decision == required && !rule.requires.empty();
        });
    if (found == rules.end()) {
      throw std::logic_error(
          "artifact schema registry release vocabulary is incomplete");
    }
  }
}

void validate_artifacts(
    const std::vector<const inferlab_high_value::ManagedArtifact*>&
        artifacts) {
  const auto& canonical = inferlab_high_value::managed_artifacts();
  if (artifacts.size() != canonical.size()) {
    throw std::logic_error(
        "artifact schema registry managed artifact count drifted");
  }
  for (std::size_t i = 0U; i < artifacts.size(); ++i) {
    if (artifacts[i] == nullptr || artifacts[i] != &canonical[i]) {
      throw std::logic_error(
          "artifact schema registry must borrow canonical catalog rows");
    }
    (void)inferlab_catalog::registry_artifact_type(*artifacts[i]);
    (void)inferlab_catalog::registry_report_kind(*artifacts[i]);
    (void)inferlab_catalog::registry_summary_group(*artifacts[i]);
    (void)inferlab_catalog::registry_final_report_group(*artifacts[i]);
    (void)inferlab_catalog::registry_prometheus_category(*artifacts[i]);
    (void)inferlab_catalog::registry_required_fields(*artifacts[i]);
    (void)inferlab_catalog::registry_required_metrics(*artifacts[i]);
    (void)inferlab_catalog::registry_claim_boundary(*artifacts[i]);
    (void)inferlab_catalog::registry_freshness_policy(*artifacts[i]);
  }
}

}  // namespace

RegistryPlan build_plan(const std::filesystem::path& repo_root) {
  inferlab_high_value::validate_managed_artifact_registry();
  RegistryPlan plan;
  for (const auto& artifact :
       inferlab_high_value::managed_artifacts()) {
    plan.artifacts.push_back(&artifact);
  }

  const auto schema_config =
      read_config(repo_root, kArtifactSchemaConfig, "schema");
  plan.registry_files = required_string_array(
      schema_config, "registry_files", "schema");
  plan.optional_fields = required_string_array(
      schema_config, "default_optional_fields", "schema");
  plan.confidence_levels = parse_confidence_levels(
      read_config(repo_root, kConfidenceConfig, "confidence"));
  plan.claim_boundary_rules = parse_claim_rules(
      read_config(repo_root, kClaimBoundaryConfig, "claim-boundary"));
  plan.release_decision_rules = parse_release_rules(
      read_config(repo_root, kReleaseDecisionConfig, "release-decision"));

  const auto& self = inferlab_high_value::require_managed_artifact(
      "artifact_schema_registry");
  const auto& release_pipeline =
      inferlab_high_value::require_managed_artifact(
          "release_evidence_pipeline");
  plan.recommended_next_commands = {
      self.command,
      inferlab_evidence_query_cli::
          selector_repair_card_export_command(),
      release_pipeline.command,
  };
  plan.selector_export_schema_registered =
      std::find(plan.registry_files.begin(), plan.registry_files.end(),
                kSelectorExportSchema) != plan.registry_files.end();
  validate_plan(plan);
  return plan;
}

void validate_plan(const RegistryPlan& plan) {
  validate_artifacts(plan.artifacts);
  validate_registry_files(plan.registry_files);
  require_unique_names(
      plan.optional_fields,
      [](const auto& value) { return value; }, "optional-field");
  validate_confidence_levels(plan.confidence_levels);
  require_unique_names(
      plan.claim_boundary_rules,
      [](const auto& value) { return value.rule; }, "claim-boundary-rule");
  for (const auto& rule : plan.claim_boundary_rules) {
    if (rule.decision.empty()) {
      throw std::logic_error(
          "artifact schema registry claim rule decision is empty");
    }
  }
  validate_release_rules(plan.release_decision_rules);
  require_unique_names(
      plan.recommended_next_commands,
      [](const auto& value) { return value; }, "recommended-command");
  const bool selector_registered =
      std::find(plan.registry_files.begin(), plan.registry_files.end(),
                kSelectorExportSchema) != plan.registry_files.end();
  if (plan.selector_export_schema_registered != selector_registered) {
    throw std::logic_error(
        "artifact schema registry selector schema summary drifted");
  }
}

}  // namespace inferlab::artifact_schema_registry
