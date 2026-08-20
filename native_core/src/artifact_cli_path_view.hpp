#pragma once

#include "artifact_metadata_reader.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_artifact {

inline std::string cli_path_field_json(const std::string& name,
                                       const std::filesystem::path& path,
                                       const int indent) {
  if (name.empty()) {
    throw std::invalid_argument("CLI path field name must be nonempty");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string value = path.string();
  const std::string label =
      path.empty() ? "" : inferlab_plan::redacted_path_label(value);
  std::ostringstream out;
  out << pad << inferlab_plan::json_string(name) << ": {\n";
  out << pad << "  \"path\": "
      << (path.empty() ? "null" : inferlab_plan::json_string(label))
      << ",\n";
  out << pad << "  \"path_redacted\": "
      << inferlab_plan::json_bool(
             !path.empty() && inferlab_plan::path_was_redacted(value, label))
      << ",\n";
  out << pad << "  \"path_status\": "
      << inferlab_plan::json_string(path_probe_status(path)) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_artifact
