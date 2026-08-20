#pragma once

#include <string>
#include <vector>

namespace inferlab_artifact_contract {

enum class ContractSource {
  Unspecified,
  CatalogManaged,
  MatureProductDispatch,
  DeepDiveWiringRegistry,
  ExternalEvidence,
  DerivedReport,
};

struct EvidenceRequirements {
  bool execute = false;
  bool hardware = false;
  bool product_replay = false;
};

struct Contract {
  std::string name;
  std::string artifact;
  std::string producer;
  std::string producer_target;
  std::string evidence_level;
  ContractSource source = ContractSource::Unspecified;
  EvidenceRequirements requirements;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
  std::vector<std::string> blockers;
  std::vector<std::string> refresh_commands;
  std::string claim_boundary;
};

const std::vector<Contract>& contract_extension_registry();
const Contract& require_contract_extension(const std::string& name);

}  // namespace inferlab_artifact_contract
