#pragma once

#include "native_core_audit_register_model.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_native_audit {

bool has_suffix(const std::string& value, const std::string& suffix);

FileAudit audit_file(const std::filesystem::path& repo_root,
                     const std::filesystem::path& path);

NativeSourceScan scan_native_sources(
    const std::filesystem::path& repo_root);

std::size_t source_root_issue_count(
    const std::vector<SourceRootStatus>& statuses);

std::size_t source_root_error_count(
    const std::vector<SourceRootStatus>& statuses);

std::size_t audit_scope_blocker_count(
    const std::vector<FileAudit>& audits,
    const std::vector<SourceRootStatus>& statuses);

}  // namespace inferlab_native_audit
