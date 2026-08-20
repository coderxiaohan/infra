#pragma once

#include "product_integration_inventory_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab::product_integration_inventory {

struct ArtifactViewContext {
  std::filesystem::path repo_root;
  std::filesystem::path output;
};

std::string artifact_json(const InventoryPlan& plan,
                          const ArtifactViewContext& context);

}  // namespace inferlab::product_integration_inventory
