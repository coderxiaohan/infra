#include "native_core_audit_register_artifact_view.hpp"

#include "high_value_artifact_catalog.hpp"
#include "native_core_audit_register_file_view.hpp"
#include "native_core_audit_register_input_view.hpp"
#include "native_core_audit_register_summary_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace inferlab_native_audit {

std::string native_core_audit_register_artifact_json(
    const NativeCoreAuditRegisterRequest& request,
    const std::string& output_arg) {
  const auto& artifact =
      inferlab_high_value::require_managed_artifact(
          "native_core_audit_register");
  const auto& catalog_artifact =
      inferlab_high_value::require_managed_artifact(
          "artifact_catalog_single_source");
  const auto& evidence_query =
      inferlab_high_value::require_managed_artifact("evidence_query_result");
  const auto& release_pipeline =
      inferlab_high_value::require_managed_artifact(
          "release_evidence_pipeline");
  const std::string default_output = "results/raw/" + artifact.artifact;
  const std::string output_path =
      output_arg.empty() ? default_output : output_arg;
  const std::string manifest_free_command =
      "./build/" + artifact.target +
      " --repo-root . --output " + default_output + " --format json";

  const auto& roots = request.source_roots();
  const auto& audits = request.audits();
  const auto& manifest = request.review_manifest();
  const auto& summary = request.summary();

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 2,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact.id) << ",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, artifact.id,
      "static native_core audit register only; does not claim full human review, build success, benchmark results, or runtime correctness",
      {"native_core", "C++17", "CUDA migration readiness"},
      artifact.target, output_path, artifact.command);
  out << "  \"status\": "
      << inferlab_plan::json_string(summary.artifact_status) << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"static source audit ledger only; validation and full human review remain separate gates\",\n";
  out << "  \"summary\": "
      << native_core_audit_summary_json(request, 2) << ",\n";
  out << "  \"audit_scope\": " << audit_scope_json(2) << ",\n";
  out << "  \"source_roots\": " << source_roots_json(roots, 2) << ",\n";
  out << "  \"review_manifest\": "
      << review_manifest_json(manifest, 2) << ",\n";
  out << "  \"files\": " << files_json(audits, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             {artifact.command, manifest_free_command,
              catalog_artifact.command, evidence_query.command,
              release_pipeline.command},
             2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_native_audit
