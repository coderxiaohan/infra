#pragma once

#include "native_core_audit_register_model.hpp"
#include "native_core_audit_register_summary_projection.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace inferlab_native_audit {

class NativeCoreAuditRegisterRequest {
 public:
  NativeCoreAuditRegisterRequest(
      const std::filesystem::path& repo_root,
      const std::filesystem::path& review_manifest_path);

  NativeCoreAuditRegisterRequest(const NativeCoreAuditRegisterRequest&) =
      delete;
  NativeCoreAuditRegisterRequest& operator=(
      const NativeCoreAuditRegisterRequest&) = delete;
  NativeCoreAuditRegisterRequest(NativeCoreAuditRegisterRequest&&) = delete;
  NativeCoreAuditRegisterRequest& operator=(NativeCoreAuditRegisterRequest&&) =
      delete;

  const std::vector<SourceRootStatus>& source_roots() const noexcept {
    return source_roots_;
  }

  const std::vector<FileAudit>& audits() const noexcept { return audits_; }

  const ReviewManifest& review_manifest() const noexcept {
    return review_manifest_;
  }

  std::size_t scope_blocker_count() const noexcept {
    return scope_blocker_count_;
  }

  const NativeAuditSummaryProjection& summary() const noexcept {
    return summary_;
  }

 private:
  std::vector<SourceRootStatus> source_roots_;
  std::vector<FileAudit> audits_;
  ReviewManifest review_manifest_;
  std::size_t scope_blocker_count_ = 0U;
  NativeAuditSummaryProjection summary_;
};

}  // namespace inferlab_native_audit
