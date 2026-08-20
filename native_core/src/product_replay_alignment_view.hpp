#pragma once

#include "product_replay_alignment_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab::product_replay_alignment {

struct AlignmentArtifactViewContext {
  std::filesystem::path repo_root;
  std::filesystem::path results_root;
  std::filesystem::path output;
  std::string format = "json";
};

std::string alignment_artifact_json(
    const AlignmentPlan& plan,
    const AlignmentArtifactViewContext& context);

}  // namespace inferlab::product_replay_alignment
