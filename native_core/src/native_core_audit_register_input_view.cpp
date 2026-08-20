#include "native_core_audit_register_input_view.hpp"

#include "native_core_audit_register_view_utils.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_native_audit {

std::string source_roots_json(
    const std::vector<SourceRootStatus>& statuses, const int indent) {
  const int row_indent = checked_view_indent(
      indent, 2, "native audit source-root row JSON indent");
  const int field_indent = checked_view_indent(
      indent, 4, "native audit source-root field JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string row_pad = inferlab_plan::json_indent(row_indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < statuses.size(); ++index) {
    const auto& status = statuses[index];
    out << row_pad << "{\n";
    out << field_pad << "\"root\": "
        << inferlab_plan::json_string(status.root) << ",\n";
    out << field_pad << "\"present\": "
        << inferlab_plan::json_bool(status.present) << ",\n";
    out << field_pad << "\"directory\": "
        << inferlab_plan::json_bool(status.directory) << ",\n";
    out << field_pad << "\"readable\": "
        << inferlab_plan::json_bool(status.readable) << ",\n";
    out << field_pad << "\"status\": "
        << inferlab_plan::json_string(status.status) << ",\n";
    out << field_pad << "\"error\": "
        << inferlab_plan::json_string(status.error) << "\n";
    out << row_pad << "}"
        << (index + 1U == statuses.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string review_manifest_json(const ReviewManifest& manifest,
                                 const int indent) {
  const int field_indent = checked_view_indent(
      indent, 2, "native audit review-manifest field JSON indent");
  const int row_indent = checked_view_indent(
      indent, 4, "native audit review-manifest row JSON indent");
  const int row_field_indent = checked_view_indent(
      indent, 6, "native audit review-manifest row field JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const std::string row_pad = inferlab_plan::json_indent(row_indent);
  const std::string row_field_pad =
      inferlab_plan::json_indent(row_field_indent);
  std::ostringstream out;
  out << "{\n";
  out << field_pad << "\"configured\": "
      << inferlab_plan::json_bool(manifest.configured) << ",\n";
  out << field_pad << "\"present\": "
      << inferlab_plan::json_bool(manifest.present) << ",\n";
  out << field_pad << "\"readable\": "
      << inferlab_plan::json_bool(manifest.readable) << ",\n";
  out << field_pad << "\"parse_ok\": "
      << inferlab_plan::json_bool(manifest.parse_ok) << ",\n";
  out << field_pad << "\"path_redacted\": "
      << inferlab_plan::json_string(manifest.path_label) << ",\n";
  out << field_pad << "\"read_status\": "
      << inferlab_plan::json_string(manifest.read_status) << ",\n";
  out << field_pad << "\"read_error\": "
      << inferlab_plan::json_string(manifest.read_error) << ",\n";
  out << field_pad << "\"override_count\": " << manifest.overrides.size()
      << ",\n";
  out << field_pad << "\"applied_override_count\": "
      << manifest.applied_override_count << ",\n";
  out << field_pad << "\"invalid_override_count\": "
      << manifest.invalid_override_count << ",\n";
  out << field_pad << "\"unmatched_override_count\": "
      << manifest.unmatched_override_count << ",\n";
  out << field_pad << "\"closed_file_count\": "
      << manifest.closed_file_count << ",\n";
  out << field_pad << "\"override_rows\": [";
  const std::size_t limit =
      std::min<std::size_t>(manifest.overrides.size(), 32U);
  for (std::size_t index = 0; index < limit; ++index) {
    const auto& row = manifest.overrides[index];
    if (index > 0U) out << ",";
    out << "\n" << row_pad << "{\n";
    out << row_field_pad << "\"file\": "
        << inferlab_plan::json_string(row.file) << ",\n";
    out << row_field_pad << "\"review_status\": "
        << inferlab_plan::json_string(row.review_status) << ",\n";
    out << row_field_pad << "\"valid\": "
        << inferlab_plan::json_bool(row.valid) << ",\n";
    out << row_field_pad << "\"applied\": "
        << inferlab_plan::json_bool(row.applied) << ",\n";
    out << row_field_pad << "\"invalid_reason\": "
        << inferlab_plan::json_string(row.invalid_reason) << "\n";
    out << row_pad << "}";
  }
  if (limit > 0U) out << "\n" << field_pad;
  out << "],\n";
  out << field_pad << "\"omitted_override_row_count\": "
      << (manifest.overrides.size() > limit
              ? manifest.overrides.size() - limit
              : 0U)
      << ",\n";
  out << field_pad
      << "\"claim_boundary\": \"review manifest is explicit human/static "
         "review input only; it does not run validation, compile code, "
         "execute native binaries, or prove runtime readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string audit_scope_json(const int indent) {
  const int field_indent = checked_view_indent(
      indent, 2, "native audit scope field JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  std::ostringstream out;
  out << "{\n";
  out << field_pad << "\"include_roots\": "
      << inferlab_plan::json_string_array(
             {"native_core/include", "native_core/src"}, field_indent)
      << ",\n";
  out << field_pad << "\"exclude_roots\": "
      << inferlab_plan::json_string_array({"native_core/tests"},
                                          field_indent)
      << ",\n";
  out << field_pad << "\"audit_focus\": "
      << inferlab_plan::json_string_array(
             {"input boundaries", "checked arithmetic",
              "artifact output contract", "CLI normalization",
              "claim boundary", "path redaction",
              "mode/executed/replayed separation"},
             field_indent)
      << ",\n";
  out << field_pad
      << "\"claim_boundary\": \"audit register is a static review ledger; it does not prove build, test, benchmark, replay, or execute success\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_native_audit
