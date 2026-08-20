#pragma once

#include <string>
#include <vector>

namespace inferlab::product_replay_alignment {

enum class NativeIdentitySource {
  core_deep_dive_registry,
  deep_dive_wiring_registry,
  high_value_catalog,
  mature_product_dispatch_contract,
};

struct NativeArtifactSelector {
  std::string role;
  NativeIdentitySource source =
      NativeIdentitySource::deep_dive_wiring_registry;
  std::string foreign_key;
  std::string family;
};

struct NativeArtifactIdentity {
  std::string role;
  NativeIdentitySource source =
      NativeIdentitySource::deep_dive_wiring_registry;
  std::string canonical_id;
  std::string artifact;
  std::string family;
  std::string expected_artifact_type;
  std::string identity_owner;
  std::string producer_target;
  std::string refresh_command;
  std::string claim_boundary;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
};

struct AlignmentRuleSpec {
  std::string name;
  std::string alignment_type;
  std::vector<std::string> product_profile_ids;
  std::vector<NativeArtifactSelector> native_artifacts;
  std::vector<std::string> replay_signals;
  std::vector<std::string> native_explainers;
  std::string claim_boundary;
  std::string next_action_when_missing;
};

const char* native_identity_source_name(NativeIdentitySource source);
NativeArtifactIdentity resolve_native_artifact(
    const NativeArtifactSelector& selector);
const std::vector<AlignmentRuleSpec>& alignment_rules();

}  // namespace inferlab::product_replay_alignment
