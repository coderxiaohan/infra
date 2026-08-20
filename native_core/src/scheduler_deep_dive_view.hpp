#pragma once

#include "scheduler_deep_dive_model.hpp"

#include <string>
#include <vector>

namespace inferlab::scheduler_deep_dive {

struct ArtifactContract {
  ArtifactRole role = ArtifactRole::batching;
  std::string artifact_type;
  std::string target_name;
  std::string artifact_path;
  std::string refresh_command;
  std::string deep_dive_type;
  std::string claim_scope;
  std::vector<std::string> research_basis;
  std::vector<std::string> product_alignment;
  std::vector<std::string> evidence_bridge_inputs;
};

const ArtifactContract& artifact_contract(ArtifactRole role);
std::string artifact_json(const Plan& plan);

}  // namespace inferlab::scheduler_deep_dive
