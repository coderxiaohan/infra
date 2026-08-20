#include "product_replay_alignment_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "native_artifact_semantic_contract.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::product_replay_alignment {
namespace {

using inferlab_evidence::EvidenceSnapshotRepository;

void increment(std::size_t& value, const char* context) {
  value = internal::checked_add_size(value, 1U, context);
}

double coverage_ratio(const std::size_t part,
                      const std::size_t whole,
                      const char* context) {
  if (whole == 0U || part > whole) {
    throw std::logic_error(std::string(context) +
                           " coverage cardinality is invalid");
  }
  const double value =
      static_cast<double>(part) / static_cast<double>(whole);
  if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
    throw std::logic_error(std::string(context) +
                           " coverage ratio is invalid");
  }
  return value;
}

std::size_t product_index(const AlignmentPlan& plan,
                          const std::string& profile_id) {
  std::size_t match = plan.product_artifacts.size();
  for (std::size_t i = 0U; i < plan.product_artifacts.size(); ++i) {
    if (product_profile(plan.product_artifacts[i]).id != profile_id) continue;
    if (match != plan.product_artifacts.size()) {
      throw std::logic_error(
          "product replay alignment product inventory is ambiguous: " +
          profile_id);
    }
    match = i;
  }
  if (match == plan.product_artifacts.size()) {
    throw std::out_of_range(
        "product replay alignment product inventory is missing: " +
        profile_id);
  }
  return match;
}

std::size_t native_index(const AlignmentPlan& plan,
                         const std::string& artifact) {
  std::size_t match = plan.native_artifacts.size();
  for (std::size_t i = 0U; i < plan.native_artifacts.size(); ++i) {
    if (plan.native_artifacts[i].identity.artifact != artifact) continue;
    if (match != plan.native_artifacts.size()) {
      throw std::logic_error(
          "product replay alignment native inventory is ambiguous: " +
          artifact);
    }
    match = i;
  }
  if (match == plan.native_artifacts.size()) {
    throw std::out_of_range(
        "product replay alignment native inventory is missing: " +
        artifact);
  }
  return match;
}

bool contains_index(const std::vector<std::size_t>& values,
                    const std::size_t expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

bool contains_string(const std::vector<std::string>& values,
                     const std::string& expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

ProductArtifactState inspect_product(
    const mature_product_profiles::ProductProfileSpec& profile,
    const std::filesystem::path& results_root,
    EvidenceSnapshotRepository& snapshots) {
  ProductArtifactState state;
  state.profile = &profile;
  state.resolved_path = results_root / profile.artifact;
  const auto& snapshot = snapshots.get(state.resolved_path);
  state.contract =
      mature_product_profiles::evaluate_product_profile_artifact_contract(
          profile, state.resolved_path, snapshot);
  state.read_issue =
      state.contract.path_probe_status == "path_probe_error" ||
      (state.contract.present &&
       (!state.contract.regular_file || !state.contract.readable ||
        !state.contract.parse_ok));
  return state;
}

NativeArtifactState inspect_native(
    const NativeArtifactIdentity& identity,
    const std::filesystem::path& results_root,
    EvidenceSnapshotRepository& snapshots) {
  NativeArtifactState state;
  state.identity = identity;
  state.resolved_path = results_root / identity.artifact;
  const auto& snapshot = snapshots.get(state.resolved_path);
  const auto semantic =
      inferlab_evidence::evaluate_native_artifact_semantic_contract(
          state.resolved_path, identity.expected_artifact_type,
          identity.required_fields, identity.required_metrics, snapshot);
  state.path_probe_status = semantic.path_probe_status;
  state.path_probe_error = semantic.path_probe_error;
  state.text_read_status = semantic.text_read_status;
  state.text_read_error = semantic.text_read_error;
  state.regular_file = semantic.regular_file;
  state.contract = semantic.artifact_contract;
  state.exact_artifact_type = semantic.exact_artifact_type;
  state.deep_dive_type_present =
      state.contract.parse_ok &&
      inferlab_artifact::contains_semantic_key_fallback(
          snapshot.read_result.text, "deep_dive_type");
  state.missing_required_fields = semantic.missing_required_fields;
  state.missing_required_metrics = semantic.missing_required_metrics;
  state.required_fields_present = semantic.required_fields_present;
  state.required_metrics_present = semantic.required_metrics_present;
  state.contract_ready = semantic.contract_ready;
  state.read_issue = semantic.read_issue;
  state.issues = semantic.issues;
  return state;
}

void build_unique_inventories(const std::filesystem::path& results_root,
                              EvidenceSnapshotRepository& snapshots,
                              AlignmentPlan& plan) {
  for (const auto& rule : alignment_rules()) {
    for (const auto& profile_id : rule.product_profile_ids) {
      const auto& profile =
          mature_product_profiles::require_product_profile(profile_id);
      bool already_present = false;
      for (const auto& state : plan.product_artifacts) {
        if (product_profile(state).id == profile.id) {
          already_present = true;
          break;
        }
      }
      if (!already_present) {
        plan.product_artifacts.push_back(
            inspect_product(profile, results_root, snapshots));
        plan.product_display_names.push_back(profile.display_name);
        inferlab_plan::append_unique_nonempty_string(
            plan.recommended_next_commands,
            mature_product_profiles::profile_replay_command(profile));
      }
    }

    for (const auto& selector : rule.native_artifacts) {
      const auto identity = resolve_native_artifact(selector);
      const auto existing = std::find_if(
          plan.native_artifacts.begin(), plan.native_artifacts.end(),
          [&](const auto& state) {
            return state.identity.artifact == identity.artifact;
          });
      if (existing == plan.native_artifacts.end()) {
        plan.native_artifacts.push_back(
            inspect_native(identity, results_root, snapshots));
        inferlab_plan::append_unique_nonempty_string(
            plan.recommended_next_commands, identity.refresh_command);
      } else if (existing->identity.canonical_id != identity.canonical_id ||
                 existing->identity.role != identity.role ||
                 existing->identity.family != identity.family) {
        throw std::logic_error(
            "product replay alignment native identity drift escaped fixture "
            "validation: " +
            identity.artifact);
      }
    }
  }
}

void classify_record(const AlignmentPlan& plan, AlignmentRecord& record) {
  bool invalid_existing_artifact = false;
  for (const auto index : record.product_artifact_indices) {
    const auto& state = plan.product_artifacts[index];
    if (state.contract.present) increment(record.product_present_count,
                                          "alignment product present count");
    if (state.contract.contract_ready) {
      increment(record.product_contract_ready_count,
                "alignment product contract-ready count");
    } else if (state.contract.present ||
               state.contract.path_probe_status == "path_probe_error") {
      invalid_existing_artifact = true;
    }
    if (state.contract.product_evidence_eligible &&
        state.contract.replayed) {
      increment(record.product_replayed_count,
                "alignment product replayed count");
    }
    if (state.contract.product_evidence_eligible &&
        state.contract.executed) {
      increment(record.product_executed_count,
                "alignment product executed count");
    }
    if (state.contract.product_evidence_eligible) {
      increment(record.product_evidence_count,
                "alignment product evidence count");
    }
    if (state.read_issue) {
      increment(record.read_issue_count, "alignment record read issue count");
    }
  }
  for (const auto index : record.native_artifact_indices) {
    const auto& state = plan.native_artifacts[index];
    if (state.contract.present) increment(record.native_present_count,
                                          "alignment native present count");
    if (state.contract_ready) {
      increment(record.native_contract_ready_count,
                "alignment native contract-ready count");
    } else if (state.contract.present ||
               state.path_probe_status == "path_probe_error") {
      invalid_existing_artifact = true;
    }
    if (state.read_issue) {
      increment(record.read_issue_count, "alignment record read issue count");
    }
  }

  const std::size_t product_count = record.product_artifact_indices.size();
  const std::size_t native_count = record.native_artifact_indices.size();
  record.product_contract_complete =
      record.product_contract_ready_count == product_count;
  record.native_contract_complete =
      record.native_contract_ready_count == native_count;
  record.full_product_evidence_coverage =
      record.product_evidence_count == product_count;
  record.product_coverage = coverage_ratio(
      record.product_present_count, product_count, "alignment product");
  record.product_contract_coverage = coverage_ratio(
      record.product_contract_ready_count, product_count,
      "alignment product contract");
  record.replay_evidence_coverage = coverage_ratio(
      record.product_evidence_count, product_count,
      "alignment replay/execute evidence");
  record.native_coverage = coverage_ratio(
      record.native_contract_ready_count, native_count,
      "alignment native contract");
  record.structural_alignment_coverage =
      (record.product_contract_coverage + record.native_coverage) / 2.0;
  record.alignment_score =
      (record.product_contract_coverage + record.native_coverage +
       record.replay_evidence_coverage) /
      3.0;
  if (!std::isfinite(record.structural_alignment_coverage) ||
      !std::isfinite(record.alignment_score) ||
      record.structural_alignment_coverage < 0.0 ||
      record.structural_alignment_coverage > 1.0 ||
      record.alignment_score < 0.0 || record.alignment_score > 1.0) {
    throw std::logic_error("product replay alignment score is invalid");
  }

  const auto& rule = alignment_rule(record);
  if (invalid_existing_artifact) {
    record.status = "blocked_invalid_artifact_contract";
    record.recommendation =
        "repair malformed, unreadable, identity-mismatched, or contract-invalid alignment inputs";
  } else if (!record.native_contract_complete) {
    record.status = "pending_native_deep_dive_artifacts";
    record.recommendation =
        "refresh contract-valid native explanation artifacts before product alignment";
  } else if (!record.product_contract_complete) {
    record.status = "pending_product_replay_or_profile_artifacts";
    record.recommendation = rule.next_action_when_missing;
  } else if (record.product_evidence_count > 0U) {
    record.status = "aligned_replay_or_execute_evidence";
    record.recommendation =
        record.full_product_evidence_coverage
            ? "use alignment as explanation only and keep product core external"
            : "alignment covers only products carrying valid replay or execute evidence; collect remaining product evidence before broader claims";
  } else {
    record.status = "dry_run_alignment_only";
    record.recommendation =
        "collect replay or execute evidence before runtime claims";
  }
}

AlignmentRecord build_record(const AlignmentRuleSpec& rule,
                             const AlignmentPlan& plan) {
  AlignmentRecord record;
  record.rule = &rule;
  for (const auto& profile_id : rule.product_profile_ids) {
    const auto index = product_index(plan, profile_id);
    if (contains_index(record.product_artifact_indices, index)) {
      throw std::logic_error(
          "product replay alignment record repeats a product index");
    }
    record.product_artifact_indices.push_back(index);
  }
  for (const auto& selector : rule.native_artifacts) {
    const auto identity = resolve_native_artifact(selector);
    const auto index = native_index(plan, identity.artifact);
    if (contains_index(record.native_artifact_indices, index)) {
      throw std::logic_error(
          "product replay alignment record repeats a native index");
    }
    record.native_artifact_indices.push_back(index);
  }
  classify_record(plan, record);
  return record;
}

int record_selection_rank(const AlignmentRecord& record) {
  if (record.status == "aligned_replay_or_execute_evidence") return 4;
  if (record.status == "dry_run_alignment_only") return 3;
  if (record.status.rfind("pending_", 0U) == 0U) return 2;
  if (record.status.rfind("blocked_", 0U) == 0U) return 1;
  throw std::logic_error(
      "product replay alignment record has an unknown selection status");
}

void build_summary(AlignmentPlan& plan,
                   const EvidenceSnapshotRepository& snapshots) {
  AlignmentSummary summary;
  summary.rule_count = plan.records.size();
  summary.unique_product_artifact_count = plan.product_artifacts.size();
  summary.unique_native_artifact_count = plan.native_artifacts.size();
  summary.physical_artifact_read_count = snapshots.size();

  for (const auto& state : plan.product_artifacts) {
    if (state.contract.contract_ready) {
      increment(summary.product_contract_ready_count,
                "alignment summary product contract count");
    }
    if (state.contract.product_evidence_eligible &&
        state.contract.replayed) {
      increment(summary.replay_evidence_product_count,
                "alignment summary replay evidence count");
    }
    if (state.contract.product_evidence_eligible &&
        state.contract.executed) {
      increment(summary.execute_evidence_product_count,
                "alignment summary execute evidence count");
    }
    if (state.read_issue) {
      increment(summary.read_issue_count,
                "alignment summary read issue count");
    }
  }
  for (const auto& state : plan.native_artifacts) {
    if (state.contract_ready) {
      increment(summary.native_contract_ready_count,
                "alignment summary native contract count");
    }
    if (state.read_issue) {
      increment(summary.read_issue_count,
                "alignment summary read issue count");
    }
  }

  if (plan.records.empty()) {
    throw std::logic_error("product replay alignment produced no records");
  }
  for (std::size_t i = 0U; i < plan.records.size(); ++i) {
    const auto& record = plan.records[i];
    if (record.status == "aligned_replay_or_execute_evidence") {
      increment(summary.aligned_rule_count,
                "alignment summary aligned count");
      if (record.full_product_evidence_coverage) {
        increment(summary.fully_aligned_rule_count,
                  "alignment summary fully aligned count");
      }
    } else if (record.status == "dry_run_alignment_only") {
      increment(summary.dry_run_rule_count,
                "alignment summary dry-run count");
    } else if (record.status.rfind("pending_", 0U) == 0U) {
      increment(summary.pending_rule_count,
                "alignment summary pending count");
    } else if (record.status.rfind("blocked_", 0U) == 0U) {
      increment(summary.blocked_rule_count,
                "alignment summary blocked count");
    } else {
      throw std::logic_error(
          "product replay alignment record has an unknown status");
    }
    const auto& best = plan.records[summary.best_record_index];
    const int record_rank = record_selection_rank(record);
    const int best_rank = record_selection_rank(best);
    if (record_rank > best_rank ||
        (record_rank == best_rank &&
         record.alignment_score > best.alignment_score)) {
      summary.best_record_index = i;
    }
  }
  summary.best_alignment_rule =
      alignment_rule(plan.records[summary.best_record_index]).name;
  summary.best_alignment_score =
      plan.records[summary.best_record_index].alignment_score;

  const std::size_t classified = internal::checked_sum_size(
      {summary.aligned_rule_count, summary.dry_run_rule_count,
       summary.pending_rule_count, summary.blocked_rule_count},
      "alignment summary classified rule count");
  const std::size_t expected_reads = internal::checked_add_size(
      summary.unique_product_artifact_count,
      summary.unique_native_artifact_count,
      "alignment summary expected physical read count");
  if (classified != summary.rule_count ||
      expected_reads != summary.physical_artifact_read_count ||
      summary.product_contract_ready_count >
          summary.unique_product_artifact_count ||
      summary.native_contract_ready_count >
          summary.unique_native_artifact_count ||
      summary.fully_aligned_rule_count > summary.aligned_rule_count) {
    throw std::logic_error(
        "product replay alignment summary reconciliation failed");
  }

  if (summary.blocked_rule_count > 0U) {
    summary.alignment_decision = "blocked";
  } else if (summary.pending_rule_count > 0U) {
    summary.alignment_decision = "pending_inputs";
  } else if (summary.fully_aligned_rule_count == summary.rule_count) {
    summary.alignment_decision = "aligned_explanation_ready";
  } else if (summary.aligned_rule_count > 0U) {
    summary.alignment_decision = "partial_alignment_ready";
  } else {
    summary.alignment_decision = "dry_run_only";
  }
  plan.summary = summary;
}

}  // namespace

const mature_product_profiles::ProductProfileSpec& product_profile(
    const ProductArtifactState& state) {
  if (state.profile == nullptr || state.contract.profile != state.profile) {
    throw std::logic_error(
        "product replay alignment product state has no canonical profile");
  }
  return *state.profile;
}

const AlignmentRuleSpec& alignment_rule(const AlignmentRecord& record) {
  if (record.rule == nullptr) {
    throw std::logic_error(
        "product replay alignment record has no canonical rule");
  }
  return *record.rule;
}

AlignmentPlan build_alignment_plan(
    const std::filesystem::path& results_root) {
  if (results_root.empty()) {
    throw std::invalid_argument(
        "product replay alignment results root must not be empty");
  }
  (void)alignment_rules();
  EvidenceSnapshotRepository snapshots;
  AlignmentPlan plan;
  plan.claim_boundary =
      "aligns contract-valid external-product replay/profile evidence to contract-valid InferLab native explanation artifacts; does not execute products, refresh producers, run benchmarks, prove causal joins, or claim product-core ownership";
  build_unique_inventories(results_root, snapshots, plan);
  for (const auto& rule : alignment_rules()) {
    plan.records.push_back(build_record(rule, plan));
  }
  build_summary(plan, snapshots);
  if (plan.product_artifacts.size() !=
          mature_product_profiles::product_profile_count_for_scope(
              mature_product_profiles::
                  ProductProfileScope::default_required) ||
      plan.product_display_names.size() != plan.product_artifacts.size() ||
      plan.recommended_next_commands.empty() ||
      contains_string(plan.recommended_next_commands, "")) {
    throw std::logic_error(
        "product replay alignment request projection is incomplete");
  }
  return plan;
}

}  // namespace inferlab::product_replay_alignment
