#include "runtime_evidence_freshness_gate_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ratio>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace inferlab_freshness_gate {
namespace {

constexpr std::size_t kMaximumFreshnessTargetCount = 4096;

void validate_target(const EvidenceTarget& target, const char* source) {
  if (target.name.empty() || target.artifact.empty() ||
      target.evidence_class.empty() || target.consumer.empty() ||
      target.refresh_command.empty() || target.stale_policy.empty()) {
    throw std::runtime_error(std::string(source) +
                             " freshness target has an empty contract field");
  }
  if (target.max_age_hours <= 0) {
    throw std::runtime_error(std::string(source) +
                             " freshness target max_age_hours must be positive");
  }
}

void require_unique_artifacts(const std::vector<EvidenceTarget>& targets,
                              const char* source) {
  if (targets.size() > kMaximumFreshnessTargetCount) {
    throw std::runtime_error(std::string(source) +
                             " freshness target count exceeds the bounded limit");
  }
  std::unordered_set<std::string> artifacts;
  artifacts.reserve(targets.size());
  for (const auto& target : targets) {
    validate_target(target, source);
    if (!artifacts.insert(target.artifact).second) {
      throw std::runtime_error(std::string(source) +
                               " freshness target artifact is duplicated: " +
                               target.artifact);
    }
  }
}

EvidenceTarget catalog_target(const inferlab_catalog::FreshnessView& view) {
  constexpr char kJsonSuffix[] = ".json";
  constexpr std::size_t kJsonSuffixLength = sizeof(kJsonSuffix) - 1U;
  if (view.artifact.size() <= kJsonSuffixLength ||
      view.artifact.compare(view.artifact.size() - kJsonSuffixLength,
                            kJsonSuffixLength, kJsonSuffix) != 0) {
    throw std::runtime_error(
        "catalog freshness artifact must use a .json identity");
  }
  const std::string artifact_name =
      view.artifact.substr(0, view.artifact.size() - kJsonSuffixLength);
  return {artifact_name,
          view.artifact,
          view.evidence_class,
          view.consumer,
          view.max_age_hours,
          view.requires_execute,
          view.requires_hardware,
          view.refresh_command,
          view.stale_policy};
}

FreshnessTargetProjection build_target_projection() {
  FreshnessTargetProjection projection;
  projection.targets = unmanaged_freshness_targets();
  projection.unmanaged_target_count = projection.targets.size();
  require_unique_artifacts(projection.targets, "unmanaged");

  const auto catalog_views = inferlab_catalog::freshness_views();
  if (catalog_views.size() > kMaximumFreshnessTargetCount) {
    throw std::runtime_error(
        "catalog freshness target count exceeds the bounded limit");
  }
  projection.registry_target_count = catalog_views.size();

  std::unordered_set<std::string> catalog_artifacts;
  catalog_artifacts.reserve(catalog_views.size());
  for (const auto& view : catalog_views) {
    EvidenceTarget authoritative = catalog_target(view);
    validate_target(authoritative, "catalog");
    if (!catalog_artifacts.insert(authoritative.artifact).second) {
      throw std::runtime_error(
          "catalog freshness target artifact is duplicated: " +
          authoritative.artifact);
    }

    const auto existing = std::find_if(
        projection.targets.begin(), projection.targets.end(),
        [&](const EvidenceTarget& target) {
          return target.artifact == authoritative.artifact;
        });
    if (existing != projection.targets.end()) {
      throw std::runtime_error(
          "unmanaged/catalog freshness target overlap is forbidden: " +
          authoritative.artifact);
    }
    projection.targets.push_back(std::move(authoritative));
    ++projection.registry_consumed_target_count;
  }
  if (projection.registry_consumed_target_count !=
      projection.registry_target_count) {
    throw std::runtime_error(
        "catalog freshness target consumption did not reconcile");
  }
  if (projection.targets.size() > kMaximumFreshnessTargetCount) {
    throw std::runtime_error(
        "merged freshness target count exceeds the bounded limit");
  }
  std::sort(projection.targets.begin(), projection.targets.end(),
            [](const EvidenceTarget& lhs, const EvidenceTarget& rhs) {
              return lhs.artifact < rhs.artifact;
            });
  require_unique_artifacts(projection.targets, "merged");
  return projection;
}

FreshnessRecord analyze_target(const EvidenceTarget& target,
                               const std::filesystem::path& results_root,
                               const double reference_age_hours) {
  FreshnessRecord record;
  record.target = target;
  record.resolved_path = results_root / target.artifact;

  const inferlab_artifact::TextReadResult read_result =
      inferlab_artifact::read_text_with_status(record.resolved_path);
  record.present = read_result.present;
  record.path_probe_status =
      read_result.status == "unreadable_file_probe"
          ? "error"
          : (read_result.present ? "present" : "missing");
  record.path_probe_error =
      read_result.status == "unreadable_file_probe" ? read_result.error : "";
  record.text_read_status =
      read_result.present ? read_result.status : "not_attempted";
  record.text_read_error =
      read_result.status == "unreadable_file_probe" ? "" : read_result.error;
  if (!record.present) {
    if (record.path_probe_status == "error") {
      record.freshness_state = "path_probe_failed";
      record.recommendation =
          "investigate_artifact_path_probe_error_before_freshness_gate";
      record.release_blocking = true;
    } else {
      record.freshness_state = "missing";
      record.recommendation =
          "generate_or_refresh_artifact_before_release_claim";
      record.release_blocking = target.requires_execute || target.requires_hardware;
    }
    return record;
  }
  if (!read_result.readable) {
    record.freshness_state = "unreadable";
    record.recommendation =
        "repair_artifact_readability_or_regenerate_before_release_claim";
    record.release_blocking = true;
    return record;
  }

  if (reference_age_hours >= 0.0) {
    record.age_known = true;
    record.age_hours = reference_age_hours;
    record.mtime_probe_status = "skipped_reference_age";
  } else {
    std::error_code error;
    const auto modified =
        std::filesystem::last_write_time(record.resolved_path, error);
    if (!error) {
      record.mtime_probe_status = "ok";
      const auto now = std::filesystem::file_time_type::clock::now();
      record.age_hours =
          std::chrono::duration<double, std::ratio<3600>>(now - modified)
              .count();
      record.age_known =
          std::isfinite(record.age_hours) && record.age_hours >= 0.0;
    } else {
      record.mtime_probe_status = "error";
      record.mtime_probe_error = inferlab_plan::redact_error_message(
          error.message(), {record.resolved_path.generic_string()});
    }
  }

  if (!record.age_known) {
    record.freshness_state = "unknown_age";
    record.freshness_score = 0.5;
    record.stale = true;
    record.release_blocking = true;
    record.recommendation =
        record.mtime_probe_status == "error"
            ? "investigate_artifact_mtime_probe_error_before_release_claim"
            : "inspect_artifact_timestamp_before_release_claim";
    return record;
  }

  record.stale =
      record.age_hours > static_cast<double>(target.max_age_hours);
  if (record.stale) {
    record.freshness_state = "stale";
    const double overage =
        record.age_hours - static_cast<double>(target.max_age_hours);
    const double denominator = std::max(
        1.0, static_cast<double>(target.max_age_hours) * 2.0);
    record.freshness_score =
        inferlab_plan::clamp01(1.0 - overage / denominator);
    record.release_blocking = target.requires_execute || target.requires_hardware;
    record.recommendation =
        target.requires_execute
            ? "rerun_on_target_environment_before_claiming_metrics"
            : "refresh_derived_artifact_before_release";
  } else {
    record.freshness_state = "fresh";
    record.freshness_score = inferlab_plan::clamp01(
        1.0 - record.age_hours /
                  std::max(1.0,
                           static_cast<double>(target.max_age_hours) * 2.0));
    record.release_blocking = false;
    record.recommendation = "artifact_freshness_ok";
  }
  return record;
}

FreshnessSummary summarize(const std::vector<FreshnessRecord>& records) {
  FreshnessSummary summary;
  double score_sum = 0.0;
  double lowest_score = 2.0;
  for (const auto& record : records) {
    summary.present_count += record.present ? 1U : 0U;
    summary.fresh_count += record.freshness_state == "fresh" ? 1U : 0U;
    summary.stale_count += record.freshness_state == "stale" ? 1U : 0U;
    summary.missing_count += record.freshness_state == "missing" ? 1U : 0U;
    summary.path_probe_failed_count +=
        record.freshness_state == "path_probe_failed" ? 1U : 0U;
    summary.unreadable_count +=
        record.freshness_state == "unreadable" ? 1U : 0U;
    summary.unknown_age_count +=
        record.freshness_state == "unknown_age" ? 1U : 0U;
    summary.release_blocking_count += record.release_blocking ? 1U : 0U;
    summary.execute_required_count +=
        record.target.requires_execute ? 1U : 0U;
    summary.hardware_required_count +=
        record.target.requires_hardware ? 1U : 0U;
    if (!std::isfinite(record.freshness_score)) {
      throw std::runtime_error("freshness score must be finite");
    }
    score_sum += record.freshness_score;
    if (!std::isfinite(score_sum)) {
      throw std::overflow_error("freshness score sum is non-finite");
    }
    if (record.freshness_score < lowest_score) {
      lowest_score = record.freshness_score;
      summary.most_urgent_target = record.target.name;
    }
  }

  summary.stale_or_missing_count = inferlab::internal::checked_sum_size(
      {summary.stale_count, summary.missing_count,
       summary.path_probe_failed_count, summary.unreadable_count,
       summary.unknown_age_count},
      "freshness stale or missing count");
  const std::size_t classified_count = inferlab::internal::checked_sum_size(
      {summary.fresh_count, summary.stale_count, summary.missing_count,
       summary.path_probe_failed_count, summary.unreadable_count,
       summary.unknown_age_count},
      "freshness classified record count");
  const std::size_t present_state_count =
      inferlab::internal::checked_sum_size(
          {summary.fresh_count, summary.stale_count,
           summary.unreadable_count, summary.unknown_age_count},
          "freshness present-state record count");
  if (classified_count != records.size() ||
      present_state_count != summary.present_count ||
      summary.release_blocking_count > records.size() ||
      summary.execute_required_count > records.size() ||
      summary.hardware_required_count > records.size()) {
    throw std::runtime_error(
        "freshness summary cardinalities did not reconcile");
  }
  summary.freshness_score =
      records.empty() ? 0.0
                      : score_sum / static_cast<double>(records.size());
  summary.release_decision =
      summary.release_blocking_count > 0
          ? "runtime_claims_blocked_until_refresh"
          : (summary.stale_or_missing_count > 0
                 ? "derived_reports_need_refresh"
                 : "fresh_for_report_claims");
  return summary;
}

}  // namespace

FreshnessTargetProjection project_freshness_targets() {
  return build_target_projection();
}

RuntimeEvidenceFreshnessGateRequest::RuntimeEvidenceFreshnessGateRequest(
    std::filesystem::path results_root, const double reference_age_hours)
    : results_root_(std::move(results_root)),
      reference_age_hours_(reference_age_hours),
      target_projection_(project_freshness_targets()) {
  if (!std::isfinite(reference_age_hours_) ||
      (reference_age_hours_ < 0.0 && reference_age_hours_ != -1.0)) {
    throw std::invalid_argument(
        "reference age must be -1 or a finite non-negative value");
  }
  records_.reserve(target_projection_.targets.size());
  for (const auto& target : target_projection_.targets) {
    records_.push_back(
        analyze_target(target, results_root_, reference_age_hours_));
  }
  summary_ = summarize(records_);
}

}  // namespace inferlab_freshness_gate
