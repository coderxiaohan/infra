#pragma once

#include "dependency_governance_model.hpp"

#include <string>

namespace inferlab::dependency_governance {

enum class ArtifactView {
  policy_gate,
  sbom_gate,
};

std::string artifact_json(const Plan& plan, ArtifactView view);

}  // namespace inferlab::dependency_governance
