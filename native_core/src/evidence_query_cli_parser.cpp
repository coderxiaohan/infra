#include "evidence_query_cli_parser.hpp"

#include "evidence_release_policy.hpp"
#include "evidence_query_operation_catalog.hpp"
#include "evidence_query_selection.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string trim_copy(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(begin, end - begin);
}

std::string normalize_query_token(std::string value) {
  value = trim_copy(value);
  std::replace(value.begin(), value.end(), ' ', '_');
  return value;
}

void require_positional_count_at_most(const std::vector<std::string>& args,
                                      const std::size_t max_count,
                                      const std::string& form) {
  if (args.size() > max_count) {
    throw std::invalid_argument("too many positional arguments for " + form +
                                "; " + evidence_query_usage_summary());
  }
}

void require_positional_count_exact(const std::vector<std::string>& args,
                                    const std::size_t expected_count,
                                    const std::string& form) {
  if (args.size() != expected_count) {
    throw std::invalid_argument("invalid positional arguments for " + form +
                                "; " + evidence_query_usage_summary());
  }
}

void require_optional_second_token(const std::vector<std::string>& args,
                                   const std::string& expected_token,
                                   const std::string& form) {
  if (args.size() >= 2U && args[1] != expected_token) {
    throw std::invalid_argument("unsupported positional command form for " +
                                form + "; " +
                                evidence_query_usage_summary());
  }
}

}  // namespace

PositionalQueryArgs parse_positional_query_args(
    const std::vector<std::string>& args) {
  PositionalQueryArgs parsed;
  if (args.empty()) return parsed;

  const std::string first = args[0];
  const std::string second = args.size() >= 2U ? args[1] : "";
  const std::string third = args.size() >= 3U ? args[2] : "";

  if (first == "list") {
    require_positional_count_at_most(args, 2U, "list artifacts");
    require_optional_second_token(args, "artifacts", "list artifacts");
    parsed.operation = "list_artifacts";
  } else if (first == "show") {
    require_positional_count_at_most(args, 3U, "show artifact <artifact>");
    require_optional_second_token(args, "artifact",
                                  "show artifact <artifact>");
    parsed.operation = "show_artifact";
    parsed.selector = second == "artifact" ? third : second;
  } else if (first == "explain") {
    require_positional_count_at_most(
        args, 3U, "explain blocker <category|artifact>");
    require_optional_second_token(
        args, "blocker", "explain blocker <category|artifact>");
    parsed.operation = "explain_blocker";
    parsed.selector = second == "blocker" ? third : second;
  } else if (first == "suggest") {
    require_positional_count_at_most(
        args, 3U, "suggest repair <artifact|category|decision>");
    require_optional_second_token(
        args, "repair", "suggest repair <artifact|category|decision>");
    parsed.operation = "suggest_repair";
    parsed.selector = second == "repair" ? third : second;
  } else if (first == "repair" && second == "groups") {
    require_positional_count_at_most(
        args, 3U, "repair groups <artifact|category>");
    parsed.operation = "repair_group_summary";
    parsed.selector = third;
  } else if (first == "repair_groups" ||
             first == "repair_group_summary") {
    require_positional_count_at_most(
        args, 2U, "repair_group_summary <artifact|category>");
    parsed.operation = "repair_group_summary";
    parsed.selector = second;
  } else if (first == "diff") {
    require_positional_count_at_most(args, 2U, "diff snapshots");
    require_optional_second_token(args, "snapshots", "diff snapshots");
    parsed.operation = "diff_snapshots";
  } else if (first == "path") {
    require_positional_count_at_most(args, 2U,
                                     "path <readiness target>");
    parsed.operation = "path";
    parsed.target = second;
  } else if (first == "artifact" && !second.empty()) {
    require_positional_count_exact(args, 2U, "artifact <artifact>");
    parsed.selector = second;
  } else if (first == "blocker" && !second.empty()) {
    require_positional_count_exact(args, 2U, "blocker <category>");
    parsed.selector = second;
  } else if (inferlab_evidence::is_readiness_target(first)) {
    require_positional_count_exact(args, 1U, "<readiness target>");
    parsed.operation = "path";
    parsed.target = first;
  } else {
    require_positional_count_exact(args, 1U, "<artifact selector>");
    parsed.selector = first;
  }
  return parsed;
}

QueryOperation parse_query_operation(const std::string& query,
                                     const std::string& artifact_filter,
                                     const std::string& blocker_filter) {
  QueryOperation op;
  op.raw_query = query;
  op.normalized_query = normalize_query_token(query);
  op.operation = "warehouse_query";
  op.selector = artifact_filter.empty() ? blocker_filter : artifact_filter;
  op.human_label = "warehouse evidence query";

  const std::string& normalized = op.normalized_query;
  if (normalized == "list_artifacts" || normalized == "list") {
    op.operation = "list_artifacts";
    op.human_label = "list artifacts";
  } else if (normalized == "show_artifact" || normalized == "show") {
    op.operation = "show_artifact";
    op.human_label = "show artifact";
  } else if (starts_with(normalized, "show_artifact_")) {
    op.operation = "show_artifact";
    op.selector = normalized.substr(std::string("show_artifact_").size());
    op.human_label = "show artifact";
  } else if (normalized == "explain_blocker" || normalized == "explain") {
    op.operation = "explain_blocker";
    op.human_label = "explain blocker";
  } else if (starts_with(normalized, "explain_blocker_")) {
    op.operation = "explain_blocker";
    op.selector =
        normalized.substr(std::string("explain_blocker_").size());
    op.human_label = "explain blocker";
  } else if (normalized == "suggest_repair" || normalized == "repair") {
    op.operation = "suggest_repair";
    op.human_label = "suggest repair";
  } else if (starts_with(normalized, "suggest_repair_")) {
    op.operation = "suggest_repair";
    op.selector = normalized.substr(std::string("suggest_repair_").size());
    op.human_label = "suggest repair";
  } else if (normalized == "repair_group_summary" ||
             normalized == "repair_groups" ||
             normalized == "repair_group") {
    op.operation = "repair_group_summary";
    op.human_label = "repair group summary";
  } else if (starts_with(normalized, "repair_group_summary_")) {
    op.operation = "repair_group_summary";
    op.selector =
        normalized.substr(std::string("repair_group_summary_").size());
    op.human_label = "repair group summary";
  } else if (starts_with(normalized, "repair_groups_")) {
    op.operation = "repair_group_summary";
    op.selector = normalized.substr(std::string("repair_groups_").size());
    op.human_label = "repair group summary";
  } else if (normalized == "diff_snapshots" || normalized == "diff") {
    op.operation = "diff_snapshots";
    op.human_label = "diff evidence snapshots";
  } else if (normalized == "path_report_ready" ||
             normalized == "report_ready") {
    op.operation = "path";
    op.target = "report_ready";
    op.human_label = "path report_ready";
  } else if (normalized == "path_replay_ready" ||
             normalized == "replay_ready") {
    op.operation = "path";
    op.target = "replay_ready";
    op.human_label = "path replay_ready";
  } else if (normalized == "path_execute_ready" ||
             normalized == "execute_ready") {
    op.operation = "path";
    op.target = "execute_ready";
    op.human_label = "path execute_ready";
  }
  if (op.selector.empty()) op.selector = blocker_filter;
  return op;
}

}  // namespace inferlab_evidence_query
