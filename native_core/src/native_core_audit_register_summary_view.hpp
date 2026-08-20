#pragma once

#include "native_core_audit_register_request.hpp"

#include <string>

namespace inferlab_native_audit {

std::string native_core_audit_summary_json(
    const NativeCoreAuditRegisterRequest& request, int indent);

}  // namespace inferlab_native_audit
