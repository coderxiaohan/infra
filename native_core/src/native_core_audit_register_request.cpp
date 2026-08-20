#include "native_core_audit_register_request.hpp"

#include "native_core_audit_register_manifest.hpp"
#include "native_core_audit_register_source.hpp"

#include <utility>

namespace inferlab_native_audit {

NativeCoreAuditRegisterRequest::NativeCoreAuditRegisterRequest(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& review_manifest_path) {
  NativeSourceScan source_scan = scan_native_sources(repo_root);
  source_roots_ = std::move(source_scan.roots);
  audits_.reserve(source_scan.files.size());
  for (const auto& file : source_scan.files) {
    audits_.push_back(audit_file(repo_root, file));
  }
  review_manifest_ = read_review_manifest(review_manifest_path);
  apply_review_manifest_overrides(audits_, review_manifest_);
  scope_blocker_count_ =
      audit_scope_blocker_count(audits_, source_roots_);
  summary_ = project_native_audit_summary(
      audits_, source_roots_, review_manifest_, scope_blocker_count_);
}

}  // namespace inferlab_native_audit
