#include "release_evidence_pipeline_request.hpp"
#include "release_evidence_pipeline_stage_view.hpp"

#include <filesystem>
#include <string>

namespace inferlab_release_pipeline {
namespace {

EvidenceSnapshotRefs capture_stage_snapshots(
    const std::filesystem::path& results,
    const std::vector<PipelineStage>& stages,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  EvidenceSnapshotRefs captured;
  captured.reserve(stages.size());
  for (const auto& stage : stages) {
    captured.push_back(&snapshots.get(results / stage.artifact()));
  }
  return captured;
}

}  // namespace

ReleaseEvidencePipelineRequest::ReleaseEvidencePipelineRequest(
    const std::filesystem::path& repo_root_value,
    const std::filesystem::path& results_value,
    const std::filesystem::path& output_value,
    const std::string& format_value)
    : repo_root(repo_root_value),
      results(results_value),
      output(output_value),
      format(format_value),
      stages(default_pipeline_stages()),
      metadata_policy(pipeline_metadata_policy()),
      snapshots(metadata_policy),
      stage_snapshots(capture_stage_snapshots(results, stages, snapshots)),
      sources(results, snapshots),
      alignment(repo_root, snapshots),
      repository_dependency_graph(
          inferlab_repository_dag::build_repository_evidence_dag(repo_root)),
      native_audit_closure(sources),
      evidence_query_closure(sources),
      root_cause_source(sources),
      repair(sources),
      snapshot_diff_release_link(evidence_query_closure, repair),
      blocker_repair_link(root_cause_source, repair.paths),
      decision(stages, stage_snapshots, sources.product_replay_readiness,
               alignment, repository_dependency_graph,
               evidence_query_closure,
               blocker_repair_link,
               native_audit_closure) {}

}  // namespace inferlab_release_pipeline
