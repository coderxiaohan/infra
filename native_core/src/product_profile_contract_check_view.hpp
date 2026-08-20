#pragma once

#include "product_profile_contract_check_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab::product_profile_contract_check {

struct ArtifactViewContext {
  std::filesystem::path repo_root;
  std::filesystem::path results_root;
  std::filesystem::path output;
  std::string format = "json";
};

std::string artifact_json(const AuditPlan& plan,
                          const ArtifactViewContext& context);

}  // namespace inferlab::product_profile_contract_check
