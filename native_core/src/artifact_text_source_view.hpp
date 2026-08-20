#pragma once

#include "artifact_text_source.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_artifact {

inline std::string text_source_metadata_json(
    const std::vector<TextSource>& sources, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact text-source view indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[";
  if (!sources.empty()) out << "\n";
  for (std::size_t index = 0U; index < sources.size(); ++index) {
    const auto& source = sources[index];
    const std::string path_label =
        inferlab_plan::redacted_path_label(source.path);
    out << nested << "{\n";
    out << nested << "  \"id\": "
        << inferlab_plan::json_string(source.id) << ",\n";
    out << nested << "  \"path\": "
        << inferlab_plan::json_string(path_label) << ",\n";
    out << nested << "  \"path_redacted\": "
        << inferlab_plan::json_bool(
               inferlab_plan::path_was_redacted(source.path, path_label))
        << ",\n";
    out << nested << "  \"present\": "
        << inferlab_plan::json_bool(source.present) << ",\n";
    out << nested << "  \"readable\": "
        << inferlab_plan::json_bool(source.readable) << ",\n";
    out << nested << "  \"read_status\": "
        << inferlab_plan::json_string(source.status) << ",\n";
    out << nested << "  \"read_issue_count\": "
        << source.read_issue_count << "\n";
    out << nested << "}"
        << (index + 1U == sources.size() ? "\n" : ",\n");
  }
  if (!sources.empty()) out << pad;
  out << "]";
  return out.str();
}

}  // namespace inferlab_artifact
