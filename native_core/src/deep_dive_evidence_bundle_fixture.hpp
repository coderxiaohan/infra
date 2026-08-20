#pragma once

#include <string>
#include <vector>

namespace inferlab_deep_dive_bundle {

struct ArtifactWiringSpec {
  std::string contract_name;
  std::string artifact;
  std::string legacy_bundle_name;
  std::string source_path;
  std::string report_path;
  std::string target;
  std::string matrix_entry;
  std::string command;
};

const std::vector<ArtifactWiringSpec>& artifact_wiring_specs();
const ArtifactWiringSpec& require_artifact_wiring_spec(
    const std::string& contract_name);

}  // namespace inferlab_deep_dive_bundle
