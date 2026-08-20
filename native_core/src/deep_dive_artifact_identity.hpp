#pragma once

#include "artifact_contract_model.hpp"
#include "deep_dive_evidence_bundle_fixture.hpp"

#include <string>

namespace inferlab_deep_dive {

struct ArtifactIdentity {
  const inferlab_artifact_contract::Contract& contract;
  const inferlab_deep_dive_bundle::ArtifactWiringSpec& wiring;
};

ArtifactIdentity require_artifact_identity(const std::string& contract_name);

}  // namespace inferlab_deep_dive
