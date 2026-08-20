#pragma once

#include "native_core_audit_register_model.hpp"

#include <filesystem>
#include <vector>

namespace inferlab_native_audit {

ReviewManifest read_review_manifest(
    const std::filesystem::path& manifest_path);

void apply_review_manifest_overrides(
    std::vector<FileAudit>& audits,
    ReviewManifest& manifest);

}  // namespace inferlab_native_audit
