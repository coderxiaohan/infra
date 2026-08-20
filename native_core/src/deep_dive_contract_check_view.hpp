#pragma once

#include "deep_dive_contract_check_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab_deep_dive_contract {

std::string artifact_json(const Plan& plan,
                          const std::filesystem::path& repo_root,
                          const std::filesystem::path& results_root,
                          const std::filesystem::path& output_path,
                          const std::string& format);

}  // namespace inferlab_deep_dive_contract
