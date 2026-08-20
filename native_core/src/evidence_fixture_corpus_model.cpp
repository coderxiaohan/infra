#include "evidence_fixture_corpus_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace inferlab_fixture_corpus {
namespace {

namespace fs = std::filesystem;

void add_issue(std::vector<std::string>& issues, const std::string& issue) {
  if (issue.empty() ||
      std::find(issues.begin(), issues.end(), issue) != issues.end()) {
    return;
  }
  issues.push_back(issue);
}

bool valid_fixture_id(const std::string& value) {
  if (value.empty() ||
      !std::islower(static_cast<unsigned char>(value.front()))) {
    return false;
  }
  for (const char character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (!std::islower(byte) && !std::isdigit(byte) && character != '_') {
      return false;
    }
  }
  return true;
}

bool safe_fixture_filename(const std::string& value) {
  if (value.empty() || value == "fixture_manifest.json" ||
      value.find('/') != std::string::npos ||
      value.find('\\') != std::string::npos ||
      value.find("..") != std::string::npos) {
    return false;
  }
  const fs::path path(value);
  return !path.has_root_path() && path.filename() == path &&
         path.extension() == ".json" && valid_fixture_id(path.stem().string());
}

std::string symlink_probe_status(const fs::path& path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error) return "probe_error";
  return fs::is_symlink(status) ? "symlink" : "not_symlink";
}

bool required_string(const std::string& document, const std::string& key,
                     std::string& value,
                     std::vector<std::string>& issues) {
  value.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(document, key)) {
    add_issue(issues, key + "_missing");
    return false;
  }
  if (!inferlab_artifact::extract_top_level_string_key_fallback(document, key,
                                                                 value)) {
    add_issue(issues, key + "_wrong_type");
    return false;
  }
  if (value.empty()) {
    add_issue(issues, key + "_empty");
    return false;
  }
  return true;
}

bool optional_nonempty_string(const std::string& document,
                              const std::string& key, std::string& value,
                              std::vector<std::string>& issues) {
  value.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(document, key)) {
    return false;
  }
  if (!inferlab_artifact::extract_top_level_string_key_fallback(document, key,
                                                                 value)) {
    add_issue(issues, key + "_wrong_type");
    return false;
  }
  if (value.empty()) {
    add_issue(issues, key + "_empty");
    return false;
  }
  return true;
}

bool required_bool(const std::string& document, const std::string& key,
                   bool& value, std::vector<std::string>& issues) {
  if (!inferlab_artifact::contains_top_level_key_fallback(document, key)) {
    add_issue(issues, key + "_missing");
    return false;
  }
  if (!inferlab_artifact::extract_bool_key_fallback(document, key, value)) {
    add_issue(issues, key + "_wrong_type");
    return false;
  }
  return true;
}

bool schema_version_one(const std::string& document, bool& present,
                        std::vector<std::string>& issues) {
  present = inferlab_artifact::contains_top_level_key_fallback(
      document, "schema_version");
  if (!present) {
    add_issue(issues, "schema_version_missing");
    return false;
  }
  std::size_t version = 0U;
  if (!inferlab_artifact::extract_unsigned_key_fallback(
          document, "schema_version", version)) {
    add_issue(issues, "schema_version_wrong_type");
    return false;
  }
  if (version != 1U) {
    add_issue(issues, "schema_version_unsupported");
    return false;
  }
  return true;
}

void string_array(const std::string& document, const std::string& key,
                  std::vector<std::string>& values,
                  std::vector<std::string>& issues,
                  const bool require_nonempty) {
  values.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(document, key)) {
    add_issue(issues, key + "_missing");
    return;
  }
  const std::string section =
      inferlab_artifact::json_array_section_or_empty(document, key);
  if (section.empty() ||
      !inferlab_artifact::parse_string_array_items_fallback(section, values)) {
    add_issue(issues, key + "_wrong_type");
    return;
  }
  if (require_nonempty && values.empty()) {
    add_issue(issues, key + "_empty");
    return;
  }
  std::set<std::string> unique;
  for (const auto& value : values) {
    if (value.empty()) add_issue(issues, key + "_contains_empty_value");
    if (!unique.insert(value).second) {
      add_issue(issues, key + "_contains_duplicate_value");
    }
  }
}

FixtureManifest load_manifest(const fs::path& fixture_directory) {
  FixtureManifest manifest;
  const fs::path path = fixture_directory / "fixture_manifest.json";
  const std::string raw_path = path.generic_string();
  manifest.path_label = inferlab_plan::redacted_path_label(raw_path);
  manifest.path_redacted =
      inferlab_plan::path_was_redacted(raw_path, manifest.path_label);
  manifest.path_probe_status = inferlab_artifact::path_probe_status(path);
  manifest.present = manifest.path_probe_status != "missing";
  manifest.regular_file = manifest.path_probe_status == "file";
  if (!manifest.regular_file) {
    add_issue(manifest.contract_issues,
              "fixture_manifest_path_" + manifest.path_probe_status);
    return manifest;
  }
  const std::string manifest_symlink_status = symlink_probe_status(path);
  if (manifest_symlink_status != "not_symlink") {
    add_issue(manifest.contract_issues,
              "fixture_manifest_" + manifest_symlink_status +
                  "_not_allowed");
    return manifest;
  }

  const auto read_result = inferlab_artifact::read_text_with_status(path);
  manifest.readable = read_result.readable && !read_result.empty;
  if (!manifest.readable) {
    add_issue(manifest.contract_issues,
              "fixture_manifest_read_" + read_result.status);
    return manifest;
  }
  manifest.document_valid =
      inferlab_artifact::json_document_is_valid_object(read_result.text);
  if (!manifest.document_valid) {
    add_issue(manifest.contract_issues, "fixture_manifest_malformed_json");
    return manifest;
  }

  manifest.schema_version_valid = schema_version_one(
      read_result.text, manifest.schema_version_present,
      manifest.contract_issues);
  required_string(read_result.text, "corpus", manifest.corpus,
                  manifest.contract_issues);
  required_string(read_result.text, "purpose", manifest.purpose,
                  manifest.contract_issues);
  required_string(read_result.text, "claim_boundary", manifest.claim_boundary,
                  manifest.contract_issues);
  string_array(read_result.text, "fixtures", manifest.filenames,
               manifest.contract_issues, true);
  for (const auto& filename : manifest.filenames) {
    if (!safe_fixture_filename(filename)) {
      add_issue(manifest.contract_issues,
                "fixture_manifest_contains_unsafe_filename");
    }
  }
  manifest.contract_valid = manifest.contract_issues.empty();
  return manifest;
}

void load_blockers(const std::string& document, FixtureRecord& record) {
  if (!inferlab_artifact::contains_top_level_key_fallback(document,
                                                           "blockers")) {
    return;
  }
  const std::string section =
      inferlab_artifact::json_array_section_or_empty(document, "blockers");
  if (section.empty()) {
    add_issue(record.contract_issues, "blockers_wrong_type");
    return;
  }
  const auto values = inferlab_artifact::top_level_json_values(section);
  const auto objects = inferlab_artifact::top_level_json_objects(section);
  if (values.size() != objects.size()) {
    add_issue(record.contract_issues, "blockers_contains_non_object");
    return;
  }
  std::set<std::string> categories;
  for (const auto& object : objects) {
    std::string category_name;
    std::string message;
    const bool category_present = required_string(
        object, "category", category_name, record.contract_issues);
    const bool message_present =
        required_string(object, "message", message, record.contract_issues);
    inferlab_evidence::BlockerCategory category =
        inferlab_evidence::BlockerCategory::kNone;
    if (category_present &&
        !inferlab_evidence::try_parse_blocker_category(category_name,
                                                        category)) {
      add_issue(record.contract_issues, "blocker_category_unknown");
    }
    if (category == inferlab_evidence::BlockerCategory::kNone) {
      add_issue(record.contract_issues, "blocker_category_none_in_array");
    }
    if (category_present && !categories.insert(category_name).second) {
      add_issue(record.contract_issues, "duplicate_blocker_category");
    }
    if (category_present && message_present &&
        category != inferlab_evidence::BlockerCategory::kNone) {
      record.blockers.push_back({category, message});
    }
  }
}

void load_summary(const std::string& document, FixtureRecord& record,
                  bool& confidence_valid, bool& decision_valid) {
  confidence_valid = false;
  decision_valid = false;
  if (!inferlab_artifact::contains_top_level_key_fallback(document,
                                                           "summary")) {
    return;
  }
  const std::string summary =
      inferlab_artifact::json_object_section_or_empty(document, "summary");
  if (summary.empty()) {
    add_issue(record.contract_issues, "summary_wrong_type");
    return;
  }
  record.summary_present = true;
  std::string confidence;
  std::string decision;
  if (required_string(summary, "evidence_confidence", confidence,
                      record.contract_issues)) {
    confidence_valid = inferlab_evidence::try_parse_evidence_level(
        confidence, record.actual_confidence);
    if (!confidence_valid) {
      add_issue(record.contract_issues, "summary_confidence_unknown");
    }
  }
  if (required_string(summary, "release_decision", decision,
                      record.contract_issues)) {
    decision_valid = inferlab_evidence::try_parse_release_decision(
        decision, record.actual_release_decision);
    if (!decision_valid) {
      add_issue(record.contract_issues, "summary_release_decision_unknown");
    }
  }
  record.blocker_count_present =
      inferlab_artifact::contains_top_level_key_fallback(summary,
                                                          "blocker_count");
  if (record.blocker_count_present &&
      !inferlab_artifact::extract_unsigned_key_fallback(
          summary, "blocker_count", record.declared_blocker_count)) {
    add_issue(record.contract_issues, "summary_blocker_count_wrong_type");
  }
  record.summary_valid = confidence_valid && decision_valid;
}

bool parse_expectation(FixtureRecord& record) {
  inferlab_evidence::EvidenceLevel confidence =
      inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
  inferlab_evidence::BlockerCategory blocker =
      inferlab_evidence::BlockerCategory::kNone;
  inferlab_evidence::ReleaseDecision decision =
      inferlab_evidence::ReleaseDecision::kBlocked;
  const bool confidence_valid = inferlab_evidence::try_parse_evidence_level(
      record.expected.confidence, confidence);
  const bool blocker_valid = inferlab_evidence::try_parse_blocker_category(
      record.expected.blocker_category, blocker);
  const bool decision_valid = inferlab_evidence::try_parse_release_decision(
      record.expected.release_decision, decision);
  const auto repair_environment = inferlab_evidence::parse_repair_environment(
      record.expected.repair_environment);
  const bool repair_valid =
      repair_environment != inferlab_evidence::RepairEnvironment::kUnknown;
  if (!confidence_valid) {
    add_issue(record.contract_issues, "expected_confidence_unknown");
  }
  if (!blocker_valid) {
    add_issue(record.contract_issues, "expected_blocker_category_unknown");
  }
  if (!decision_valid) {
    add_issue(record.contract_issues, "expected_release_decision_unknown");
  }
  if (!repair_valid) {
    add_issue(record.contract_issues, "expected_repair_environment_unknown");
  }
  if (confidence == inferlab_evidence::EvidenceLevel::kExecuteVerified ||
      decision == inferlab_evidence::ReleaseDecision::kExecuteReady) {
    add_issue(record.contract_issues,
              "synthetic_fixture_must_not_claim_execute_evidence");
  }
  record.expected.fields_valid = confidence_valid && blocker_valid &&
                                 decision_valid && repair_valid &&
                                 !record.expected.repair_action.empty() &&
                                 !record.expected.forbidden_claims.empty();
  return record.expected.fields_valid;
}

void validate_summary_semantics(FixtureRecord& record,
                                const bool confidence_valid,
                                const bool decision_valid) {
  if (!record.summary_present || !confidence_valid || !decision_valid) return;

  const bool state_is_blocked =
      !record.claim_scope_present || !record.blockers.empty();
  if (state_is_blocked &&
      record.actual_release_decision !=
          inferlab_evidence::ReleaseDecision::kBlocked) {
    add_issue(record.contract_issues,
              "summary_release_decision_ignores_blocker");
  }
  if (!state_is_blocked &&
      record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kBlocked) {
    add_issue(record.contract_issues,
              "summary_blocked_without_represented_blocker");
  }

  if (record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kReportReady &&
      record.actual_confidence !=
          inferlab_evidence::EvidenceLevel::kDryRunReady) {
    add_issue(record.contract_issues,
              "summary_report_ready_confidence_mismatch");
  }
  if (record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kReplayReady &&
      record.actual_confidence !=
          inferlab_evidence::EvidenceLevel::kReplaySupported) {
    add_issue(record.contract_issues,
              "summary_replay_ready_confidence_mismatch");
  }
  if (record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kExecuteReady ||
      record.actual_confidence ==
          inferlab_evidence::EvidenceLevel::kExecuteVerified) {
    add_issue(record.contract_issues,
              "synthetic_fixture_must_not_claim_execute_evidence");
  }
  if (record.mode == "dry_run" &&
      (record.actual_confidence ==
           inferlab_evidence::EvidenceLevel::kReplaySupported ||
       record.actual_confidence ==
           inferlab_evidence::EvidenceLevel::kExecuteVerified)) {
    add_issue(record.contract_issues,
              "dry_run_fixture_summary_confidence_mismatch");
  }
  if (record.mode == "replay" &&
      record.actual_confidence !=
          inferlab_evidence::EvidenceLevel::kReplaySupported) {
    add_issue(record.contract_issues,
              "replay_fixture_summary_confidence_mismatch");
  }
}

void derive_actual_state(FixtureRecord& record) {
  if (!record.contract_valid) {
    record.actual_confidence =
        inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
    record.actual_blocker =
        inferlab_evidence::BlockerCategory::kSchemaMissing;
    record.actual_release_decision =
        inferlab_evidence::ReleaseDecision::kBlocked;
  } else if (!record.summary_present) {
    record.actual_confidence = inferlab_evidence::EvidenceLevel::kSchemaOnly;
    record.actual_blocker =
        inferlab_evidence::BlockerCategory::kSummaryMissing;
    record.actual_release_decision =
        inferlab_evidence::ReleaseDecision::kBlocked;
  } else if (!record.claim_scope_present) {
    record.actual_blocker =
        inferlab_evidence::BlockerCategory::kClaimScopeMissing;
  } else if (!record.blockers.empty()) {
    record.actual_blocker = record.blockers.front().category;
  } else {
    record.actual_blocker = inferlab_evidence::BlockerCategory::kNone;
  }

  const std::string actual_blocker_name =
      inferlab_evidence::blocker_category_name(record.actual_blocker);
  if (record.actual_blocker == inferlab_evidence::BlockerCategory::kNone &&
      record.mode == "replay" && record.replayed) {
    record.actual_repair_environment =
        inferlab_evidence::RepairEnvironment::kTargetProductOrReplayEvidence;
    record.actual_repair_action =
        "attach real replay evidence before product behavior claims";
  } else {
    record.actual_repair_environment =
        inferlab_evidence::repair_environment_kind(actual_blocker_name);
    record.actual_repair_action =
        inferlab_evidence::repair_action_for_blocker(actual_blocker_name);
  }

  record.confidence_matches =
      record.expected.confidence ==
      inferlab_evidence::evidence_level_name(record.actual_confidence);
  record.blocker_matches =
      record.expected.blocker_category == actual_blocker_name;
  if (record.expected.blocker_category == "claim_scope_missing" &&
      record.claim_scope_present) {
    record.blocker_matches = false;
  }
  if (record.expected.blocker_category == "summary_missing" &&
      record.summary_present) {
    record.blocker_matches = false;
  }
  record.release_decision_matches =
      record.expected.release_decision ==
      inferlab_evidence::release_decision_name(
          record.actual_release_decision);
  record.repair_environment_matches =
      record.expected.repair_environment ==
      inferlab_evidence::repair_environment_name(
          record.actual_repair_environment);

  std::set<std::string> enabled_claims = {
      inferlab_evidence::evidence_level_name(record.actual_confidence),
      inferlab_evidence::release_decision_name(
          record.actual_release_decision)};
  if (record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kReplayReady ||
      record.actual_release_decision ==
          inferlab_evidence::ReleaseDecision::kExecuteReady) {
    enabled_claims.insert("report_ready");
  }
  if (record.actual_release_decision ==
      inferlab_evidence::ReleaseDecision::kExecuteReady) {
    enabled_claims.insert("release_ready");
    enabled_claims.insert("execute_verified");
  }
  record.claim_boundary_violation = record.executed;
  for (const auto& forbidden : record.expected.forbidden_claims) {
    if (enabled_claims.count(forbidden) != 0U) {
      record.claim_boundary_violation = true;
    }
  }
  record.expectation_matches =
      record.expected.fields_valid && record.confidence_matches &&
      record.blocker_matches && record.release_decision_matches &&
      record.repair_environment_matches;
}

FixtureRecord load_fixture(const fs::path& fixture_directory,
                           const std::string& filename) {
  FixtureRecord record;
  record.filename = filename;
  if (!safe_fixture_filename(filename)) {
    record.path_label = "unsafe_fixture_path";
    record.path_probe_status = "unsafe";
    add_issue(record.contract_issues, "unsafe_fixture_filename");
    derive_actual_state(record);
    return record;
  }

  const fs::path path = fixture_directory / filename;
  const std::string raw_path = path.generic_string();
  record.path_label = inferlab_plan::redacted_path_label(raw_path);
  record.path_redacted =
      inferlab_plan::path_was_redacted(raw_path, record.path_label);
  record.path_probe_status = inferlab_artifact::path_probe_status(path);
  record.present = record.path_probe_status != "missing";
  record.regular_file = record.path_probe_status == "file";
  if (!record.regular_file) {
    add_issue(record.contract_issues,
              "fixture_path_" + record.path_probe_status);
    derive_actual_state(record);
    return record;
  }
  const std::string fixture_symlink_status = symlink_probe_status(path);
  if (fixture_symlink_status != "not_symlink") {
    add_issue(record.contract_issues,
              "fixture_" + fixture_symlink_status + "_not_allowed");
    derive_actual_state(record);
    return record;
  }

  const auto read_result = inferlab_artifact::read_text_with_status(path);
  record.readable = read_result.readable && !read_result.empty;
  if (!record.readable) {
    add_issue(record.contract_issues, "fixture_read_" + read_result.status);
    derive_actual_state(record);
    return record;
  }
  record.document_valid =
      inferlab_artifact::json_document_is_valid_object(read_result.text);
  if (!record.document_valid) {
    add_issue(record.contract_issues, "fixture_malformed_json");
    derive_actual_state(record);
    return record;
  }

  record.schema_version_valid = schema_version_one(
      read_result.text, record.schema_version_present,
      record.contract_issues);
  required_string(read_result.text, "fixture_id", record.fixture_id,
                  record.contract_issues);
  required_string(read_result.text, "fixture_type", record.fixture_type,
                  record.contract_issues);
  required_string(read_result.text, "artifact_type", record.artifact_type,
                  record.contract_issues);
  required_string(read_result.text, "mode", record.mode,
                  record.contract_issues);
  record.executed_present = required_bool(
      read_result.text, "executed", record.executed, record.contract_issues);
  record.replayed_present = required_bool(
      read_result.text, "replayed", record.replayed, record.contract_issues);
  record.claim_scope_present = optional_nonempty_string(
      read_result.text, "claim_scope", record.claim_scope,
      record.contract_issues);

  required_string(read_result.text, "expected_confidence",
                  record.expected.confidence, record.contract_issues);
  required_string(read_result.text, "expected_blocker_category",
                  record.expected.blocker_category, record.contract_issues);
  required_string(read_result.text, "expected_release_decision",
                  record.expected.release_decision, record.contract_issues);
  required_string(read_result.text, "expected_repair_environment",
                  record.expected.repair_environment, record.contract_issues);
  required_string(read_result.text, "expected_repair_action",
                  record.expected.repair_action, record.contract_issues);
  string_array(read_result.text, "forbidden_claims",
               record.expected.forbidden_claims, record.contract_issues, true);

  if (!valid_fixture_id(record.fixture_id)) {
    add_issue(record.contract_issues, "fixture_id_invalid");
  }
  if (record.fixture_id != path.stem().string()) {
    add_issue(record.contract_issues, "fixture_id_filename_mismatch");
  }
  if (record.fixture_type != record.fixture_id) {
    add_issue(record.contract_issues, "fixture_type_id_mismatch");
  }
  if (record.artifact_type != "fixture_" + record.fixture_id) {
    add_issue(record.contract_issues, "fixture_artifact_type_mismatch");
  }
  if (record.executed) {
    add_issue(record.contract_issues,
              "synthetic_fixture_executed_must_be_false");
  }

  bool summary_confidence_valid = false;
  bool summary_decision_valid = false;
  load_summary(read_result.text, record, summary_confidence_valid,
               summary_decision_valid);
  load_blockers(read_result.text, record);
  parse_expectation(record);

  if (record.mode == "replay") {
    if (!record.replayed || record.expected.confidence != "replay_supported" ||
        record.expected.release_decision != "replay_ready") {
      add_issue(record.contract_issues, "replay_fixture_mode_mismatch");
    }
  } else if (record.mode == "dry_run") {
    if (record.replayed) {
      add_issue(record.contract_issues, "dry_run_fixture_replayed_true");
    }
  } else {
    add_issue(record.contract_issues, "fixture_mode_unsupported");
  }

  validate_summary_semantics(record, summary_confidence_valid,
                             summary_decision_valid);

  if (!record.blockers.empty() && !record.blocker_count_present) {
    add_issue(record.contract_issues, "summary_blocker_count_missing");
  }
  if (record.blocker_count_present &&
      record.declared_blocker_count != record.blockers.size()) {
    add_issue(record.contract_issues, "summary_blocker_count_mismatch");
  }
  record.contract_valid = record.contract_issues.empty();
  derive_actual_state(record);
  return record;
}

}  // namespace

FixtureCorpus load_fixture_corpus(const fs::path& fixture_directory) {
  FixtureCorpus corpus;
  corpus.manifest = load_manifest(fixture_directory);
  corpus.fixtures.reserve(corpus.manifest.filenames.size());
  for (const auto& filename : corpus.manifest.filenames) {
    corpus.fixtures.push_back(load_fixture(fixture_directory, filename));
  }
  return corpus;
}

}  // namespace inferlab_fixture_corpus
