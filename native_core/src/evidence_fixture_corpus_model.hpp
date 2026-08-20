#pragma once

#include "evidence_domain_types.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_fixture_corpus {

struct FixtureBlocker {
  inferlab_evidence::BlockerCategory category =
      inferlab_evidence::BlockerCategory::kNone;
  std::string message;
};

struct FixtureExpectation {
  std::string confidence;
  std::string blocker_category;
  std::string release_decision;
  std::string repair_environment;
  std::string repair_action;
  std::vector<std::string> forbidden_claims;
  bool fields_valid = false;
};

struct FixtureRecord {
  std::string filename;
  std::string fixture_id;
  std::string fixture_type;
  std::string artifact_type;
  std::string mode;
  std::string claim_scope;
  std::string path_label;
  std::string path_probe_status = "not_evaluated";
  bool path_redacted = false;
  bool present = false;
  bool regular_file = false;
  bool readable = false;
  bool document_valid = false;
  bool schema_version_present = false;
  bool schema_version_valid = false;
  bool executed_present = false;
  bool replayed_present = false;
  bool executed = false;
  bool replayed = false;
  bool claim_scope_present = false;
  bool summary_present = false;
  bool summary_valid = false;
  bool blocker_count_present = false;
  std::size_t declared_blocker_count = 0U;
  std::vector<FixtureBlocker> blockers;
  FixtureExpectation expected;
  inferlab_evidence::EvidenceLevel actual_confidence =
      inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
  inferlab_evidence::BlockerCategory actual_blocker =
      inferlab_evidence::BlockerCategory::kSchemaMissing;
  inferlab_evidence::ReleaseDecision actual_release_decision =
      inferlab_evidence::ReleaseDecision::kBlocked;
  inferlab_evidence::RepairEnvironment actual_repair_environment =
      inferlab_evidence::RepairEnvironment::kCurrentRepoFixable;
  std::string actual_repair_action;
  bool contract_valid = false;
  bool confidence_matches = false;
  bool blocker_matches = false;
  bool release_decision_matches = false;
  bool repair_environment_matches = false;
  bool expectation_matches = false;
  bool claim_boundary_violation = false;
  std::vector<std::string> contract_issues;
};

struct FixtureManifest {
  std::string path_label;
  std::string path_probe_status = "not_evaluated";
  std::string corpus;
  std::string purpose;
  std::string claim_boundary;
  bool path_redacted = false;
  bool present = false;
  bool regular_file = false;
  bool readable = false;
  bool document_valid = false;
  bool schema_version_present = false;
  bool schema_version_valid = false;
  bool contract_valid = false;
  std::vector<std::string> filenames;
  std::vector<std::string> contract_issues;
};

struct FixtureCorpus {
  FixtureManifest manifest;
  std::vector<FixtureRecord> fixtures;
};

FixtureCorpus load_fixture_corpus(
    const std::filesystem::path& fixture_directory);

}  // namespace inferlab_fixture_corpus
