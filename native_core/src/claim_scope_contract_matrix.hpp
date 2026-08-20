#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_claim_scope {

inline constexpr char kClaimScopeContractArtifact[] =
    "native_claim_scope_contract.json";

struct ClaimScopeMatrixProducer {
  std::string artifact;
  std::string producer_target;
  std::string entry_name;
  std::string producer_source;
};

struct ClaimScopeMatrixInventory {
  std::vector<std::string> artifacts;
  std::vector<ClaimScopeMatrixProducer> producers;
  std::size_t ignored_output_count = 0;
  std::size_t duplicate_output_count = 0;
  std::vector<std::string> issues;
};

ClaimScopeMatrixInventory parse_claim_scope_matrix(const std::string& text);

}  // namespace inferlab_claim_scope
