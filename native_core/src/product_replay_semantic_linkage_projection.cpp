#include "product_replay_semantic_linkage_projection.hpp"

#include "artifact_contract_model.hpp"
#include "artifact_field_projection.hpp"
#include "checked_index.hpp"
#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_evidence {
namespace {

namespace fs = std::filesystem;

ProductReplayLinkageSource legacy_source(
    const ProductReplayLinkageSourceRole role,
    const std::string& contract_name) {
  const auto& contract =
      inferlab_artifact_contract::require_contract_extension(contract_name);
  ProductReplayLinkageSource source;
  source.role = role;
  source.id = contract.name;
  source.artifact = fs::path(contract.artifact).filename().generic_string();
  return source;
}

void validate_linkage_sources(
    const std::vector<ProductReplayLinkageSource>& sources) {
  constexpr std::size_t kExpectedSourceCount = 2U;
  if (sources.size() != kExpectedSourceCount) {
    throw std::logic_error(
        "product replay semantic linkage source count drift");
  }
  for (std::size_t index = 0U; index < sources.size(); ++index) {
    const auto& source = sources[index];
    if (source.id.empty() || source.artifact.empty() ||
        source.artifact != fs::path(source.artifact).filename().generic_string() ||
        fs::path(source.artifact).extension() != ".json") {
      throw std::logic_error(
          "product replay semantic linkage source identity is invalid");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (sources[prior].id == source.id ||
          sources[prior].artifact == source.artifact ||
          sources[prior].role == source.role) {
        throw std::logic_error(
            "product replay semantic linkage source identity is duplicated");
      }
    }
  }
}

const ProductReplayLinkageSource& require_linkage_source(
    const ProductReplayLinkageSourceRole role) {
  const ProductReplayLinkageSource* match = nullptr;
  for (const auto& source : product_replay_linkage_sources()) {
    if (source.role != role) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "product replay semantic linkage role is ambiguous");
    }
    match = &source;
  }
  if (match == nullptr) {
    throw std::logic_error(
        "product replay semantic linkage role is missing");
  }
  return *match;
}

struct StrictSummarySource {
  std::string summary;
  bool usable = false;
  bool contract_issue = false;
};

StrictSummarySource select_strict_summary(const EvidenceSnapshot& snapshot) {
  StrictSummarySource source;
  if (!snapshot.metadata.present || !snapshot.read_result.readable ||
      !snapshot.metadata.parse_ok) {
    return source;
  }
  const auto selected =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          snapshot.read_result.text, "summary");
  source.usable = selected.usable;
  source.contract_issue = !selected.usable;
  source.summary = selected.usable ? selected.text : "";
  return source;
}

std::size_t required_count(const std::string& source, const char* key,
                           bool& contract_issue) {
  const auto field = inferlab_artifact::project_unsigned_field(source, {key});
  contract_issue = contract_issue || !field.present || field.invalid;
  return field.value;
}

bool required_bool(const std::string& source, const char* key,
                   bool& contract_issue) {
  const auto field = inferlab_artifact::project_bool_field(source, {key});
  contract_issue = contract_issue || !field.present || field.invalid;
  return field.value;
}

std::string required_string(const std::string& source, const char* key,
                            bool& contract_issue) {
  const auto field = inferlab_artifact::project_string_field(source, {key});
  contract_issue = contract_issue || !field.present || field.invalid ||
                   field.value.empty();
  return field.value;
}

std::size_t source_issue_count(const EvidenceSnapshot& snapshot,
                               const bool contract_issue) {
  const bool physical_or_parse_issue =
      inferlab_artifact::text_read_has_issue(snapshot.read_result) ||
      (snapshot.metadata.present && !snapshot.metadata.parse_ok);
  return physical_or_parse_issue || contract_issue ? 1U : 0U;
}

void project_semantic_summary(const StrictSummarySource& source,
                              ProductReplaySemanticLinkageSignals& signals,
                              bool& contract_issue) {
  if (!source.usable) return;
  signals.available_event_count =
      required_count(source.summary, "available_event_count", contract_issue);
  signals.pending_event_count =
      required_count(source.summary, "pending_event_count", contract_issue);
  signals.sample_event_count =
      required_count(source.summary, "sample_event_count", contract_issue);
  signals.replay_supported_event_count = required_count(
      source.summary, "replay_supported_event_count", contract_issue);
  signals.sample_replay_present =
      required_bool(source.summary, "sample_replay_present", contract_issue);
}

void project_alignment_summary(const StrictSummarySource& source,
                               ProductReplaySemanticLinkageSignals& signals,
                               bool& contract_issue) {
  if (!source.usable) return;
  signals.aligned_rule_count =
      required_count(source.summary, "aligned_rule_count", contract_issue);
  signals.fully_aligned_rule_count = required_count(
      source.summary, "fully_aligned_rule_count", contract_issue);
  signals.pending_rule_count =
      required_count(source.summary, "pending_rule_count", contract_issue);
  signals.blocked_rule_count =
      required_count(source.summary, "blocked_rule_count", contract_issue);
  signals.dry_run_rule_count =
      required_count(source.summary, "dry_run_rule_count", contract_issue);
  signals.alignment_rule_count =
      required_count(source.summary, "rule_count", contract_issue);
  signals.alignment_decision = required_string(
      source.summary, "alignment_decision", contract_issue);
}

ArtifactContractPolicy structural_contract_policy() {
  ArtifactContractPolicy policy;
  policy.require_status_ok = false;
  policy.require_clear_blockers = false;
  return policy;
}

const inferlab_high_value::ManagedArtifact& require_rule_native_artifact(
    const std::string& filename) {
  const inferlab_high_value::ManagedArtifact* match = nullptr;
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    if (artifact.artifact != filename) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "product replay correlation native artifact is ambiguous: " +
          filename);
    }
    match = &artifact;
  }
  if (match == nullptr) {
    throw std::logic_error(
        "product replay correlation native artifact is not catalog managed: " +
        filename);
  }
  return *match;
}

void project_correlation_rules(
    const fs::path& results_path,
    const ProductReplayLinkageSource& semantic_source,
    const EvidenceSnapshot& semantic_snapshot,
    EvidenceSnapshotRepository& snapshots,
    ProductReplaySemanticLinkageSignals& signals) {
  const auto& rules = inferlab_high_value::correlation_rules();
  inferlab_high_value::validate_correlation_rule_registry(rules);
  const auto semantic_contract = evaluate_artifact_contract(
      semantic_snapshot, structural_contract_policy());
  signals.correlation_count = rules.size();
  for (const auto& rule : rules) {
    if (rule.product_source != semantic_source.artifact) {
      throw std::logic_error(
          "product replay correlation rule uses a non-canonical semantic source");
    }
    const auto& native_artifact =
        require_rule_native_artifact(rule.native_artifact);
    const auto& native_snapshot =
        snapshots.get(results_path / native_artifact.artifact);
    const auto native_contract = evaluate_artifact_contract(
        native_snapshot, structural_contract_policy());
    if (semantic_contract.ready && native_contract.ready) {
      signals.ready_correlation_count = inferlab::internal::checked_add_size(
          signals.ready_correlation_count, 1U,
          "product replay semantic linkage ready correlation count");
    } else {
      signals.blocked_correlation_count = inferlab::internal::checked_add_size(
          signals.blocked_correlation_count, 1U,
          "product replay semantic linkage blocked correlation count");
    }
  }
  if (inferlab::internal::checked_sum_size(
          {signals.ready_correlation_count,
           signals.blocked_correlation_count},
          "product replay semantic linkage classified correlation count") !=
      signals.correlation_count) {
    throw std::logic_error(
        "product replay semantic linkage correlation accounting drift");
  }
}

void derive_linkage_status(ProductReplaySemanticLinkageSignals& signals) {
  if (!signals.semantic_events_present) {
    signals.status = "semantic_events_missing";
    signals.first_blocker = "product_replay_semantic_events.json missing";
    signals.next_action =
        "refresh product replay semantic events after importing replay samples";
  } else if (!signals.alignment_present) {
    signals.status = "alignment_missing";
    signals.first_blocker = "native_product_replay_alignment.json missing";
    signals.next_action =
        "refresh product replay alignment after semantic events are available";
  } else if (signals.read_issue_count > 0) {
    signals.status = "linkage_unreadable";
    signals.first_blocker =
        "one or more product replay linkage artifacts are unreadable or violate their summary contract";
    signals.next_action =
        "repair unreadable or contract-invalid product replay linkage artifacts";
  } else if (signals.blocked_correlation_count > 0) {
    signals.status = "correlation_blocked";
    signals.first_blocker = "product replay semantic correlations are blocked";
    signals.next_action =
        "refresh contract-valid native explanation artifacts referenced by the semantic correlation registry";
  } else if (signals.blocked_rule_count > 0U) {
    signals.status = "alignment_blocked";
    signals.first_blocker = "product replay alignment rules are blocked";
    signals.next_action = "repair blocked product replay alignment rules";
  } else if (signals.pending_rule_count > 0 ||
             signals.pending_event_count > 0) {
    signals.status = "replay_evidence_pending";
    signals.first_blocker =
        "product replay semantic events or alignment rules are pending";
    signals.next_action =
        "import product replay evidence and refresh semantic/alignment artifacts";
  } else if (signals.dry_run_rule_count > 0U ||
             signals.alignment_rule_count == 0U ||
             signals.alignment_decision != "aligned_explanation_ready" ||
             signals.fully_aligned_rule_count !=
                 signals.alignment_rule_count) {
    signals.status = "alignment_incomplete";
    signals.first_blocker =
        "product replay alignment is not fully covered by replay or execute evidence";
    signals.next_action =
        "complete product evidence coverage for every alignment rule";
  } else {
    signals.status = "semantic_linkage_ready";
    signals.next_action = "product replay semantic linkage is ready";
  }
}

void derive_readiness_status(ProductReplayReadinessSignals& signals) {
  if (!signals.semantic_events_present) {
    signals.status = "semantic_events_missing";
    signals.next_action =
        "refresh product replay semantic events after importing replay samples";
  } else if (!signals.semantic_events_readable) {
    signals.status = "semantic_events_unreadable";
    signals.next_action =
        "repair the product replay semantic-events artifact contract";
  } else if (!signals.alignment_present) {
    signals.status = "alignment_missing";
    signals.next_action =
        "refresh product replay alignment after semantic events are available";
  } else if (!signals.alignment_readable) {
    signals.status = "alignment_unreadable";
    signals.next_action =
        "repair the product replay alignment artifact contract";
  } else if (signals.read_issue_count > 0U) {
    signals.status = "replay_contract_invalid";
    signals.next_action =
        "repair malformed replay semantic or alignment summary fields";
  } else if (signals.blocked_rule_count > 0U) {
    signals.status = "alignment_blocked";
    signals.next_action =
        "repair blocked product replay alignment rules";
  } else if (signals.pending_event_count > 0U ||
             signals.pending_rule_count > 0U) {
    signals.status = "replay_evidence_pending";
    signals.next_action =
        "import product replay evidence and refresh semantic events plus alignment";
  } else if (signals.dry_run_rule_count > 0U) {
    signals.status = "alignment_dry_run_only";
    signals.next_action =
        "replace dry-run product inputs with replay or execute evidence and refresh alignment";
  } else if (signals.alignment_rule_count == 0U) {
    signals.status = "alignment_rules_missing";
    signals.next_action =
        "restore the canonical product replay alignment rule inventory";
  } else if (signals.alignment_decision != "aligned_explanation_ready" ||
             signals.fully_aligned_rule_count !=
                 signals.alignment_rule_count) {
    signals.status = "alignment_incomplete";
    signals.next_action =
        "complete product evidence coverage for every alignment rule";
  } else if (!signals.sample_replay_present) {
    signals.status = "sample_replay_missing";
    signals.next_action =
        "import an authorized replay sample and refresh product replay semantic events";
  } else if (signals.available_event_count == 0U) {
    signals.status = "replay_events_missing";
    signals.next_action =
        "refresh product replay semantic events with normalized replay events";
  } else if (signals.replay_supported_event_count == 0U) {
    signals.status = "replay_support_missing";
    signals.next_action =
        "add replay-supported semantic mappings and refresh replay evidence";
  } else {
    signals.status = "replay_evidence_ready";
    signals.next_action =
        "retain artifact-scoped replay evidence and refresh downstream reports";
  }
  signals.replay_ready = signals.blocker_count == 0U;
}

}  // namespace

const char* product_replay_linkage_source_role_name(
    const ProductReplayLinkageSourceRole role) {
  switch (role) {
    case ProductReplayLinkageSourceRole::SemanticEvents:
      return "semantic_events";
    case ProductReplayLinkageSourceRole::Alignment:
      return "alignment";
  }
  throw std::invalid_argument(
      "unknown product replay semantic linkage source role");
}

const std::vector<ProductReplayLinkageSource>&
product_replay_linkage_sources() {
  static const std::vector<ProductReplayLinkageSource> sources = {
      legacy_source(ProductReplayLinkageSourceRole::SemanticEvents,
                    "product_replay_semantic_events"),
      legacy_source(ProductReplayLinkageSourceRole::Alignment,
                    "product_replay_alignment")};
  static const bool validated = [] {
    validate_linkage_sources(sources);
    return true;
  }();
  (void)validated;
  return sources;
}

ProductReplaySemanticLinkageSignals
project_product_replay_semantic_linkage(
    const std::filesystem::path& results_path,
    EvidenceSnapshotRepository& snapshots) {
  const auto& semantic_source_spec = require_linkage_source(
      ProductReplayLinkageSourceRole::SemanticEvents);
  const auto& alignment_source_spec = require_linkage_source(
      ProductReplayLinkageSourceRole::Alignment);
  const auto& semantic_snapshot =
      snapshots.get(results_path / semantic_source_spec.artifact);
  const auto& alignment_snapshot =
      snapshots.get(results_path / alignment_source_spec.artifact);

  ProductReplaySemanticLinkageSignals signals;
  signals.semantic_events_present = semantic_snapshot.metadata.present;
  signals.semantic_events_readable = semantic_snapshot.read_result.readable;
  signals.semantic_events_read_status = semantic_snapshot.read_result.status;
  signals.alignment_present = alignment_snapshot.metadata.present;
  signals.alignment_readable = alignment_snapshot.read_result.readable;
  signals.alignment_read_status = alignment_snapshot.read_result.status;

  const auto semantic_source = select_strict_summary(semantic_snapshot);
  const auto alignment_source = select_strict_summary(alignment_snapshot);
  bool semantic_contract_issue = semantic_source.contract_issue;
  bool alignment_contract_issue = alignment_source.contract_issue;
  project_semantic_summary(semantic_source, signals, semantic_contract_issue);
  project_alignment_summary(alignment_source, signals,
                            alignment_contract_issue);
  project_correlation_rules(results_path, semantic_source_spec,
                            semantic_snapshot, snapshots, signals);

  signals.read_issue_count = inferlab::internal::checked_sum_size(
      {source_issue_count(semantic_snapshot, semantic_contract_issue),
       source_issue_count(alignment_snapshot, alignment_contract_issue)},
      "product replay semantic linkage source issue count");
  const std::size_t missing_artifact_count =
      inferlab::internal::checked_sum_size(
          {signals.semantic_events_present ? 0U : 1U,
           signals.alignment_present ? 0U : 1U},
          "product replay semantic linkage missing artifact count");
  signals.blocker_count = inferlab::internal::checked_sum_size(
      {missing_artifact_count, signals.read_issue_count,
       signals.pending_event_count, signals.pending_rule_count,
       signals.blocked_rule_count, signals.dry_run_rule_count,
       signals.alignment_rule_count > 0U ? 0U : 1U,
       signals.alignment_decision == "aligned_explanation_ready" &&
               signals.fully_aligned_rule_count ==
                   signals.alignment_rule_count
           ? 0U
           : 1U,
       signals.blocked_correlation_count},
      "product replay semantic linkage blocker count");
  signals.target_or_validation_action_count =
      inferlab::internal::checked_sum_size(
          {missing_artifact_count, signals.pending_event_count,
           signals.pending_rule_count, signals.blocked_rule_count,
           signals.dry_run_rule_count,
           signals.alignment_rule_count > 0U ? 0U : 1U,
           signals.alignment_decision == "aligned_explanation_ready" &&
                   signals.fully_aligned_rule_count ==
                       signals.alignment_rule_count
               ? 0U
               : 1U,
           signals.blocked_correlation_count},
          "product replay semantic linkage target action count");
  derive_linkage_status(signals);
  return signals;
}

ProductReplayReadinessSignals project_product_replay_readiness(
    const std::filesystem::path& results_path,
    EvidenceSnapshotRepository& snapshots) {
  const auto& semantic_source_spec = require_linkage_source(
      ProductReplayLinkageSourceRole::SemanticEvents);
  const auto& alignment_source_spec = require_linkage_source(
      ProductReplayLinkageSourceRole::Alignment);
  const auto& semantic_snapshot =
      snapshots.get(results_path / semantic_source_spec.artifact);
  const auto& alignment_snapshot =
      snapshots.get(results_path / alignment_source_spec.artifact);

  ProductReplayReadinessSignals signals;
  signals.semantic_events_present = semantic_snapshot.metadata.present;
  signals.semantic_events_readable = semantic_snapshot.read_result.readable;
  signals.semantic_events_read_status = semantic_snapshot.read_result.status;
  signals.alignment_present = alignment_snapshot.metadata.present;
  signals.alignment_readable = alignment_snapshot.read_result.readable;
  signals.alignment_read_status = alignment_snapshot.read_result.status;

  const auto semantic_source = select_strict_summary(semantic_snapshot);
  const auto alignment_source = select_strict_summary(alignment_snapshot);
  bool semantic_contract_issue = semantic_source.contract_issue;
  bool alignment_contract_issue = alignment_source.contract_issue;
  ProductReplaySemanticLinkageSignals projected;
  project_semantic_summary(semantic_source, projected,
                           semantic_contract_issue);
  project_alignment_summary(alignment_source, projected,
                            alignment_contract_issue);
  signals.available_event_count = projected.available_event_count;
  signals.pending_event_count = projected.pending_event_count;
  signals.replay_supported_event_count =
      projected.replay_supported_event_count;
  signals.alignment_rule_count = projected.alignment_rule_count;
  signals.fully_aligned_rule_count = projected.fully_aligned_rule_count;
  signals.pending_rule_count = projected.pending_rule_count;
  signals.blocked_rule_count = projected.blocked_rule_count;
  signals.dry_run_rule_count = projected.dry_run_rule_count;
  signals.alignment_decision = projected.alignment_decision;
  signals.sample_replay_present = projected.sample_replay_present;
  signals.read_issue_count = inferlab::internal::checked_sum_size(
      {source_issue_count(semantic_snapshot, semantic_contract_issue),
       source_issue_count(alignment_snapshot, alignment_contract_issue)},
      "product replay readiness source issue count");
  const std::size_t missing_artifact_count =
      inferlab::internal::checked_sum_size(
          {signals.semantic_events_present ? 0U : 1U,
           signals.alignment_present ? 0U : 1U},
          "product replay readiness missing artifact count");
  signals.blocker_count = inferlab::internal::checked_sum_size(
      {missing_artifact_count, signals.read_issue_count,
       signals.pending_event_count, signals.pending_rule_count,
       signals.blocked_rule_count, signals.dry_run_rule_count,
       signals.alignment_rule_count > 0U ? 0U : 1U,
       signals.alignment_decision == "aligned_explanation_ready" &&
               signals.fully_aligned_rule_count ==
                   signals.alignment_rule_count
           ? 0U
           : 1U,
       signals.sample_replay_present ? 0U : 1U,
       signals.available_event_count > 0U ? 0U : 1U,
       signals.replay_supported_event_count > 0U ? 0U : 1U},
      "product replay readiness blocker count");
  derive_readiness_status(signals);
  return signals;
}

}  // namespace inferlab_evidence
