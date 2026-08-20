#pragma once

#include "high_value_artifact_catalog.hpp"

#include <string>
#include <vector>

namespace inferlab_repository_dag {

enum class DirectDependencyPhase {
  CurrentAuthoritative,
  CurrentRepositoryGovernance,
};

struct DirectConsumerSource {
  std::string consumer_id;
  std::string role;
  std::string catalog_id;
  DirectDependencyPhase phase =
      DirectDependencyPhase::CurrentAuthoritative;
  bool blocks_current_decision = true;
  std::string claim_boundary;
};

const std::vector<DirectConsumerSource>& direct_consumer_sources();
const DirectConsumerSource& require_direct_consumer_source(
    const std::string& consumer_id, const std::string& role);
const inferlab_high_value::ManagedArtifact& require_direct_consumer_artifact(
    const std::string& consumer_id, const std::string& role);
}  // namespace inferlab_repository_dag
