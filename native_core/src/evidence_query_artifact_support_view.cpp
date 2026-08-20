#include "evidence_query_artifact_support_view.hpp"

#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

std::string product_replay_semantic_linkage_json(
    const ProductReplaySemanticLinkageSignals& signals,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(signals.status) << ",\n";
  out << nested << "\"semantic_events_present\": "
      << (signals.semantic_events_present ? "true" : "false") << ",\n";
  out << nested << "\"semantic_events_readable\": "
      << (signals.semantic_events_readable ? "true" : "false") << ",\n";
  out << nested << "\"semantic_events_read_status\": "
      << inferlab_plan::json_string(signals.semantic_events_read_status)
      << ",\n";
  out << nested << "\"alignment_present\": "
      << (signals.alignment_present ? "true" : "false") << ",\n";
  out << nested << "\"alignment_readable\": "
      << (signals.alignment_readable ? "true" : "false") << ",\n";
  out << nested << "\"alignment_read_status\": "
      << inferlab_plan::json_string(signals.alignment_read_status) << ",\n";
  out << nested << "\"read_issue_count\": " << signals.read_issue_count
      << ",\n";
  out << nested << "\"available_event_count\": "
      << signals.available_event_count << ",\n";
  out << nested << "\"pending_event_count\": "
      << signals.pending_event_count << ",\n";
  out << nested << "\"sample_event_count\": " << signals.sample_event_count
      << ",\n";
  out << nested << "\"sample_replay_present\": "
      << (signals.sample_replay_present ? "true" : "false") << ",\n";
  out << nested << "\"replay_supported_event_count\": "
      << signals.replay_supported_event_count << ",\n";
  out << nested << "\"alignment_rule_count\": "
      << signals.alignment_rule_count << ",\n";
  out << nested << "\"aligned_rule_count\": " << signals.aligned_rule_count
      << ",\n";
  out << nested << "\"pending_rule_count\": " << signals.pending_rule_count
      << ",\n";
  out << nested << "\"correlation_count\": " << signals.correlation_count
      << ",\n";
  out << nested << "\"ready_correlation_count\": "
      << signals.ready_correlation_count << ",\n";
  out << nested << "\"blocked_correlation_count\": "
      << signals.blocked_correlation_count << ",\n";
  out << nested << "\"blocker_count\": " << signals.blocker_count << ",\n";
  out << nested << "\"target_or_validation_action_count\": "
      << signals.target_or_validation_action_count << ",\n";
  out << nested << "\"first_blocker\": "
      << inferlab_plan::json_string(signals.first_blocker) << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(signals.next_action) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "product replay semantic linkage maps canonical semantic events to catalog-managed native explanation artifacts; it does not execute products, create benchmark timing, prove causal joins, or convert replay into execute evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string source_files_json(
    const std::vector<inferlab_artifact::TextSource>& sources,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "[";
  bool first = true;
  for (const auto& source : sources) {
    if (!first) out << ",";
    first = false;
    const std::string path_label =
        inferlab_plan::redacted_path_label(source.path);
    out << "\n" << nested << "{\n";
    out << nested << "  \"id\": "
        << inferlab_plan::json_string(source.id) << ",\n";
    out << nested << "  \"path\": "
        << inferlab_plan::json_string(path_label) << ",\n";
    out << nested << "  \"path_redacted\": "
        << (inferlab_plan::path_was_redacted(source.path, path_label)
                ? "true"
                : "false")
        << ",\n";
    out << nested << "  \"present\": "
        << (source.present ? "true" : "false") << ",\n";
    out << nested << "  \"readable\": "
        << (source.readable ? "true" : "false") << ",\n";
    out << nested << "  \"read_status\": "
        << inferlab_plan::json_string(source.status) << ",\n";
    out << nested << "  \"read_issue_count\": "
        << source.read_issue_count << "\n";
    out << nested << "}";
  }
  if (!sources.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string query_plan_json(const EvidenceQueryRequest& request,
                            const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& operation = request.query_operation;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(request.query) << ",\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"operation_label\": "
      << inferlab_plan::json_string(operation.human_label) << ",\n";
  out << nested << "\"operation_selector\": "
      << inferlab_plan::json_string(operation.selector) << ",\n";
  out << nested << "\"operation_target\": "
      << inferlab_plan::json_string(operation.target) << ",\n";
  out << nested << "\"artifact_filter\": "
      << inferlab_plan::json_string(request.artifact_filter) << ",\n";
  out << nested << "\"blocker_filter\": "
      << inferlab_plan::json_string(request.blocker_filter) << ",\n";
  out << inferlab_plan::redacted_path_json_fields(
      "warehouse", request.warehouse_path.string(), indent + 2);
  out << inferlab_plan::redacted_path_json_fields(
      "repo_root", request.repo_root.string(), indent + 2);
  out << inferlab_plan::redacted_path_json_fields(
      "results", request.results_path.string(), indent + 2);
  out << inferlab_plan::redacted_path_json_fields(
      "fixture_dir", request.fixture_dir.string(), indent + 2);
  out << nested << "\"warehouse_used\": "
      << (request.source.warehouse_used ? "true" : "false") << ",\n";
  out << nested << "\"fixture_used\": "
      << (request.source.fixture_used ? "true" : "false") << ",\n";
  out << nested
      << "\"query_scope\": \"read-only evidence inspection; no performance claims are created\"\n";
  out << pad << "}";
  return out.str();
}

std::string human_summary_json(
    const std::size_t matched_count,
    const std::size_t blocked_count,
    const std::string& query,
    const std::string& blocker_filter,
    const int indent) {
  std::vector<std::string> lines = {
      "query=" + query + " matched " + std::to_string(matched_count) +
          " artifact records",
      "blocked_or_partial records=" + std::to_string(blocked_count),
      "Evidence Query is read-only and does not create benchmark or product execute claims"};
  if (!blocker_filter.empty()) {
    lines.push_back("blocker filter applied: " + blocker_filter);
  }
  return inferlab_plan::json_string_array(lines, indent);
}

}  // namespace inferlab_evidence_query
