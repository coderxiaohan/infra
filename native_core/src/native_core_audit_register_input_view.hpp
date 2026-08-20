#pragma once

#include "native_core_audit_register_model.hpp"

#include <string>
#include <vector>

namespace inferlab_native_audit {

std::string source_roots_json(
    const std::vector<SourceRootStatus>& statuses, int indent);

std::string review_manifest_json(const ReviewManifest& manifest, int indent);

std::string audit_scope_json(int indent);

}  // namespace inferlab_native_audit
