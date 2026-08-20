#pragma once

#include "claim_scope_contract_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab_claim_scope {

struct ClaimScopeContractViewContext {
  std::filesystem::path repo_root;
  std::filesystem::path output_path;
  std::string format = "json";
};

std::string claim_scope_contract_artifact_json(
    const ClaimScopeContractAudit& audit,
    const ClaimScopeContractViewContext& context);

}  // namespace inferlab_claim_scope
