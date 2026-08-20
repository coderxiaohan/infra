#include "catalog_derived_views.hpp"

#include "plan_json_utils.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_catalog {

using inferlab_plan::contains;

std::string raw_artifact_path(const std::string& artifact) {
  if (contains(artifact, "/")) return artifact;
  return "results/raw/" + artifact;
}

std::string strip_inferlab_prefix(const std::string& target) {
  const std::string prefix = "inferlab_";
  if (target.rfind(prefix, 0) == 0) return target.substr(prefix.size());
  return target;
}

std::string benchmark_entry_name(
    const inferlab_high_value::ManagedArtifact& artifact) {
  const std::string prefix = "native_";
  return (artifact.id.rfind(prefix, 0U) == 0U ? artifact.id
                                              : prefix + artifact.id) +
         "_refresh";
}

std::string registry_artifact_type(
    const inferlab_high_value::ManagedArtifact& artifact) {
  std::string name = artifact.artifact;
  const std::size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) name.erase(0, slash + 1U);
  constexpr char kJsonSuffix[] = ".json";
  constexpr std::size_t kJsonSuffixSize = sizeof(kJsonSuffix) - 1U;
  if (name.size() <= kJsonSuffixSize ||
      name.compare(name.size() - kJsonSuffixSize, kJsonSuffixSize,
                   kJsonSuffix) != 0) {
    throw std::logic_error(
        "managed artifact filename must be a nonempty .json file: " +
        artifact.id);
  }
  name.resize(name.size() - kJsonSuffixSize);
  return name;
}

const std::vector<std::string>& registry_allowed_modes() {
  static const std::vector<std::string> modes = [] {
    std::vector<std::string> values = {
        "plan",          "dry_run",          "dry_run_plan",
        "dry_run_ready",
        "replay",        "replay_supported", "replay_ready",
        "execute",
    };
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (values[i].empty()) {
        throw std::logic_error("catalog evidence mode must not be empty");
      }
      for (std::size_t j = i + 1U; j < values.size(); ++j) {
        if (values[i] == values[j]) {
          throw std::logic_error("duplicate catalog evidence mode: " +
                                 values[i]);
        }
      }
    }
    return values;
  }();
  return modes;
}

std::string source_path_for(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (artifact.id == "kernel_microbench_harness") {
    return "native_core/src/kernel_microbench_harness_model.cpp";
  }
  return "native_core/src/" + strip_inferlab_prefix(artifact.target) +
         "_main.cpp";
}

bool registry_requires_hardware(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (artifact.requires_hardware) return true;
  return contains(artifact.id, "kernel_microbench") ||
         contains(artifact.id, "heterogeneous_gpu") ||
         contains(artifact.evidence_level, "microbenchmark");
}

bool registry_requires_execute(
    const inferlab_high_value::ManagedArtifact& artifact) {
  return artifact.requires_execute || contains(artifact.evidence_level, "execute");
}

bool registry_requires_product_replay(
    const inferlab_high_value::ManagedArtifact& artifact) {
  return artifact.requires_product_replay;
}

std::string registry_claim_boundary(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.claim_boundary.empty()) return artifact.claim_boundary;
  if (contains(artifact.id, "microbench") || contains(artifact.id, "kernel")) {
    return "microbenchmark manifest and wiring only; timing and CUDA performance claims require target-machine execute artifacts";
  }
  if (contains(artifact.id, "fixture")) {
    return "fixture corpus evidence is synthetic/offline and cannot be reported as product execute or benchmark evidence";
  }
  if (contains(artifact.id, "release")) {
    return "release gate evidence explains readiness and blockers only; it does not execute producers or create performance claims";
  }
  if (contains(artifact.id, "query") || contains(artifact.id, "root_cause")) {
    return "query/root-cause evidence explains existing artifacts only; it does not manufacture new benchmark, replay, or execute evidence";
  }
  if (contains(artifact.id, "catalog") || contains(artifact.id, "schema")) {
    return "schema/catalog registry evidence only; downstream artifacts must be refreshed before release claims";
  }
  return "catalog-managed dry-run evidence only; benchmark, replay, and execute claims require refreshed results/raw artifacts";
}

std::string registry_freshness_policy(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.freshness_policy.empty()) return artifact.freshness_policy;
  return registry_stale_policy(artifact);
}

std::string registry_summary_group(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.summary_group.empty()) return artifact.summary_group;
  if (contains(artifact.id, "release")) return "release_governance";
  if (contains(artifact.id, "query") || contains(artifact.id, "root_cause")) {
    return "evidence_query";
  }
  if (contains(artifact.id, "fixture")) return "fixture_corpus";
  if (contains(artifact.id, "catalog") || contains(artifact.id, "schema")) {
    return "schema_catalog";
  }
  if (contains(artifact.id, "dependency")) return "dependency_governance";
  if (contains(artifact.id, "microbench") || contains(artifact.id, "kernel")) {
    return "kernel_microbenchmark";
  }
  return "evidence_governance";
}

std::string registry_final_report_group(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.final_report_group.empty()) return artifact.final_report_group;
  return registry_summary_group(artifact);
}

std::string registry_prometheus_category(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.prometheus_category.empty()) return artifact.prometheus_category;
  return registry_summary_group(artifact);
}

std::string registry_report_kind(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.report_kind.empty()) return artifact.report_kind;
  if (contains(artifact.report, "microbench") || contains(artifact.id, "kernel")) {
    return "kernel_microbenchmark_manifest";
  }
  if (contains(artifact.id, "dependency")) return "dependency_governance";
  if (contains(artifact.id, "query") || contains(artifact.id, "root_cause")) {
    return "evidence_query";
  }
  if (contains(artifact.id, "schema") || contains(artifact.id, "catalog")) {
    return "schema_catalog";
  }
  if (contains(artifact.id, "fixture")) return "fixture_corpus";
  if (contains(artifact.id, "release")) return "release_gate";
  if (contains(artifact.id, "audit")) return "audit_register";
  return "evidence_governance";
}

std::vector<std::string> registry_required_fields(
    const inferlab_high_value::ManagedArtifact& artifact) {
  std::vector<std::string> fields = {"schema_version", "status"};
  for (const auto& field : artifact.required_fields) {
    bool seen = false;
    for (const auto& existing : fields) {
      if (existing == field) {
        seen = true;
        break;
      }
    }
    if (!seen) fields.push_back(field);
  }
  return fields;
}

}  // namespace inferlab_catalog
