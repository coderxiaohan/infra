#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_freshness_gate {

struct EvidenceTarget {
  std::string name;
  std::string artifact;
  std::string evidence_class;
  std::string consumer;
  int max_age_hours = 24;
  bool requires_execute = false;
  bool requires_hardware = false;
  std::string refresh_command;
  std::string stale_policy;
};

struct FreshnessRecord {
  EvidenceTarget target;
  std::filesystem::path resolved_path;
  bool present = false;
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string text_read_status = "not_attempted";
  std::string text_read_error;
  std::string mtime_probe_status = "not_attempted";
  std::string mtime_probe_error;
  bool age_known = false;
  double age_hours = 0.0;
  double freshness_score = 0.0;
  bool stale = true;
  bool release_blocking = false;
  std::string freshness_state;
  std::string recommendation;
};

struct FreshnessTargetProjection {
  std::vector<EvidenceTarget> targets;
  std::size_t registry_target_count = 0;
  std::size_t registry_consumed_target_count = 0;
  std::size_t unmanaged_target_count = 0;
};

struct FreshnessSummary {
  std::size_t present_count = 0;
  std::size_t fresh_count = 0;
  std::size_t stale_count = 0;
  std::size_t missing_count = 0;
  std::size_t path_probe_failed_count = 0;
  std::size_t unreadable_count = 0;
  std::size_t unknown_age_count = 0;
  std::size_t release_blocking_count = 0;
  std::size_t execute_required_count = 0;
  std::size_t hardware_required_count = 0;
  std::size_t stale_or_missing_count = 0;
  double freshness_score = 0.0;
  std::string most_urgent_target;
  std::string release_decision;
};

std::vector<EvidenceTarget> unmanaged_freshness_targets();
FreshnessTargetProjection project_freshness_targets();

class RuntimeEvidenceFreshnessGateRequest {
 public:
  RuntimeEvidenceFreshnessGateRequest(std::filesystem::path results_root,
                                      double reference_age_hours);

  RuntimeEvidenceFreshnessGateRequest(
      const RuntimeEvidenceFreshnessGateRequest&) = delete;
  RuntimeEvidenceFreshnessGateRequest& operator=(
      const RuntimeEvidenceFreshnessGateRequest&) = delete;
  RuntimeEvidenceFreshnessGateRequest(
      RuntimeEvidenceFreshnessGateRequest&&) = delete;
  RuntimeEvidenceFreshnessGateRequest& operator=(
      RuntimeEvidenceFreshnessGateRequest&&) = delete;

  const std::filesystem::path& results_root() const { return results_root_; }
  double reference_age_hours() const { return reference_age_hours_; }
  const FreshnessTargetProjection& target_projection() const {
    return target_projection_;
  }
  const std::vector<FreshnessRecord>& records() const { return records_; }
  const FreshnessSummary& summary() const { return summary_; }

 private:
  std::filesystem::path results_root_;
  double reference_age_hours_ = -1.0;
  FreshnessTargetProjection target_projection_;
  std::vector<FreshnessRecord> records_;
  FreshnessSummary summary_;
};

}  // namespace inferlab_freshness_gate
