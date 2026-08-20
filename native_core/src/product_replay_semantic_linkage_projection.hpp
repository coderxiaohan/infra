#pragma once

#include "evidence_release_policy.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_evidence {

enum class ProductReplayLinkageSourceRole {
  SemanticEvents,
  Alignment,
};

struct ProductReplayLinkageSource {
  ProductReplayLinkageSourceRole role =
      ProductReplayLinkageSourceRole::SemanticEvents;
  std::string id;
  std::string artifact;
};

struct ProductReplayReadinessSignals {
  bool semantic_events_present = false;
  bool semantic_events_readable = false;
  std::string semantic_events_read_status = "missing";
  bool alignment_present = false;
  bool alignment_readable = false;
  std::string alignment_read_status = "missing";
  std::size_t read_issue_count = 0;
  std::size_t available_event_count = 0;
  std::size_t pending_event_count = 0;
  std::size_t replay_supported_event_count = 0;
  std::size_t alignment_rule_count = 0;
  std::size_t fully_aligned_rule_count = 0;
  std::size_t pending_rule_count = 0;
  std::size_t blocked_rule_count = 0;
  std::size_t dry_run_rule_count = 0;
  bool sample_replay_present = false;
  std::size_t blocker_count = 0;
  bool replay_ready = false;
  std::string alignment_decision = "not_reported";
  std::string status = "not_reported";
  std::string next_action;
};

struct ProductReplaySemanticLinkageSignals {
  bool semantic_events_present = false;
  bool semantic_events_readable = false;
  std::string semantic_events_read_status = "missing";
  bool alignment_present = false;
  bool alignment_readable = false;
  std::string alignment_read_status = "missing";
  std::size_t read_issue_count = 0;
  std::size_t available_event_count = 0;
  std::size_t pending_event_count = 0;
  std::size_t sample_event_count = 0;
  std::size_t replay_supported_event_count = 0;
  bool sample_replay_present = false;
  std::size_t aligned_rule_count = 0;
  std::size_t fully_aligned_rule_count = 0;
  std::size_t pending_rule_count = 0;
  std::size_t blocked_rule_count = 0;
  std::size_t dry_run_rule_count = 0;
  std::size_t alignment_rule_count = 0;
  std::string alignment_decision = "not_reported";
  std::size_t correlation_count = 0;
  std::size_t ready_correlation_count = 0;
  std::size_t blocked_correlation_count = 0;
  std::size_t blocker_count = 0;
  std::size_t target_or_validation_action_count = 0;
  std::string status = "not_reported";
  std::string first_blocker;
  std::string next_action;
};

ProductReplaySemanticLinkageSignals
project_product_replay_semantic_linkage(
    const std::filesystem::path& results_path,
    EvidenceSnapshotRepository& snapshots);

ProductReplayReadinessSignals project_product_replay_readiness(
    const std::filesystem::path& results_path,
    EvidenceSnapshotRepository& snapshots);

const char* product_replay_linkage_source_role_name(
    ProductReplayLinkageSourceRole role);
const std::vector<ProductReplayLinkageSource>&
product_replay_linkage_sources();

}  // namespace inferlab_evidence
