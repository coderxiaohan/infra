#include "deep_dive_artifact_identity.hpp"

#include <stdexcept>

namespace inferlab_deep_dive {

ArtifactIdentity require_artifact_identity(
    const std::string& contract_name) {
  const auto& contract =
      inferlab_artifact_contract::require_contract_extension(contract_name);
  const auto& wiring =
      inferlab_deep_dive_bundle::require_artifact_wiring_spec(contract_name);
  if (contract.source != inferlab_artifact_contract::ContractSource::
                             DeepDiveWiringRegistry ||
      contract.name != wiring.contract_name ||
      contract.artifact != "results/raw/" + wiring.artifact ||
      contract.producer_target != wiring.target ||
      contract.refresh_commands.size() != 1U ||
      contract.refresh_commands.front() != wiring.command ||
      contract.required_fields.empty() || contract.required_metrics.empty() ||
      contract.claim_boundary.empty() || wiring.source_path.empty() ||
      wiring.report_path.empty() || wiring.matrix_entry.empty()) {
    throw std::logic_error(
        "Deep-Dive artifact identity is incomplete or drifted: " +
        contract_name);
  }
  return {contract, wiring};
}

}  // namespace inferlab_deep_dive
