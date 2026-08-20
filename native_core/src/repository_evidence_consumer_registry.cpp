#include "repository_evidence_consumer_registry.hpp"

#include <cstddef>
#include <stdexcept>

namespace inferlab_repository_dag {
namespace {

void validate_registry(const std::vector<DirectConsumerSource>& sources) {
  constexpr std::size_t kExpectedSourceCount = 2U;
  if (sources.size() != kExpectedSourceCount) {
    throw std::logic_error(
        "repository direct-consumer source count drift");
  }
  for (std::size_t index = 0U; index < sources.size(); ++index) {
    const auto& source = sources[index];
    if (source.consumer_id.empty() || source.role.empty() ||
        source.catalog_id.empty() || source.claim_boundary.empty()) {
      throw std::logic_error(
          "repository direct-consumer source contains an empty field");
    }
    (void)inferlab_high_value::require_managed_artifact(source.catalog_id);
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (sources[prior].consumer_id == source.consumer_id &&
          sources[prior].role == source.role) {
        throw std::logic_error(
            "duplicate repository direct-consumer source role: " +
            source.consumer_id + "/" + source.role);
      }
    }
  }
}

}  // namespace

const std::vector<DirectConsumerSource>& direct_consumer_sources() {
  static const std::vector<DirectConsumerSource> sources = {
      {"evidence_query", "catalog", "artifact_catalog_single_source",
       DirectDependencyPhase::CurrentRepositoryGovernance, true,
       "Catalog flow metadata is current repository-governance evidence."},
      {"evidence_query", "native_audit", "native_core_audit_register",
       DirectDependencyPhase::CurrentRepositoryGovernance, true,
       "Native audit closure is current repository-governance evidence."},
  };
  static const bool validated = [] {
    validate_registry(sources);
    return true;
  }();
  (void)validated;
  return sources;
}

const DirectConsumerSource& require_direct_consumer_source(
    const std::string& consumer_id, const std::string& role) {
  const DirectConsumerSource* match = nullptr;
  for (const auto& source : direct_consumer_sources()) {
    if (source.consumer_id != consumer_id || source.role != role) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "ambiguous repository direct-consumer source role: " +
          consumer_id + "/" + role);
    }
    match = &source;
  }
  if (match == nullptr) {
    throw std::out_of_range(
        "unknown repository direct-consumer source role: " + consumer_id +
        "/" + role);
  }
  return *match;
}

const inferlab_high_value::ManagedArtifact& require_direct_consumer_artifact(
    const std::string& consumer_id, const std::string& role) {
  return inferlab_high_value::require_managed_artifact(
      require_direct_consumer_source(consumer_id, role).catalog_id);
}

}  // namespace inferlab_repository_dag
