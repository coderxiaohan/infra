#pragma once

#include "native_core_audit_register_model.hpp"

#include <string>
#include <vector>

namespace inferlab_native_audit {

std::string files_json(const std::vector<FileAudit>& audits, int indent);

}  // namespace inferlab_native_audit
