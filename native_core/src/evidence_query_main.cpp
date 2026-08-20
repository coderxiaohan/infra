#include "evidence_domain_utils.hpp"
#include "evidence_query_cli_contract.hpp"
#include "evidence_query_cli_parser.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_request.hpp"
#include "evidence_query_result_artifact_view.hpp"
#include "evidence_query_selector_export_artifact_view.hpp"
#include "evidence_query_text_view.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

using inferlab_evidence::is_readiness_target;
using inferlab_evidence_query::EvidenceQueryRequest;
using inferlab_evidence_query::PositionalQueryArgs;
using inferlab_evidence_query::is_known_blocker_category;
using inferlab_evidence_query::parse_positional_query_args;
using inferlab_evidence_query::render_evidence_query_result_artifact;
using inferlab_evidence_query::render_evidence_query_text;
using inferlab_evidence_query::render_selector_repair_card_export_artifact;

}  // namespace

int main(int argc, char** argv) {
  try {
    const char* program_name = inferlab_plan::cli_program_name(
        argc, argv, "inferlab_evidence_query");
    const bool selector_export_wrapper =
        inferlab_evidence_query_cli::
            is_selector_repair_card_compatibility_invocation(
                program_name);
    const auto& cli_options = inferlab_evidence_query_cli::option_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program_name, cli_options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_options);
    const auto option_value = [&](const std::string& name,
                                  const std::string& fallback) {
      return inferlab_plan::arg_value_with_spec(argc, argv, name, fallback,
                                                cli_options);
    };
    const auto positionals =
        inferlab_plan::positional_args(argc, argv, cli_options);
    const PositionalQueryArgs positional_command =
        parse_positional_query_args(positionals);
    const fs::path repo_root = option_value("--repo-root", ".");
    const fs::path warehouse_arg =
        option_value("--warehouse",
                     inferlab_evidence_query_cli::kDefaultWarehouseArtifact);
    const fs::path warehouse =
        inferlab_plan::resolve_repo_relative_path(repo_root, warehouse_arg);
    const fs::path results = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        option_value("--results",
                     inferlab_evidence_query_cli::kDefaultResultsDirectory));
    const fs::path fixture_arg = option_value("--fixture-dir", "");
    const fs::path fixture_dir =
        fixture_arg.empty()
            ? fixture_arg
            : inferlab_plan::resolve_repo_relative_path(repo_root,
                                                        fixture_arg);
    std::string query = option_value("--query", "confidence_summary");
    std::string operation = option_value("--operation", "");
    std::string selector = option_value("--selector", "");
    std::string target = option_value("--target", "");
    std::string artifact = option_value("--artifact", "");
    std::string blocker = option_value("--blocker", "");
    if (operation.empty() && !positional_command.operation.empty()) {
      operation = positional_command.operation;
    }
    if (selector.empty() && !positional_command.selector.empty()) {
      selector = positional_command.selector;
    }
    if (target.empty() && !positional_command.target.empty()) {
      target = positional_command.target;
    }
    inferlab_evidence_query_cli::
        apply_selector_repair_card_compatibility_contract(
            selector_export_wrapper, operation, selector);
    if (operation == "path" && target.empty()) {
      target = "report_ready";
    }
    if (operation == "path" && !is_readiness_target(target)) {
      throw std::invalid_argument("unsupported readiness target: " + target +
                                  "; expected report_ready, replay_ready, or "
                                  "execute_ready");
    }
    if (!operation.empty()) {
      query = operation == "path" && !target.empty() ? "path_" + target
                                                     : operation;
    }
    const fs::path before_arg = option_value("--before", "");
    const fs::path after_arg = option_value("--after", "");
    const fs::path before_snapshot =
        before_arg.empty()
            ? before_arg
            : inferlab_plan::resolve_repo_relative_path(repo_root, before_arg);
    const fs::path after_snapshot =
        after_arg.empty()
            ? after_arg
            : inferlab_plan::resolve_repo_relative_path(repo_root, after_arg);
    if (artifact.empty() && !selector.empty() &&
        !is_known_blocker_category(selector)) {
      artifact = selector;
    }
    if (blocker.empty() && !selector.empty() &&
        is_known_blocker_category(selector)) {
      blocker = selector;
    }
    const std::string format = option_value("--format", "json");
    if (format != "json" && format != "text") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json or text");
    }
    const fs::path output_arg = option_value(
        "--output",
        inferlab_evidence_query_cli::primary_output_default(
            selector_export_wrapper));
    const fs::path output =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output_arg);
    const fs::path selector_repair_card_output_arg = option_value(
        "--selector-repair-card-output",
        inferlab_evidence_query_cli::selector_repair_card_output_default(
            selector_export_wrapper));
    const fs::path selector_repair_card_output =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, selector_repair_card_output_arg);
    const std::string selector_repair_card_export_write_status =
        selector_repair_card_output.empty() ? "not_configured"
                                            : "written_before_primary_output";
    EvidenceQueryRequest request(
        warehouse, repo_root, results, fixture_dir, query, artifact, blocker,
        before_snapshot, after_snapshot, selector_repair_card_output,
        selector_repair_card_export_write_status);
    if (!selector_repair_card_output.empty()) {
      inferlab_plan::write_or_print(
          selector_repair_card_output.string(),
          render_selector_repair_card_export_artifact(request));
      if (output.empty()) return 0;
    }
    if (format == "text") {
      inferlab_plan::write_or_print(output.string(),
                                    render_evidence_query_text(request));
    } else {
      inferlab_plan::write_or_print(
          output.string(), render_evidence_query_result_artifact(request));
    }
  } catch (const std::exception& exc) {
    inferlab_plan::report_cli_error(exc, "inferlab_evidence_query failed: ");
    return 1;
  }
  return 0;
}
