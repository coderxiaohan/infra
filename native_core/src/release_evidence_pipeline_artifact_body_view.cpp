#include "release_evidence_pipeline_artifact_sections.hpp"

#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_alignment_view.hpp"
#include "release_evidence_pipeline_decision_view.hpp"
#include "release_evidence_pipeline_non_gpu_review_view.hpp"
#include "release_evidence_pipeline_stage_view.hpp"
#include "repository_evidence_dag_view.hpp"

#include <ostream>

namespace inferlab_release_pipeline::detail {

void append_release_pipeline_artifact_body(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection) {
  const auto& request = projection.request;
  out << "  \"source_contract_gate\": "
      << release_pipeline_source_contract_gate_json(request.decision, 2)
      << ",\n";
  out << "  \"replay_evidence_dimension\": "
      << release_pipeline_replay_evidence_dimension_json(request.decision, 2)
      << ",\n";
  out << "  \"release_alignment_dimension\": "
      << release_pipeline_alignment_dimension_json(request.alignment, 2)
      << ",\n";
  out << "  \"repository_dependency_graph\": "
      << inferlab_repository_dag::repository_evidence_dag_json(
             request.repository_dependency_graph, 2)
      << ",\n";
  out << "  \"input_artifacts\": "
      << input_artifacts_json(request.stages, request.stage_snapshots, 2)
      << ",\n";
  out << "  \"pipeline_stages\": "
      << stages_json(request.stages, request.stage_snapshots, 2) << ",\n";
  out << "  \"release_dimensions\": "
      << release_dimensions_json(request.stages, request.stage_snapshots, 2)
      << ",\n";
  out << "  \"blockers\": "
      << blockers_json(request.stages, request.stage_snapshots, 2) << ",\n";
  out << "  \"repair_plan\": "
      << repair_plan_json(request.stages, request.stage_snapshots, 2)
      << ",\n";
  out << "  \"repair_order\": "
      << repair_order_json(request.stages, request.stage_snapshots, 2)
      << ",\n";
  out << "  \"blocked_by_environment\": "
      << grouped_blockers_json(request.stages, request.stage_snapshots,
                               "environment", 2)
      << ",\n";
  out << "  \"blocked_by_missing_wiring\": "
      << grouped_blockers_json(request.stages, request.stage_snapshots,
                               "missing_wiring", 2)
      << ",\n";
  out << "  \"blocked_by_missing_replay\": "
      << grouped_blockers_json(request.stages, request.stage_snapshots,
                               "missing_replay", 2)
      << ",\n";
  out << "  \"blocked_by_missing_execute\": "
      << grouped_blockers_json(request.stages, request.stage_snapshots,
                               "missing_execute", 2)
      << ",\n";
  out << "  \"shortest_path_to_report_ready\": "
      << shortest_path_json("report_ready", 2) << ",\n";
  out << "  \"shortest_path_to_replay_ready\": "
      << shortest_path_json("replay_ready", 2) << ",\n";
  out << "  \"shortest_path_to_execute_ready\": "
      << shortest_path_json("execute_ready", 2) << ",\n";
  out << "  \"non_gpu_review_closure\": "
      << non_gpu_review_closure_json(projection, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(projection.recommended_next_commands,
                                          2)
      << "\n";
  out << "}\n";
}

}  // namespace inferlab_release_pipeline::detail
