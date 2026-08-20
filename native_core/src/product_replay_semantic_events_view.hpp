#pragma once

#include "product_replay_semantic_events_model.hpp"

#include <filesystem>
#include <string>

namespace inferlab::product_replay_semantic_events {

struct SemanticEventsArtifactViewContext {
  std::filesystem::path repo_root;
  std::filesystem::path results_root;
  std::filesystem::path output;
  std::string format = "json";
};

std::string semantic_events_artifact_json(
    const SemanticEventsPlan& plan,
    const SemanticEventsArtifactViewContext& context);

}  // namespace inferlab::product_replay_semantic_events
