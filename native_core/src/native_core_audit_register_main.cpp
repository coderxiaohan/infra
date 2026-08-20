#include "native_core_audit_register_artifact_view.hpp"
#include "native_core_audit_register_request.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--output", "--review-manifest", "--format"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  try {
    const char* program_name = inferlab_plan::cli_program_name(
        argc, argv, "inferlab_native_core_audit_register");
    if (inferlab_plan::handle_cli_help(argc, argv, program_name,
                                      kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument(
          "unsupported --format value: " + format + "; expected json");
    }

    const std::filesystem::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::filesystem::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const std::filesystem::path review_manifest_arg =
        inferlab_plan::arg_value(argc, argv, "--review-manifest", "");
    const std::filesystem::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    const std::filesystem::path review_manifest_path =
        review_manifest_arg.empty()
            ? review_manifest_arg
            : inferlab_plan::resolve_repo_relative_path(repo_root,
                                                        review_manifest_arg);

    const inferlab_native_audit::NativeCoreAuditRegisterRequest request(
        repo_root, review_manifest_path);
    inferlab_plan::write_or_print(
        output_path.empty() ? "" : output_path.generic_string(),
        inferlab_native_audit::native_core_audit_register_artifact_json(
            request, output_arg.generic_string()));
  } catch (const std::exception& exc) {
    inferlab_plan::report_cli_error(
        exc, "inferlab_native_core_audit_register failed: ");
    return 1;
  }
  return 0;
}
