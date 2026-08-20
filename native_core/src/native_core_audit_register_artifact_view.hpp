#pragma once

#include "native_core_audit_register_request.hpp"

#include <string>

namespace inferlab_native_audit {

std::string native_core_audit_register_artifact_json(
    const NativeCoreAuditRegisterRequest& request,
    const std::string& output_arg);

}  // namespace inferlab_native_audit
