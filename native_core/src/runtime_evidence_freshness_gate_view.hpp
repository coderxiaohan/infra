#pragma once

#include "runtime_evidence_freshness_gate_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab_freshness_gate {

std::string runtime_evidence_freshness_gate_artifact_json(
    const RuntimeEvidenceFreshnessGateRequest& request,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& resolved_output_path,
    const std::string& reference_age_arg, const std::string& format);

}  // namespace inferlab_freshness_gate
