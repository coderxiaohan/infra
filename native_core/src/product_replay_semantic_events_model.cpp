#include "product_replay_semantic_events_model.hpp"

#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"
#include "product_replay_semantic_events_sample_parser.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::product_replay_semantic_events {
namespace {

namespace fs = std::filesystem;
using inferlab_evidence::EvidenceSnapshotRepository;

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

void append_index(std::vector<std::size_t>& values,
                  const std::size_t value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void append_string(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

ProductProfileState inspect_profile(
    const mature_product_profiles::ProductProfileSpec& profile,
    const fs::path& results_root,
    EvidenceSnapshotRepository& snapshots) {
  ProductProfileState state;
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

std::size_t find_profile_index(
    const std::vector<ProductProfileState>& profiles,
    const std::string& profile_id) {
  for (std::size_t index = 0U; index < profiles.size(); ++index) {
    if (product_profile(profiles[index]).id == profile_id) return index;
  }
  return profiles.size();
}

void validate_requests(const std::vector<ReplaySampleRequest>& requests) {
  if (requests.empty() ||
      requests.size() > maximum_replay_sample_count()) {
    throw std::invalid_argument(
        "product replay semantic events require 1..64 sample inputs");
  }
  std::vector<std::string> paths;
  for (const auto& request : requests) {
    const std::string normalized =
        request.path.lexically_normal().generic_string();
    if (normalized.empty()) {
      throw std::invalid_argument(
          "product replay semantic sample path must not be empty");
    }
    if (std::find(paths.begin(), paths.end(), normalized) != paths.end()) {
      throw std::invalid_argument(
          "duplicate product replay semantic sample path");
    }
    paths.push_back(normalized);
    if (request.origin == SampleInputOrigin::default_fixture) {
      if (request.expected_product_profile_id.empty()) {
        throw std::invalid_argument(
            "default semantic sample requires a product profile id");
      }
      (void)mature_product_profiles::require_product_profile(
          request.expected_product_profile_id);
    } else if (!request.expected_product_profile_id.empty()) {
      throw std::invalid_argument(
          "explicit semantic replay sample must carry product ids in rows");
    }
  }
}

void build_profile_inventory(SemanticEventsPlan& plan,
                             const fs::path& results_root,
                             EvidenceSnapshotRepository& snapshots) {
  std::vector<std::string> profile_ids;
  for (const auto& source : plan.sample_sources) {
    if (!source.request.expected_product_profile_id.empty()) {
      append_string(profile_ids,
                    source.request.expected_product_profile_id);
    }
    for (const auto& profile_id : source.product_profile_ids) {
      append_string(profile_ids, profile_id);
    }
  }
  for (const auto& profile_id : profile_ids) {
    const auto& profile =
        mature_product_profiles::require_product_profile(profile_id);
    plan.product_profiles.push_back(
        inspect_profile(profile, results_root, snapshots));
    append_string(plan.product_display_names, profile.display_name);
    append_string(plan.recommended_next_commands,
                  mature_product_profiles::profile_replay_command(profile));
  }
  for (auto& event : plan.normalized_events) {
    if (!event.product_identity_valid) continue;
    const auto index = find_profile_index(plan.product_profiles,
                                          event.product_profile_id);
    if (index >= plan.product_profiles.size()) {
      throw std::logic_error(
          "semantic event profile escaped profile inventory");
    }
    event.profile_contract_ready =
        plan.product_profiles[index].contract.contract_ready;
    event.source_artifact =
        product_profile(plan.product_profiles[index]).artifact;
  }
}

void classify_record(const SemanticEventsPlan& plan,
                     SemanticEventRecord& record) {
  bool represented_profile_invalid = false;
  for (const auto profile_index : record.product_profile_indices) {
    const auto& profile = plan.product_profiles[profile_index];
    if (profile.contract.present) {
      increment(record.product_profile_present_count,
                "semantic event product profile present count");
    }
    if (profile.contract.contract_ready) {
      increment(record.product_profile_contract_ready_count,
                "semantic event product profile contract-ready count");
    } else if (profile.contract.present) {
      represented_profile_invalid = true;
      append_string(record.issues,
                    "product_profile_contract_invalid:" +
                        product_profile(profile).id);
    }
    if (profile.contract.contract_ready && profile.contract.replayed) {
      increment(record.product_profile_replayed_count,
                "semantic event product profile replayed count");
    }
    if (profile.contract.contract_ready && profile.contract.executed) {
      increment(record.product_profile_executed_count,
                "semantic event product profile executed count");
    }
    if (profile.read_issue) {
      increment(record.read_issue_count,
                "semantic event record profile read issue count");
    }
  }
  for (const auto source_index : record.sample_source_indices) {
    if (plan.sample_sources[source_index].read_issue) {
      increment(record.read_issue_count,
                "semantic event record sample read issue count");
    }
  }

  record.source_present = record.product_profile_present_count > 0U;
  record.source_replayed = record.explicit_replay_event_count > 0U;
  record.source_executed =
      record.product_profile_executed_count > 0U;
  record.sample_observed =
      record.fixture_event_count > 0U ||
      record.explicit_replay_event_count > 0U;
  record.observed_signal = record.explicit_replay_event_count > 0U;

  record.contract_check_count = 4U;
  record.satisfied_contract_check_count = 1U;
  if (record.sample_observed) {
    increment(record.satisfied_contract_check_count,
              "semantic event satisfied contract check count");
  }
  if (!record.product_profile_indices.empty() &&
      record.product_profile_contract_ready_count ==
          record.product_profile_indices.size()) {
    increment(record.satisfied_contract_check_count,
              "semantic event satisfied contract check count");
  }
  if (record.replay_ready_event_count > 0U) {
    increment(record.satisfied_contract_check_count,
              "semantic event satisfied contract check count");
  }
  record.contract_coverage =
      static_cast<double>(record.satisfied_contract_check_count) /
      static_cast<double>(record.contract_check_count);
  record.confidence = record.contract_coverage;

  if (record.invalid_explicit_event_count > 0U ||
      represented_profile_invalid) {
    record.status = "blocked_invalid_replay_event";
    record.recommendation =
        "repair the explicit replay row or canonical product profile contract";
  } else if (record.replay_ready_event_count ==
                 record.explicit_replay_event_count &&
             record.replay_ready_event_count > 0U) {
    record.status = "semantic_event_available";
    record.recommendation =
        "attach the replay-derived event to the declared native explanation target";
  } else if (record.replay_ready_event_count > 0U) {
    record.status = "partial_replay_event_profile_contract_ready";
    record.recommendation =
        "refresh every represented canonical product profile before treating the aggregated event type as replay-ready";
  } else if (record.explicit_replay_event_count > 0U) {
    record.status = "replay_event_profile_contract_pending";
    record.recommendation =
        "refresh the canonical product profile contract before using the replay event";
  } else if (record.fixture_event_count > 0U) {
    record.status = "fixture_event_schema_available";
    record.recommendation =
        "use the fixture only as schema guidance and import explicit replay evidence";
  } else if (record.invalid_fixture_event_count > 0U) {
    record.status = "fixture_event_schema_invalid";
    record.recommendation =
        "repair the repository fixture before using it as schema guidance";
  } else {
    record.status = "pending_replay_event";
    record.recommendation = record.rule->next_action_when_missing;
  }
}

void build_event_records(SemanticEventsPlan& plan) {
  for (const auto& rule : semantic_event_rules()) {
    SemanticEventRecord record;
    record.rule = &rule;
    record.native_destination =
        product_replay_alignment::resolve_native_artifact(
            rule.native_destination);
    append_string(plan.recommended_next_commands,
                  record.native_destination.refresh_command);

    for (std::size_t event_index = 0U;
         event_index < plan.normalized_events.size(); ++event_index) {
      const auto& event = plan.normalized_events[event_index];
      if (event.rule != &rule) continue;
      append_index(record.sample_source_indices, event.source_index);
      if (event.product_identity_valid) {
        append_string(record.observed_product_profile_ids,
                      event.product_profile_id);
        const auto profile_index = find_profile_index(
            plan.product_profiles, event.product_profile_id);
        if (profile_index >= plan.product_profiles.size()) {
          throw std::logic_error(
              "semantic event record references an unknown product profile");
        }
        append_index(record.product_profile_indices, profile_index);
        append_string(record.source_artifacts,
                      product_profile(plan.product_profiles[profile_index])
                          .artifact);
      }
      const auto origin =
          plan.sample_sources[event.source_index].request.origin;
      if (event.fixture_schema_valid) {
        increment(record.fixture_event_count,
                  "semantic event fixture row count");
      } else if (event.explicit_replay_valid) {
        increment(record.explicit_replay_event_count,
                  "semantic event explicit replay row count");
        if (event.profile_contract_ready) {
          increment(record.replay_ready_event_count,
                    "semantic event replay-ready row count");
        }
      } else if (origin == SampleInputOrigin::default_fixture) {
        increment(record.invalid_fixture_event_count,
                  "semantic event invalid fixture row count");
      } else {
        increment(record.invalid_explicit_event_count,
                  "semantic event invalid explicit row count");
      }
    }
    classify_record(plan, record);
    plan.records.push_back(std::move(record));
  }
}

void build_product_coverage(SemanticEventsPlan& plan) {
  for (std::size_t profile_index = 0U;
       profile_index < plan.product_profiles.size(); ++profile_index) {
    ProductCoverageRecord coverage;
    coverage.product_profile_index = profile_index;
    const auto& profile = product_profile(plan.product_profiles[profile_index]);
    for (std::size_t source_index = 0U;
         source_index < plan.sample_sources.size(); ++source_index) {
      const auto& source = plan.sample_sources[source_index];
      if (source.request.expected_product_profile_id == profile.id ||
          std::find(source.product_profile_ids.begin(),
                    source.product_profile_ids.end(), profile.id) !=
              source.product_profile_ids.end()) {
        coverage.sample_source_indices.push_back(source_index);
      }
    }
    for (const auto& event : plan.normalized_events) {
      if (event.product_profile_id != profile.id) continue;
      if (event.fixture_schema_valid) {
        increment(coverage.fixture_event_count,
                  "semantic event product fixture coverage count");
      }
      if (event.explicit_replay_valid) {
        increment(coverage.explicit_replay_event_count,
                  "semantic event product replay coverage count");
      }
    }
    coverage.valid_event_count = inferlab::internal::checked_add_size(
        coverage.fixture_event_count, coverage.explicit_replay_event_count,
        "semantic event product valid coverage count");
    if (coverage.explicit_replay_event_count > 0U &&
        plan.product_profiles[profile_index].contract.contract_ready) {
      coverage.coverage_status = "replay_linked";
    } else if (coverage.explicit_replay_event_count > 0U) {
      coverage.coverage_status = "replay_profile_contract_pending";
    } else if (coverage.fixture_event_count > 0U) {
      coverage.coverage_status = "fixture_schema_available";
    } else if (!coverage.sample_source_indices.empty()) {
      coverage.coverage_status = "sample_contract_pending";
    } else {
      coverage.coverage_status = "sample_missing";
    }
    plan.product_coverage.push_back(std::move(coverage));
  }
}

void build_native_alignment_coverage(SemanticEventsPlan& plan) {
  for (const auto& record : plan.records) {
    auto existing = std::find_if(
        plan.native_alignment_coverage.begin(),
        plan.native_alignment_coverage.end(), [&](const auto& candidate) {
          return candidate.native_destination.artifact ==
                 record.native_destination.artifact;
        });
    if (existing == plan.native_alignment_coverage.end()) {
      NativeAlignmentCoverageRecord coverage;
      coverage.native_destination = record.native_destination;
      plan.native_alignment_coverage.push_back(std::move(coverage));
      existing = std::prev(plan.native_alignment_coverage.end());
    } else if (existing->native_destination.canonical_id !=
                   record.native_destination.canonical_id ||
               existing->native_destination.producer_target !=
                   record.native_destination.producer_target ||
               existing->native_destination.refresh_command !=
                   record.native_destination.refresh_command) {
      throw std::logic_error(
          "semantic event native destination identity drift");
    }
    append_string(existing->event_types, record.rule->event_type);
    increment(existing->event_type_count,
              "semantic event native alignment event type count");
    if (record.fixture_event_count > 0U) {
      increment(existing->fixture_event_type_count,
                "semantic event native fixture event type count");
    }
    if (record.explicit_replay_event_count > 0U) {
      increment(existing->replay_event_type_count,
                "semantic event native replay event type count");
    }
  }
  for (auto& coverage : plan.native_alignment_coverage) {
    if (coverage.replay_event_type_count > 0U) {
      coverage.coverage_status = "replay_link_declared";
    } else if (coverage.fixture_event_type_count > 0U) {
      coverage.coverage_status = "fixture_link_declared";
    } else {
      coverage.coverage_status = "pending_replay";
    }
  }
}

bool status_is_blocked(const std::string& status);

void build_release_pipeline_linkage(SemanticEventsPlan& plan) {
  for (const auto& record : plan.records) {
    ReleasePipelineLinkageRecord linkage;
    linkage.event_type = record.rule->event_type;
    linkage.event_status = record.status;
    if (record.status == "semantic_event_available") {
      linkage.release_impact = "can_explain_replay_claim_boundary";
    } else if (status_is_blocked(record.status)) {
      linkage.release_impact = "blocks_replay_claim_until_contract_is_repaired";
    } else if (record.status == "fixture_event_schema_available") {
      linkage.release_impact = "fixture_schema_only_no_replay_claim";
    } else {
      linkage.release_impact = "blocks_replay_claim_until_evidence_exists";
    }
    linkage.claim_boundary =
        "semantic event output is an upstream input to the release pipeline replay dimension; this producer does not read, execute, or close the pipeline";
    plan.release_pipeline_linkage.push_back(std::move(linkage));
  }
}

bool status_is_blocked(const std::string& status) {
  return status == "blocked_invalid_replay_event" ||
         status == "fixture_event_schema_invalid";
}

bool status_is_pending(const std::string& status) {
  return status == "replay_event_profile_contract_pending" ||
         status == "partial_replay_event_profile_contract_ready" ||
         status == "pending_replay_event";
}

void build_summary(SemanticEventsPlan& plan,
                   const EvidenceSnapshotRepository& snapshots) {
  auto& summary = plan.summary;
  summary.event_type_count = plan.records.size();
  summary.sample_source_count = plan.sample_sources.size();
  summary.product_profile_count = plan.product_profiles.size();
  for (const auto& source : plan.sample_sources) {
    if (source.request.origin == SampleInputOrigin::default_fixture) {
      increment(summary.default_fixture_source_count,
                "semantic event default fixture source count");
    } else {
      increment(summary.explicit_replay_source_count,
                "semantic event explicit replay source count");
    }
    if (source.present) {
      increment(summary.present_sample_source_count,
                "semantic event present sample source count");
    }
    if (source.readable) {
      increment(summary.readable_sample_source_count,
                "semantic event readable sample source count");
    }
    if (source.line_count > 0U) {
      increment(summary.eventful_sample_source_count,
                "semantic event eventful sample source count");
    }
    if (source.source_contract_ready) {
      increment(summary.source_contract_ready_count,
                "semantic event source contract-ready count");
    }
    summary.sample_event_count = inferlab::internal::checked_add_size(
        summary.sample_event_count, source.line_count,
        "semantic event sample line total");
    summary.valid_sample_event_count =
        inferlab::internal::checked_add_size(
            summary.valid_sample_event_count, source.valid_event_count,
            "semantic event valid sample event total");
    summary.fixture_sample_event_count =
        inferlab::internal::checked_add_size(
            summary.fixture_sample_event_count, source.fixture_event_count,
            "semantic event fixture sample event total");
    summary.explicit_replay_sample_event_count =
        inferlab::internal::checked_add_size(
            summary.explicit_replay_sample_event_count,
            source.explicit_replay_event_count,
            "semantic event explicit replay sample event total");
    summary.invalid_sample_event_count =
        inferlab::internal::checked_add_size(
            summary.invalid_sample_event_count, source.invalid_event_count,
            "semantic event invalid sample event total");
    summary.unsupported_sample_event_count =
        inferlab::internal::checked_add_size(
            summary.unsupported_sample_event_count,
            source.unsupported_event_count,
            "semantic event unsupported sample event total");
    if (source.read_issue) {
      increment(summary.read_issue_count,
                "semantic event source read issue count");
    }
  }

  for (const auto& profile : plan.product_profiles) {
    if (profile.contract.present) {
      increment(summary.product_profile_present_count,
                "semantic event product profile present count");
    }
    if (profile.contract.contract_ready) {
      increment(summary.product_profile_contract_ready_count,
                "semantic event profile contract-ready count");
    }
    if (profile.contract.contract_ready && profile.contract.replayed) {
      increment(summary.product_profile_replayed_count,
                "semantic event profile replayed count");
    }
    if (profile.contract.contract_ready && profile.contract.executed) {
      increment(summary.product_profile_executed_count,
                "semantic event profile executed count");
    }
    if (profile.read_issue) {
      increment(summary.read_issue_count,
                "semantic event profile read issue count");
    }
  }

  for (const auto& record : plan.records) {
    if (record.status == "semantic_event_available") {
      increment(summary.available_event_count,
                "semantic event available event count");
    } else if (record.status == "fixture_event_schema_available") {
      increment(summary.sample_available_event_count,
                "semantic event sample available event count");
    } else if (status_is_pending(record.status)) {
      increment(summary.pending_event_count,
                "semantic event pending event count");
    } else if (status_is_blocked(record.status)) {
      increment(summary.blocked_event_count,
                "semantic event blocked event count");
    } else {
      throw std::logic_error(
          "unknown product replay semantic event record status");
    }
    if (record.fixture_event_count > 0U) {
      increment(summary.fixture_schema_event_count,
                "semantic event fixture schema event count");
    }
    if (record.explicit_replay_event_count > 0U) {
      increment(summary.replay_supported_event_count,
                "semantic event replay-supported event count");
    }
    if (record.replay_ready_event_count > 0U) {
      increment(summary.replay_ready_event_count,
                "semantic event replay-ready event count");
    }
  }

  summary.sample_present = summary.present_sample_source_count > 0U;
  summary.sample_replay_present =
      summary.explicit_replay_sample_event_count > 0U;
  summary.mode = summary.sample_replay_present ? "replay" : "dry_run";
  summary.physical_source_capture_count =
      inferlab::internal::checked_add_size(
          plan.sample_sources.size(), snapshots.size(),
          "semantic event physical source capture count");
  summary.blocker_count = inferlab::internal::checked_add_size(
      summary.blocked_event_count, summary.read_issue_count,
      "semantic event blocker count");
  summary.blocker_count = inferlab::internal::checked_add_size(
      summary.blocker_count, plan.request_trace.summary.blocker_count,
      "semantic event request-trace blocker count");
  if (summary.blocker_count > 0U) {
    summary.semantic_event_decision = "blocked";
  } else if (summary.available_event_count == summary.event_type_count) {
    summary.semantic_event_decision = "replay_semantics_ready";
  } else if (summary.available_event_count > 0U) {
    summary.semantic_event_decision = "partial_replay_semantics_ready";
  } else if (summary.fixture_schema_event_count > 0U) {
    summary.semantic_event_decision = "fixture_schema_ready";
  } else {
    summary.semantic_event_decision = "pending_replay_evidence";
  }
}

void build_blockers(SemanticEventsPlan& plan) {
  for (const auto& source : plan.sample_sources) {
    if (source.read_issue && !source.issues.empty()) {
      append_string(plan.blockers,
                    std::string("sample_source:") +
                        sample_input_origin_name(source.request.origin) +
                        ":" + inferlab_plan::redacted_path_label(
                                   source.request.path.generic_string()) +
                        ":" + source.issues.front());
    }
  }
  for (const auto& profile : plan.product_profiles) {
    if (!profile.read_issue) continue;
    append_string(plan.blockers,
                  "product_profile:" + product_profile(profile).id +
                      ":artifact_read_or_parse_issue");
  }
  for (const auto& record : plan.records) {
    if (!status_is_blocked(record.status)) continue;
    append_string(plan.blockers,
                  "event_type:" + record.rule->event_type + ":" +
                      record.status);
  }
  for (const auto& blocker : plan.request_trace.blockers) {
    append_string(plan.blockers, blocker);
  }
}

void discard_ephemeral_identity_values(
    std::vector<NormalizedEventRow>& events) {
  for (auto& event : events) {
    event.request_id.clear();
    event.request_ids.clear();
    event.trace_id.clear();
    event.trace_ids.clear();
  }
}

void validate_plan(const SemanticEventsPlan& plan,
                   const EvidenceSnapshotRepository& snapshots) {
  const auto& summary = plan.summary;
  if (plan.artifact_identity.canonical_id !=
          "product_replay_semantic_events" ||
      plan.release_pipeline == nullptr ||
      plan.release_pipeline->id != "release_evidence_pipeline" ||
      plan.records.size() != semantic_event_rules().size() ||
      summary.event_type_count != plan.records.size() ||
      summary.sample_source_count != plan.sample_sources.size() ||
      summary.product_profile_count != plan.product_profiles.size() ||
      summary.physical_source_capture_count !=
          inferlab::internal::checked_add_size(
              plan.sample_sources.size(), snapshots.size(),
              "semantic event expected source capture count") ||
      inferlab::internal::checked_sum_size(
          {summary.available_event_count,
           summary.sample_available_event_count,
           summary.pending_event_count, summary.blocked_event_count},
          "semantic event record status partition") !=
          summary.event_type_count ||
      inferlab::internal::checked_sum_size(
          {summary.valid_sample_event_count,
           summary.invalid_sample_event_count,
           summary.unsupported_sample_event_count},
          "semantic event sample row partition") !=
          summary.sample_event_count ||
      inferlab::internal::checked_add_size(
          summary.fixture_sample_event_count,
          summary.explicit_replay_sample_event_count,
          "semantic event valid sample partition") !=
          summary.valid_sample_event_count ||
      summary.present_sample_source_count > summary.sample_source_count ||
      summary.readable_sample_source_count > summary.sample_source_count ||
      summary.source_contract_ready_count > summary.sample_source_count ||
      summary.product_profile_present_count >
          summary.product_profile_count ||
      summary.product_profile_contract_ready_count >
          summary.product_profile_count ||
      summary.replay_ready_event_count >
          summary.replay_supported_event_count ||
      summary.sample_event_count != plan.normalized_events.size() ||
      plan.request_trace.summary.schema_valid_event_count !=
          summary.valid_sample_event_count ||
      plan.request_trace.summary.blocker_count !=
          plan.request_trace.blockers.size() ||
      (summary.mode == "replay") != summary.sample_replay_present ||
      summary.blocker_count != plan.blockers.size()) {
    throw std::logic_error(
        "product replay semantic events plan accounting is inconsistent");
  }
  if (plan.product_coverage.size() != plan.product_profiles.size() ||
      plan.release_pipeline_linkage.size() != plan.records.size() ||
      plan.native_alignment_coverage.empty()) {
    throw std::logic_error(
        "product replay semantic coverage projections are inconsistent");
  }
  for (const auto& coverage : plan.product_coverage) {
    if (coverage.product_profile_index >= plan.product_profiles.size() ||
        coverage.valid_event_count !=
            inferlab::internal::checked_add_size(
                coverage.fixture_event_count,
                coverage.explicit_replay_event_count,
                "semantic event expected product coverage count")) {
      throw std::logic_error(
          "product replay semantic product coverage is inconsistent");
    }
  }
  for (const auto& coverage : plan.native_alignment_coverage) {
    if (coverage.native_destination.artifact.empty() ||
        coverage.event_type_count != coverage.event_types.size() ||
        coverage.fixture_event_type_count > coverage.event_type_count ||
        coverage.replay_event_type_count > coverage.event_type_count) {
      throw std::logic_error(
          "product replay semantic native coverage is inconsistent");
    }
  }
  std::vector<bool> event_represented(plan.normalized_events.size(), false);
  for (std::size_t source_index = 0U;
       source_index < plan.sample_sources.size(); ++source_index) {
    const auto& source = plan.sample_sources[source_index];
    if (source.event_indices.size() != source.line_count ||
        source.valid_event_count !=
            inferlab::internal::checked_add_size(
                source.fixture_event_count,
                source.explicit_replay_event_count,
                "semantic event source valid row partition")) {
      throw std::logic_error(
          "product replay semantic sample accounting is inconsistent");
    }
    for (const auto event_index : source.event_indices) {
      if (event_index >= plan.normalized_events.size() ||
          plan.normalized_events[event_index].source_index != source_index ||
          event_represented[event_index]) {
        throw std::logic_error(
            "product replay semantic sample contains an invalid event index");
      }
      event_represented[event_index] = true;
    }
  }
  if (std::find(event_represented.begin(), event_represented.end(), false) !=
      event_represented.end()) {
    throw std::logic_error(
        "product replay semantic event row escaped its source inventory");
  }
  for (const auto& record : plan.records) {
    if (record.rule == nullptr || record.native_destination.artifact.empty() ||
        record.contract_check_count != 4U ||
        record.satisfied_contract_check_count >
            record.contract_check_count ||
        record.replay_ready_event_count >
            record.explicit_replay_event_count) {
      throw std::logic_error(
          "product replay semantic event record is inconsistent");
    }
    for (const auto index : record.product_profile_indices) {
      if (index >= plan.product_profiles.size()) {
        throw std::logic_error(
            "product replay semantic event record profile index invalid");
      }
    }
    for (const auto index : record.sample_source_indices) {
      if (index >= plan.sample_sources.size()) {
        throw std::logic_error(
            "product replay semantic event record source index invalid");
      }
    }
  }
}

}  // namespace

const mature_product_profiles::ProductProfileSpec& product_profile(
    const ProductProfileState& state) {
  if (state.profile == nullptr) {
    throw std::logic_error(
        "product replay semantic profile state has no profile");
  }
  return *state.profile;
}

const SemanticEventRuleSpec& semantic_event_rule(
    const NormalizedEventRow& row) {
  if (row.rule == nullptr) {
    throw std::logic_error(
        "normalized product replay event has no known rule");
  }
  return *row.rule;
}

const SemanticEventRuleSpec& semantic_event_rule(
    const SemanticEventRecord& record) {
  if (record.rule == nullptr) {
    throw std::logic_error(
        "product replay semantic event record has no rule");
  }
  return *record.rule;
}

SemanticEventsPlan build_semantic_events_plan(
    const fs::path& results_root,
    const std::vector<ReplaySampleRequest>& sample_requests) {
  return build_semantic_events_plan(
      results_root, sample_requests,
      inferlab_trace_clock::CalibrationSource{});
}

SemanticEventsPlan build_semantic_events_plan(
    const fs::path& results_root,
    const std::vector<ReplaySampleRequest>& sample_requests,
    inferlab_trace_clock::CalibrationSource clock_calibration) {
  if (results_root.empty()) {
    throw std::invalid_argument(
        "product replay semantic events results root must not be empty");
  }
  validate_requests(sample_requests);
  SemanticEventsPlan plan;
  plan.artifact_identity = semantic_events_artifact_identity();
  plan.release_pipeline = &release_pipeline_artifact_identity();
  plan.claim_boundary =
      "semantic extraction of typed fixture and explicitly imported replay events plus source-scoped anonymous request grouping, optional explicit request-arrival-to-terminal lifecycle, non-cryptographic per-request trace/request-span token inputs, W3C trace-context summaries, and optional trace-relative calibrated request windows only; raw request/trace/span identities, absolute timestamps and calibration anchors are discarded after projection, and request lifecycle, token, calibration-fingerprint or time-window compatibility does not by itself prove cryptographic identity, native same-request correlation, inferred causality, performance, execute, or release readiness";
  append_string(plan.recommended_next_commands,
                plan.artifact_identity.refresh_command);
  append_string(plan.recommended_next_commands,
                plan.release_pipeline->command);

  for (std::size_t source_index = 0U;
       source_index < sample_requests.size(); ++source_index) {
    plan.sample_sources.push_back(parse_replay_sample_source(
        sample_requests[source_index], source_index,
        plan.normalized_events));
  }

  EvidenceSnapshotRepository snapshots;
  build_profile_inventory(plan, results_root, snapshots);
  plan.request_trace = build_request_trace_projection(
      plan.normalized_events, plan.sample_sources,
      std::move(clock_calibration));
  discard_ephemeral_identity_values(plan.normalized_events);
  build_event_records(plan);
  build_product_coverage(plan);
  build_native_alignment_coverage(plan);
  build_release_pipeline_linkage(plan);
  build_summary(plan, snapshots);
  build_blockers(plan);
  validate_plan(plan, snapshots);
  return plan;
}

}  // namespace inferlab::product_replay_semantic_events
