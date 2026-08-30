#pragma once

#include "checked_index.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inferlab_repository_wiring {

struct CMakeTargetSourceMatch {
  std::size_t target_declaration_count = 0U;
  std::size_t source_match_count = 0U;

  bool registered() const {
    return target_declaration_count == 1U && source_match_count == 1U;
  }
};

struct BenchmarkMatrixEntryMatch {
  std::size_t entry_count = 0U;
  std::size_t duplicate_field_count = 0U;
  bool binary_matches = false;
  bool output_matches = false;
  bool command_matches = false;

  bool registered() const {
    return entry_count == 1U && duplicate_field_count == 0U &&
           binary_matches && output_matches && command_matches;
  }
};

struct ProductProfileMatrixEntryMatch {
  std::size_t entry_count = 0U;
  std::size_t duplicate_field_count = 0U;
  bool script_matches = false;
  bool entry_module_matches = false;
  bool backend_matches = false;
  bool config_matches = false;
  bool output_matches = false;
  bool report_matches = false;
  bool command_matches = false;
  bool replay_command_matches = false;

  bool structural_wiring_registered() const {
    return entry_count == 1U && duplicate_field_count == 0U &&
           script_matches && entry_module_matches && backend_matches &&
           config_matches && output_matches && report_matches;
  }

  bool dry_run_command_registered() const {
    return entry_count == 1U && duplicate_field_count == 0U &&
           command_matches;
  }

  bool replay_command_registered() const {
    return entry_count == 1U && duplicate_field_count == 0U &&
           replay_command_matches;
  }

  bool registered() const {
    return structural_wiring_registered() && dry_run_command_registered() &&
           replay_command_registered();
  }
};

namespace detail {

inline bool identifier_char(const char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
         (ch >= '0' && ch <= '9') || ch == '_';
}

inline std::string trim(std::string value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1U);
}

inline std::string unquote(std::string value) {
  value = trim(std::move(value));
  if (value.size() >= 2U &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1U, value.size() - 2U);
  }
  return value;
}

inline std::vector<std::string> cmake_call_bodies(
    const std::string& document, const std::string_view command) {
  std::vector<std::string> bodies;
  std::size_t search = 0U;
  bool quoted = false;
  bool escaped = false;
  bool comment = false;
  while (search < document.size()) {
    const char current = document[search];
    if (comment) {
      if (current == '\n') comment = false;
      ++search;
      continue;
    }
    if (quoted) {
      if (escaped) {
        escaped = false;
      } else if (current == '\\') {
        escaped = true;
      } else if (current == '"') {
        quoted = false;
      }
      ++search;
      continue;
    }
    if (current == '#') {
      comment = true;
      ++search;
      continue;
    }
    if (current == '"') {
      quoted = true;
      ++search;
      continue;
    }
    if (document.compare(search, command.size(), command) != 0) {
      ++search;
      continue;
    }

    const std::size_t found = search;
    const std::size_t after = found + command.size();
    if ((found > 0U && identifier_char(document[found - 1U])) ||
        (after < document.size() && identifier_char(document[after]))) {
      search = after;
      continue;
    }
    std::size_t open = after;
    while (open < document.size() &&
           (document[open] == ' ' || document[open] == '\t' ||
            document[open] == '\r' || document[open] == '\n')) {
      ++open;
    }
    if (open >= document.size() || document[open] != '(') {
      search = after;
      continue;
    }

    std::size_t depth = 1U;
    bool body_quoted = false;
    bool body_escaped = false;
    bool body_comment = false;
    std::size_t close = open + 1U;
    for (; close < document.size(); ++close) {
      const char ch = document[close];
      if (body_comment) {
        if (ch == '\n') body_comment = false;
        continue;
      }
      if (body_quoted) {
        if (body_escaped) {
          body_escaped = false;
        } else if (ch == '\\') {
          body_escaped = true;
        } else if (ch == '"') {
          body_quoted = false;
        }
        continue;
      }
      if (ch == '#') {
        body_comment = true;
      } else if (ch == '"') {
        body_quoted = true;
      } else if (ch == '(') {
        ++depth;
      } else if (ch == ')') {
        --depth;
        if (depth == 0U) break;
      }
    }
    if (depth != 0U) break;
    bodies.push_back(document.substr(open + 1U, close - open - 1U));
    search = close + 1U;
  }
  return bodies;
}

inline std::vector<std::string> cmake_tokens(const std::string& body) {
  std::vector<std::string> tokens;
  std::string token;
  bool quoted = false;
  bool escaped = false;
  bool comment = false;
  const auto flush = [&] {
    if (!token.empty()) {
      tokens.push_back(token);
      token.clear();
    }
  };
  for (const char ch : body) {
    if (comment) {
      if (ch == '\n') comment = false;
      continue;
    }
    if (quoted) {
      if (escaped) {
        token.push_back(ch);
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        quoted = false;
      } else {
        token.push_back(ch);
      }
      continue;
    }
    if (ch == '#') {
      flush();
      comment = true;
    } else if (ch == '"') {
      quoted = true;
    } else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
               ch == '(' || ch == ')') {
      flush();
    } else {
      token.push_back(ch);
    }
  }
  flush();
  return tokens;
}

inline bool contains_token(const std::vector<std::string>& tokens,
                           const std::string& expected) {
  for (const auto& token : tokens) {
    if (token == expected) return true;
  }
  return false;
}

inline bool named_argument_matches(const std::vector<std::string>& tokens,
                                   const std::string& name,
                                   const std::string& expected) {
  for (std::size_t i = 0U; i + 1U < tokens.size(); ++i) {
    if (tokens[i] == name && tokens[i + 1U] == expected) return true;
  }
  return false;
}

inline std::size_t leading_spaces(const std::string& line) {
  std::size_t count = 0U;
  while (count < line.size() && line[count] == ' ') ++count;
  return count;
}

inline std::string yaml_value(const std::string& line,
                              const std::string& key) {
  const std::string stripped = trim(line);
  const std::string prefix = key + ":";
  if (stripped.rfind(prefix, 0U) != 0U) return {};
  std::string value = stripped.substr(prefix.size());
  bool quoted = false;
  for (std::size_t i = 0U; i < value.size(); ++i) {
    if (value[i] == '"' && (i == 0U || value[i - 1U] != '\\')) {
      quoted = !quoted;
    } else if (value[i] == '#' && !quoted) {
      value.resize(i);
      break;
    }
  }
  return unquote(std::move(value));
}

inline bool yaml_list_name(const std::string& line,
                           const std::string& expected) {
  std::string stripped = trim(line);
  if (stripped.rfind("- ", 0U) != 0U) return false;
  stripped.erase(0U, 2U);
  return yaml_value(stripped, "name") == expected;
}

}  // namespace detail

inline CMakeTargetSourceMatch cmake_target_source_match(
    const std::string& document,
    const std::string& target,
    const std::string& source) {
  CMakeTargetSourceMatch match;
  if (target.empty() || source.empty()) return match;

  for (const auto& command : {std::string("add_executable"),
                              std::string("add_library")}) {
    for (const auto& body : detail::cmake_call_bodies(document, command)) {
      const auto tokens = detail::cmake_tokens(body);
      if (tokens.empty() || tokens.front() != target) continue;
      match.target_declaration_count =
          inferlab::internal::checked_add_size(
              match.target_declaration_count, 1U,
              "CMake target declaration match count");
      if (detail::contains_token(tokens, source)) {
        match.source_match_count = inferlab::internal::checked_add_size(
            match.source_match_count, 1U,
            "CMake target source match count");
      }
    }
  }

  return match;
}

inline BenchmarkMatrixEntryMatch benchmark_matrix_entry_match(
    const std::string& document,
    const std::string& entry_name,
    const std::string& target,
    const std::string& artifact,
    const std::string& command) {
  BenchmarkMatrixEntryMatch result;
  if (entry_name.empty() || target.empty() || artifact.empty() ||
      command.empty()) {
    return result;
  }

  std::vector<std::string> lines;
  std::size_t begin = 0U;
  while (begin <= document.size()) {
    const std::size_t end = document.find('\n', begin);
    lines.push_back(document.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1U;
  }

  const std::string expected_binary = "build/" + target;
  const std::string expected_output = "results/raw/" + artifact;
  for (std::size_t i = 0U; i < lines.size(); ++i) {
    if (!detail::yaml_list_name(lines[i], entry_name)) continue;
    result.entry_count = inferlab::internal::checked_add_size(
        result.entry_count, 1U, "benchmark-matrix entry match count");
    const std::size_t row_indent = detail::leading_spaces(lines[i]);
    std::size_t binary_count = 0U;
    std::size_t output_count = 0U;
    std::size_t command_count = 0U;
    bool binary_matches = false;
    bool output_matches = false;
    bool command_matches = false;
    for (std::size_t j = i + 1U; j < lines.size(); ++j) {
      const std::string stripped = detail::trim(lines[j]);
      if (stripped.empty() || stripped.front() == '#') continue;
      if (detail::leading_spaces(lines[j]) <= row_indent) {
        break;
      }
      const std::string binary = detail::yaml_value(lines[j], "binary");
      if (!binary.empty()) {
        binary_count = inferlab::internal::checked_add_size(
            binary_count, 1U, "benchmark-matrix binary field count");
        binary_matches = binary == expected_binary;
      }
      const std::string output = detail::yaml_value(lines[j], "output");
      if (!output.empty()) {
        output_count = inferlab::internal::checked_add_size(
            output_count, 1U, "benchmark-matrix output field count");
        output_matches = output == expected_output;
      }
      const std::string row_command =
          detail::yaml_value(lines[j], "command");
      if (!row_command.empty()) {
        command_count = inferlab::internal::checked_add_size(
            command_count, 1U, "benchmark-matrix command field count");
        command_matches = row_command == command;
      }
    }
    if (binary_count > 1U) {
      result.duplicate_field_count = inferlab::internal::checked_add_size(
          result.duplicate_field_count, binary_count - 1U,
          "benchmark-matrix duplicate binary field count");
    }
    if (output_count > 1U) {
      result.duplicate_field_count = inferlab::internal::checked_add_size(
          result.duplicate_field_count, output_count - 1U,
          "benchmark-matrix duplicate output field count");
    }
    if (command_count > 1U) {
      result.duplicate_field_count = inferlab::internal::checked_add_size(
          result.duplicate_field_count, command_count - 1U,
          "benchmark-matrix duplicate command field count");
    }
    result.binary_matches = result.binary_matches || binary_matches;
    result.output_matches = result.output_matches || output_matches;
    result.command_matches = result.command_matches || command_matches;
  }
  return result;
}

inline ProductProfileMatrixEntryMatch product_profile_matrix_entry_match(
    const std::string& document,
    const std::string& entry_name,
    const std::string& script,
    const std::string& entry_module,
    const std::string& config,
    const std::string& artifact,
    const std::string& report,
    const std::string& command,
    const std::string& replay_command) {
  ProductProfileMatrixEntryMatch result;
  if (entry_name.empty() || script.empty() || entry_module.empty() ||
      config.empty() || artifact.empty() || report.empty() ||
      command.empty() || replay_command.empty()) {
    return result;
  }

  std::vector<std::string> lines;
  std::size_t begin = 0U;
  while (begin <= document.size()) {
    const std::size_t end = document.find('\n', begin);
    lines.push_back(document.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1U;
  }

  const std::string expected_output = "results/raw/" + artifact;
  for (std::size_t index = 0U; index < lines.size(); ++index) {
    if (!detail::yaml_list_name(lines[index], entry_name)) continue;
    result.entry_count = inferlab::internal::checked_add_size(
        result.entry_count, 1U,
        "product-profile benchmark-matrix entry match count");
    const std::size_t row_indent = detail::leading_spaces(lines[index]);
    std::size_t script_count = 0U;
    std::size_t module_count = 0U;
    std::size_t backend_count = 0U;
    std::size_t config_count = 0U;
    std::size_t output_count = 0U;
    std::size_t report_count = 0U;
    std::size_t command_count = 0U;
    std::size_t replay_command_count = 0U;
    for (std::size_t row = index + 1U; row < lines.size(); ++row) {
      const std::string stripped = detail::trim(lines[row]);
      if (stripped.empty() || stripped.front() == '#') continue;
      if (detail::leading_spaces(lines[row]) <= row_indent) break;

      const auto inspect_field = [&](const std::string& key,
                                     const std::string& expected,
                                     std::size_t& count,
                                     bool& matches,
                                     const char* context) {
        const std::string actual = detail::yaml_value(lines[row], key);
        if (actual.empty()) return;
        count = inferlab::internal::checked_add_size(count, 1U, context);
        matches = matches || actual == expected;
      };
      inspect_field("script", script, script_count, result.script_matches,
                    "product-profile matrix script field count");
      inspect_field("entry_module", entry_module, module_count,
                    result.entry_module_matches,
                    "product-profile matrix module field count");
      inspect_field("backend", "product_profile", backend_count,
                    result.backend_matches,
                    "product-profile matrix backend field count");
      inspect_field("config", config, config_count, result.config_matches,
                    "product-profile matrix config field count");
      inspect_field("output", expected_output, output_count,
                    result.output_matches,
                    "product-profile matrix output field count");
      inspect_field("report", report, report_count, result.report_matches,
                    "product-profile matrix report field count");
      inspect_field("command", command, command_count,
                    result.command_matches,
                    "product-profile matrix command field count");
      inspect_field("replay_command", replay_command,
                    replay_command_count, result.replay_command_matches,
                    "product-profile matrix replay-command field count");
    }

    const auto add_duplicate_count = [&](const std::size_t count,
                                         const char* context) {
      if (count <= 1U) return;
      result.duplicate_field_count = inferlab::internal::checked_add_size(
          result.duplicate_field_count, count - 1U, context);
    };
    add_duplicate_count(script_count,
                        "product-profile duplicate script field count");
    add_duplicate_count(module_count,
                        "product-profile duplicate module field count");
    add_duplicate_count(backend_count,
                        "product-profile duplicate backend field count");
    add_duplicate_count(config_count,
                        "product-profile duplicate config field count");
    add_duplicate_count(output_count,
                        "product-profile duplicate output field count");
    add_duplicate_count(report_count,
                        "product-profile duplicate report field count");
    add_duplicate_count(command_count,
                        "product-profile duplicate command field count");
    add_duplicate_count(
        replay_command_count,
        "product-profile duplicate replay-command field count");
  }
  return result;
}

}  // namespace inferlab_repository_wiring
