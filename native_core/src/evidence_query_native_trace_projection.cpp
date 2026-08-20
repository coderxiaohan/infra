#include "evidence_query_native_trace_projection.hpp"

#include "artifact_evidence_contract.hpp"
#include "artifact_semantic_index.hpp"
#include "catalog_derived_views.hpp"
#include "high_value_artifact_catalog.hpp"
#include "release_evidence_pipeline_stage.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

struct OwnedTraceSource {
  std::string artifact;
  std::string artifact_type;
  std::string refresh_command;
  std::vector<std::string> canonical_required_metrics;
  const inferlab_evidence::EvidenceSnapshot* snapshot = nullptr;
  inferlab_artifact::ArtifactSemanticIndex semantic_index;
  bool structural_ready = false;
};

bool contains_value(const std::vector<std::string>& values,
                    const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

std::string artifact_type_from_filename(const std::string& artifact) {
  constexpr std::size_t kJsonSuffixSize = 5U;
  if (artifact.size() <= kJsonSuffixSize ||
      artifact.compare(artifact.size() - kJsonSuffixSize, kJsonSuffixSize,
                       ".json") != 0 ||
      artifact.find('/') != std::string::npos ||
      artifact.find('\\') != std::string::npos ||
      artifact.find("..") != std::string::npos) {
    throw std::logic_error(
        "evidence-query native trace source filename is invalid");
  }
  return artifact.substr(0U, artifact.size() - kJsonSuffixSize);
}

inferlab_evidence::ArtifactContractPolicy trace_contract_policy() {
  inferlab_evidence::ArtifactContractPolicy policy;
  policy.require_clear_blockers = false;
  return policy;
}

OwnedTraceSource capture_trace_source(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots,
    std::string artifact, std::string refresh_command,
    std::vector<std::string> canonical_required_metrics) {
  OwnedTraceSource source;
  source.artifact = std::move(artifact);
  source.artifact_type = artifact_type_from_filename(source.artifact);
  source.refresh_command = std::move(refresh_command);
  source.canonical_required_metrics =
      std::move(canonical_required_metrics);
  if (source.refresh_command.empty()) {
    throw std::logic_error(
        "evidence-query native trace refresh command is empty");
  }

  source.snapshot = &snapshots.get(results_path / source.artifact);
  source.semantic_index = inferlab_artifact::build_artifact_semantic_index(
      source.snapshot->read_result.text);
  const auto contract = inferlab_evidence::evaluate_artifact_contract(
      *source.snapshot, trace_contract_policy());
  source.structural_ready =
      contract.ready && source.semantic_index.parse_ok &&
      source.snapshot->metadata.artifact_type == source.artifact_type;
  return source;
}

const inferlab_release_pipeline::PipelineStage& product_trace_stage(
    const std::vector<inferlab_release_pipeline::PipelineStage>& stages) {
  const inferlab_release_pipeline::PipelineStage* selected = nullptr;
  for (const auto& stage : stages) {
    if (stage.id != "product_replay_semantics") continue;
    if (selected != nullptr) {
      throw std::logic_error(
          "evidence-query native trace resolved duplicate product stages");
    }
    selected = &stage;
  }
  if (selected == nullptr) {
    throw std::logic_error(
        "evidence-query native trace product stage is missing");
  }
  return *selected;
}

}  // namespace

inferlab_deep_dive_trace::TraceTokenJoinProjection
project_evidence_query_native_trace_correlation(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  inferlab_high_value::validate_managed_artifact_registry();
  const auto& managed = inferlab_high_value::managed_artifacts();
  const auto stages = inferlab_release_pipeline::default_pipeline_stages();

  std::vector<OwnedTraceSource> sources;
  sources.reserve(managed.size() + 1U);
  const auto& product = product_trace_stage(stages);
  sources.push_back(capture_trace_source(
      results_path, snapshots, product.artifact(), product.refresh_command(),
      {}));

  for (const auto& artifact : managed) {
    if (!contains_value(artifact.required_fields, "native_trace_context")) {
      continue;
    }
    auto metrics = inferlab_catalog::registry_required_metrics(artifact);
    if (!contains_value(metrics, "native_trace_context_decision")) {
      throw std::logic_error(
          "catalog trace source is missing native trace contract metrics: " +
          artifact.id);
    }
    sources.push_back(capture_trace_source(
        results_path, snapshots, artifact.artifact, artifact.command,
        std::move(metrics)));
  }
  if (sources.size() <= 1U) {
    throw std::logic_error(
        "evidence-query native trace catalog source inventory is empty");
  }

  std::set<std::string> unique_artifacts;
  std::vector<inferlab_deep_dive_trace::TraceTokenArtifactInput> inputs;
  inputs.reserve(sources.size());
  for (const auto& source : sources) {
    if (!unique_artifacts.insert(source.artifact).second) {
      throw std::logic_error(
          "evidence-query native trace artifact inventory is duplicated");
    }
    inputs.push_back(
        {source.artifact,
         source.artifact_type,
         source.refresh_command,
         source.canonical_required_metrics,
         source.structural_ready,
         &source.semantic_index,
         &source.snapshot->read_result.text});
  }
  return inferlab_deep_dive_trace::build_trace_token_join_projection(inputs);
}

}  // namespace inferlab_evidence_query
