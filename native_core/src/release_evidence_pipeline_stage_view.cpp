#include "release_evidence_pipeline_stage_view.hpp"

#include "checked_index.hpp"
#include "evidence_domain_utils.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

void require_aligned_stages(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots) {
  if (stages.size() != snapshots.size()) {
    throw std::logic_error(
        "release pipeline stage and snapshot counts must match");
  }
  for (const auto* snapshot : snapshots) {
    if (snapshot == nullptr) {
      throw std::logic_error(
          "release pipeline stage snapshot must not be null");
    }
  }
}

std::vector<std::string> release_dimension_order(
    const std::vector<PipelineStage>& stages) {
  std::vector<std::string> dimensions;
  dimensions.reserve(stages.size());
  for (const auto& stage : stages) {
    if (!stage.current_decision_input()) continue;
    inferlab_plan::append_unique_nonempty_string(dimensions,
                                                 stage.evidence_role);
  }
  return dimensions;
}

std::string stage_json(
    const PipelineStage& stage,
    const inferlab_evidence::EvidenceSnapshot& snapshot,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string category = repair_category(stage, snapshot);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(stage.id)
      << ",\n";
  out << nested << "\"stage\": " << inferlab_plan::json_string(stage.stage)
      << ",\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(stage.artifact()) << ",\n";
  out << nested << "\"evidence_role\": "
      << inferlab_plan::json_string(stage.evidence_role) << ",\n";
  out << nested << "\"required\": " << inferlab_plan::json_bool(stage.required)
      << ",\n";
  out << nested << "\"dependency_phase\": "
      << inferlab_plan::json_string(pipeline_stage_phase_name(stage.phase))
      << ",\n";
  out << nested << "\"blocks_current_decision\": "
      << inferlab_plan::json_bool(stage.current_decision_input() &&
                                  stage.required)
      << ",\n";
  out << nested << "\"historical_snapshot_only\": "
      << inferlab_plan::json_bool(stage.post_generation_consumer())
      << ",\n";
  out << nested << "\"ready\": "
      << inferlab_plan::json_bool(
             inferlab_evidence::evidence_snapshot_ready(snapshot))
      << ",\n";
  out << nested << "\"repair_category\": "
      << inferlab_plan::json_string(category) << ",\n";
  out << nested << "\"repair_environment\": "
      << inferlab_plan::json_string(
             inferlab_evidence::repair_environment(category))
      << ",\n";
  out << nested << "\"evidence_confidence\": "
      << inferlab_plan::json_string(snapshot.confidence) << ",\n";
  out << nested << "\"refresh_command\": "
      << inferlab_plan::json_string(stage.refresh_command()) << ",\n";
  out << nested << "\"catalog_managed\": "
      << inferlab_plan::json_bool(stage.catalog_managed()) << ",\n";
  out << nested << "\"identity_source\": "
      << inferlab_plan::json_string(stage.identity_source()) << ",\n";
  out << nested << "\"fallback_retained\": "
      << inferlab_plan::json_bool(stage.fallback_retained()) << ",\n";
  out << nested << "\"fallback_owner\": "
      << (stage.fallback_owner.empty()
              ? "null"
              : inferlab_plan::json_string(stage.fallback_owner))
      << ",\n";
  out << nested << "\"fallback_removal_condition\": "
      << (stage.fallback_removal_condition.empty()
              ? "null"
              : inferlab_plan::json_string(
                    stage.fallback_removal_condition))
      << ",\n";
  out << nested << "\"drift_status\": "
      << inferlab_plan::json_string(stage.drift_status()) << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(stage.next_action()) << ",\n";
  out << nested << "\"metadata\": "
      << inferlab_artifact::metadata_json(snapshot.metadata, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

inferlab_evidence::MetadataBlockerPolicy pipeline_metadata_policy() {
  inferlab_evidence::MetadataBlockerPolicy policy;
  policy.required_field_priority =
      inferlab_evidence::RequiredFieldPriority::kSummaryBeforeClaimScope;
  policy.blocked_category =
      inferlab_evidence::BlockerCategory::kUpstreamBlocker;
  return policy;
}

std::string repair_category(
    const PipelineStage& stage,
    const inferlab_evidence::EvidenceSnapshot& snapshot) {
  if (snapshot.assessment.blocker_category !=
      inferlab_evidence::BlockerCategory::kNone) {
    return inferlab_evidence::blocker_category_name(
        snapshot.assessment.blocker_category);
  }
  if (stage.stage == "product_replay" && !snapshot.metadata.replayed) {
    return "replay_missing";
  }
  if ((inferlab_evidence::contains(stage.artifact(), "microbench") ||
       inferlab_evidence::contains(stage.artifact(), "cuda") ||
       inferlab_evidence::contains(stage.artifact(), "kernel")) &&
      !snapshot.metadata.executed) {
    return "execute_missing";
  }
  return "none";
}

std::string repair_action(const PipelineStage& stage,
                          const std::string& category) {
  if (category == "artifact_missing") return stage.refresh_command();
  if (category == "schema_missing") {
    return "refresh producer with required schema/mode/executed/replayed/claim_scope fields: " +
           stage.refresh_command();
  }
  if (category == "summary_missing") {
    return "wire artifact into summary/report surfaces before report_ready claims";
  }
  if (category == "claim_scope_missing") {
    return "add claim_scope or claim_scope_detail to producer artifact";
  }
  if (category == "upstream_blocker") {
    return "inspect upstream blockers, repair input artifacts, then refresh " +
           stage.artifact();
  }
  if (category == "replay_missing") {
    return "attach product replay evidence or run semantic replay extraction on target evidence";
  }
  if (category == "execute_missing") {
    return "run target-machine correctness/timing/execute workflow before execute claims";
  }
  return "no repair required";
}

std::string stages_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < stages.size(); ++index) {
    out << stage_json(stages[index], *snapshots[index], indent + 2)
        << (index + 1U == stages.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string input_artifacts_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < stages.size(); ++index) {
    out << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(stages[index].artifact()) << ",\n";
    out << nested << "  \"stage\": "
        << inferlab_plan::json_string(stages[index].stage) << ",\n";
    out << nested << "  \"required\": "
        << inferlab_plan::json_bool(stages[index].required) << ",\n";
    out << nested << "  \"dependency_phase\": "
        << inferlab_plan::json_string(
               pipeline_stage_phase_name(stages[index].phase))
        << ",\n";
    out << nested << "  \"blocks_current_decision\": "
        << inferlab_plan::json_bool(
               stages[index].current_decision_input() &&
               stages[index].required)
        << ",\n";
    out << nested << "  \"present\": "
        << inferlab_plan::json_bool(snapshots[index]->metadata.present)
        << ",\n";
    out << nested << "  \"catalog_managed\": "
        << inferlab_plan::json_bool(stages[index].catalog_managed()) << ",\n";
    out << nested << "  \"fallback_retained\": "
        << inferlab_plan::json_bool(stages[index].fallback_retained())
        << ",\n";
    out << nested << "  \"evidence_confidence\": "
        << inferlab_plan::json_string(snapshots[index]->confidence) << "\n";
    out << nested << "}"
        << (index + 1U == stages.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string blockers_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  bool first = true;
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < stages.size(); ++index) {
    if (!stages[index].current_decision_input() ||
        !stages[index].required ||
        inferlab_evidence::evidence_snapshot_ready(*snapshots[index])) {
      continue;
    }
    if (!first) out << ",";
    first = false;
    const std::string category =
        repair_category(stages[index], *snapshots[index]);
    out << "\n" << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(stages[index].artifact()) << ",\n";
    out << nested << "  \"stage\": "
        << inferlab_plan::json_string(stages[index].stage) << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested
        << "  \"message\": \"required release evidence stage is missing, unparsable, lacks schema/summary/claim scope, or reports blockers\",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(
               inferlab_evidence::repair_environment(category))
        << ",\n";
    out << nested << "  \"recommended_action\": "
        << inferlab_plan::json_string(repair_action(stages[index], category))
        << "\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string release_dimensions_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const auto dimensions = release_dimension_order(stages);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t dimension_index = 0;
       dimension_index < dimensions.size(); ++dimension_index) {
    std::size_t total = 0;
    std::size_t ready = 0;
    std::string confidence = "stale_or_blocked";
    for (std::size_t stage_index = 0; stage_index < stages.size();
         ++stage_index) {
      if (!stages[stage_index].current_decision_input()) continue;
      if (stages[stage_index].evidence_role != dimensions[dimension_index]) {
        continue;
      }
      total = inferlab::internal::checked_add_size(
          total, 1U, "release pipeline dimension stage count");
      if (inferlab_evidence::evidence_snapshot_ready(
              *snapshots[stage_index])) {
        ready = inferlab::internal::checked_add_size(
            ready, 1U, "release pipeline ready dimension stage count");
      }
      confidence = snapshots[stage_index]->confidence;
    }
    out << nested << "{\n";
    out << nested << "  \"dimension\": "
        << inferlab_plan::json_string(dimensions[dimension_index]) << ",\n";
    out << nested << "  \"ready\": "
        << inferlab_plan::json_bool(total > 0U && ready == total) << ",\n";
    out << nested << "  \"stage_count\": " << total << ",\n";
    out << nested << "  \"ready_stage_count\": " << ready << ",\n";
    out << nested << "  \"evidence_confidence\": "
        << inferlab_plan::json_string(confidence) << "\n";
    out << nested << "}"
        << (dimension_index + 1U == dimensions.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string grouped_blockers_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const std::string& category_filter,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  bool first = true;
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < stages.size(); ++index) {
    if (!stages[index].current_decision_input()) continue;
    const std::string category =
        repair_category(stages[index], *snapshots[index]);
    const auto environment =
        inferlab_evidence::repair_environment_kind(category);
    if (category_filter == "environment" &&
        environment !=
            inferlab_evidence::RepairEnvironment::kTargetGpuOrExecuteMachine &&
        environment != inferlab_evidence::RepairEnvironment::
                           kTargetProductOrReplayEvidence) {
      continue;
    }
    if (category_filter == "missing_wiring" &&
        category != "summary_missing" && category != "schema_missing" &&
        category != "claim_scope_missing") {
      continue;
    }
    if (category_filter == "missing_replay" && category != "replay_missing") {
      continue;
    }
    if (category_filter == "missing_execute" &&
        category != "execute_missing") {
      continue;
    }
    if (category == "none") continue;
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "{\n";
    out << nested << "  \"stage\": "
        << inferlab_plan::json_string(stages[index].stage) << ",\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(stages[index].artifact()) << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(
               inferlab_evidence::repair_environment(category))
        << ",\n";
    out << nested << "  \"recommended_action\": "
        << inferlab_plan::json_string(repair_action(stages[index], category))
        << "\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string repair_plan_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  const std::vector<std::string> order = {
      "schema_missing", "summary_missing", "claim_scope_missing",
      "artifact_missing", "upstream_blocker", "replay_missing",
      "execute_missing"};
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  bool first = true;
  std::size_t step = 1U;
  std::ostringstream out;
  out << "[";
  for (const auto& category : order) {
    std::vector<std::string> artifacts;
    std::string action;
    for (std::size_t index = 0; index < stages.size(); ++index) {
      if (!stages[index].current_decision_input()) continue;
      if (repair_category(stages[index], *snapshots[index]) != category) {
        continue;
      }
      artifacts.push_back(stages[index].artifact());
      if (action.empty()) action = repair_action(stages[index], category);
    }
    if (artifacts.empty()) continue;
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "{\n";
    out << nested << "  \"step\": " << step << ",\n";
    step = inferlab::internal::checked_add_size(
        step, 1U, "release pipeline repair-plan step");
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(
               inferlab_evidence::repair_environment(category))
        << ",\n";
    out << nested << "  \"recommended_action\": "
        << inferlab_plan::json_string(action) << ",\n";
    out << nested << "  \"artifacts\": "
        << inferlab_plan::json_string_array(artifacts, indent + 2) << "\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string repair_order_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const int indent) {
  require_aligned_stages(stages, snapshots);
  std::vector<std::string> order;
  for (std::size_t index = 0; index < stages.size(); ++index) {
    if (!stages[index].current_decision_input()) continue;
    const std::string category =
        repair_category(stages[index], *snapshots[index]);
    if (category == "none") continue;
    order.push_back(stages[index].id + ":" + category);
  }
  return inferlab_plan::json_string_array(order, indent);
}

std::string shortest_path_json(const std::string& target, const int indent) {
  std::vector<std::string> path = {
      "refresh current-decision contract, catalog, audit, lineage, readiness, and optional replay evidence",
      "rerun release evidence pipeline in dry-run mode",
      "refresh post-generation contract/freshness/claim/query/report consumers"};
  inferlab_evidence::ReleaseDecision target_decision =
      inferlab_evidence::ReleaseDecision::kBlocked;
  if (inferlab_evidence::try_parse_release_decision(target,
                                                    target_decision)) {
    switch (target_decision) {
      case inferlab_evidence::ReleaseDecision::kReplayReady:
        path.push_back(
            "import product replay files or run product semantic replay extraction");
        path.push_back("refresh product replay alignment and release gate");
        break;
      case inferlab_evidence::ReleaseDecision::kExecuteReady:
        path.push_back("complete replay-ready path");
        path.push_back(
            "move to target GPU/product machine and run execute/correctness/timing workflow");
        break;
      case inferlab_evidence::ReleaseDecision::kReportReady:
        path.push_back(
            "render summary, Prometheus export, and final report only after artifacts are refreshed");
        break;
      case inferlab_evidence::ReleaseDecision::kBlocked:
        break;
    }
  }
  return inferlab_plan::json_string_array(path, indent);
}

}  // namespace inferlab_release_pipeline
