#pragma once

#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace inferlab_artifact {

namespace fs = std::filesystem;

struct Metadata {
  std::string source;
  bool present = false;
  bool parse_ok = false;
  bool used_structured_parser = false;
  bool schema_present = false;
  bool summary_present = false;
  bool claim_scope_present = false;
  bool executed_present = false;
  bool replayed_present = false;
  bool target_gpu_execute_verified_present = false;
  bool executed = false;
  bool replayed = false;
  bool target_gpu_execute_verified = false;
  std::size_t blocker_count = 0;
  std::string artifact_type = "unknown";
  std::string mode = "missing";
  std::string claim_scope;
  std::string release_decision;
  std::vector<std::string> warnings;
};

struct WarehouseEvidenceProjection {
  bool resolved = false;
  std::string blocker_category;
  std::string repair_environment;
  std::string confidence;
  std::vector<std::string> affected_claims;
};

struct WarehouseArtifactRecord {
  std::string artifact;
  bool present = false;
  bool matrix_present = false;
  bool summary_present = false;
  bool prometheus_present = false;
  bool report_present = false;
  bool final_report_present = false;
  bool contract_present = false;
  bool freshness_present = false;
  Metadata metadata;
  WarehouseEvidenceProjection evidence_projection;
};

struct WarehouseIndex {
  std::string path;
  bool present = false;
  bool parsed = false;
  bool used_structured_parser = false;
  std::vector<WarehouseArtifactRecord> records;
  std::vector<std::string> warnings;
};

struct TextReadResult {
  std::string text;
  bool present = false;
  bool readable = false;
  bool empty = false;
  std::string status = "missing";
  std::string error;
};

inline bool path_exists(const fs::path& path) {
  std::error_code error;
  const bool exists = fs::exists(path, error);
  return !error && exists;
}

inline bool path_is_regular_file(const fs::path& path) {
  std::error_code error;
  const bool regular = fs::is_regular_file(path, error);
  return !error && regular;
}

inline bool path_is_directory(const fs::path& path) {
  std::error_code error;
  const bool directory = fs::is_directory(path, error);
  return !error && directory;
}

inline std::string path_probe_status(const fs::path& path) {
  if (path.empty()) return "not_configured";
  std::error_code error;
  const bool exists = fs::exists(path, error);
  if (error) return "path_probe_error";
  if (!exists) return "missing";
  const bool directory = fs::is_directory(path, error);
  if (error) return "path_probe_error";
  if (directory) return "directory";
  const bool regular = fs::is_regular_file(path, error);
  if (error) return "path_probe_error";
  return regular ? "file" : "other";
}

inline std::uintmax_t file_size_or_zero(const fs::path& path) {
  if (!path_is_regular_file(path)) return 0;
  std::error_code error;
  const std::uintmax_t size = fs::file_size(path, error);
  return error ? 0 : size;
}

inline TextReadResult read_text_with_status(const fs::path& path) {
  TextReadResult result;
  std::error_code exists_error;
  result.present = fs::exists(path, exists_error);
  if (exists_error) {
    result.status = "unreadable_file_probe";
    result.error = inferlab_plan::redact_error_message(
        exists_error.message(), {path.generic_string()});
    return result;
  }
  if (!result.present) {
    result.status = "missing";
    return result;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    result.status = "unreadable";
    result.error = "failed to open file";
    return result;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    result.status = "unreadable";
    result.error = "failed while reading file";
    return result;
  }

  result.text = buffer.str();
  result.readable = true;
  result.empty = result.text.empty();
  result.status = result.empty ? "empty" : "ok";
  return result;
}

inline TextReadResult read_text_with_status(const fs::path& path,
                                            const std::size_t maximum_bytes) {
  if (maximum_bytes == 0U ||
      maximum_bytes >=
          static_cast<std::size_t>(
              std::numeric_limits<std::streamsize>::max())) {
    throw std::invalid_argument(
        "bounded text read requires a representable positive byte limit");
  }

  TextReadResult result;
  std::error_code exists_error;
  result.present = fs::exists(path, exists_error);
  if (exists_error) {
    result.status = "unreadable_file_probe";
    result.error = inferlab_plan::redact_error_message(
        exists_error.message(), {path.generic_string()});
    return result;
  }
  if (!result.present) {
    result.status = "missing";
    return result;
  }

  std::error_code size_error;
  const std::uintmax_t file_size = fs::file_size(path, size_error);
  if (!size_error && file_size > maximum_bytes) {
    result.status = "too_large";
    result.error = "file exceeds bounded text read limit";
    return result;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    result.status = "unreadable";
    result.error = "failed to open file";
    return result;
  }
  std::string buffer(maximum_bytes + 1U, '\0');
  file.read(buffer.data(),
            static_cast<std::streamsize>(buffer.size()));
  const std::streamsize read_count = file.gcount();
  if (file.bad() || read_count < 0) {
    result.status = "unreadable";
    result.error = "failed while reading file";
    return result;
  }
  if (static_cast<std::size_t>(read_count) > maximum_bytes) {
    result.status = "too_large";
    result.error = "file grew beyond bounded text read limit";
    return result;
  }
  buffer.resize(static_cast<std::size_t>(read_count));
  result.text = std::move(buffer);
  result.readable = true;
  result.empty = result.text.empty();
  result.status = result.empty ? "empty" : "ok";
  return result;
}

inline bool is_json_space(const char ch) {
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

inline std::size_t skip_json_space(const std::string& text,
                                   std::size_t pos) {
  while (pos < text.size() && is_json_space(text[pos])) ++pos;
  return pos;
}

inline bool is_json_value_boundary(const std::string& text,
                                   const std::size_t pos) {
  if (pos >= text.size()) return true;
  const char ch = text[pos];
  return is_json_space(ch) || ch == ',' || ch == '}' || ch == ']';
}

inline bool parse_json_string_literal(const std::string& text,
                                      std::size_t open,
                                      std::size_t& close,
                                      std::string& value);

inline std::size_t find_json_key_fallback(const std::string& text,
                                          const std::string& key,
                                          const std::size_t start_pos = 0) {
  std::size_t key_pos = start_pos;
  while (key_pos < text.size()) {
    if (text[key_pos] != '"') {
      ++key_pos;
      continue;
    }
    std::size_t close = std::string::npos;
    std::string candidate;
    if (!parse_json_string_literal(text, key_pos, close, candidate)) {
      return std::string::npos;
    }
    const std::size_t after_key = skip_json_space(text, close + 1);
    if (candidate == key && after_key < text.size() &&
        text[after_key] == ':') {
      return key_pos;
    }
    key_pos = close + 1;
  }
  return std::string::npos;
}

inline int json_hex_value(const char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
  return -1;
}

inline bool append_json_unicode_escape(const std::string& raw,
                                       const std::size_t pos,
                                       std::string& out,
                                       std::size_t& consumed) {
  if (pos + 4 > raw.size()) return false;
  int value = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    const int nibble = json_hex_value(raw[pos + i]);
    if (nibble < 0) return false;
    value = (value << 4) | nibble;
  }
  consumed = 4;
  if (value >= 0xD800 && value <= 0xDFFF) {
    out += "\\u";
    out += raw.substr(pos, 4);
    return true;
  }
  if (value <= 0x7F) {
    out.push_back(static_cast<char>(value));
    return true;
  }
  if (value <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((value >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return true;
  }
  out.push_back(static_cast<char>(0xE0 | ((value >> 12) & 0x0F)));
  out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
  out.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  return true;
}

inline std::string decode_json_string_fragment(const std::string& raw) {
  std::string decoded;
  decoded.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const char ch = raw[i];
    if (ch != '\\' || i + 1 >= raw.size()) {
      decoded.push_back(ch);
      continue;
    }
    const char escaped = raw[++i];
    switch (escaped) {
      case '"':
      case '\\':
      case '/':
        decoded.push_back(escaped);
        break;
      case 'b':
        decoded.push_back('\b');
        break;
      case 'f':
        decoded.push_back('\f');
        break;
      case 'n':
        decoded.push_back('\n');
        break;
      case 'r':
        decoded.push_back('\r');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      case 'u': {
        std::size_t consumed = 0;
        if (append_json_unicode_escape(raw, i + 1, decoded, consumed)) {
          i += consumed;
        } else {
          decoded.push_back('\\');
          decoded.push_back(escaped);
        }
        break;
      }
      default:
        decoded.push_back('\\');
        decoded.push_back(escaped);
        break;
    }
  }
  return decoded;
}

inline bool parse_json_string_literal(const std::string& text,
                                      const std::size_t open,
                                      std::size_t& close,
                                      std::string& value) {
  value.clear();
  if (open >= text.size() || text[open] != '"') return false;
  std::string raw;
  bool escaped = false;
  for (std::size_t i = open + 1; i < text.size(); ++i) {
    const char ch = text[i];
    if (escaped) {
      if (ch == 'u') {
        if (i + 4 >= text.size()) return false;
        for (std::size_t digit = 1; digit <= 4; ++digit) {
          if (json_hex_value(text[i + digit]) < 0) return false;
        }
        raw.push_back('\\');
        raw.push_back(ch);
        raw.append(text, i + 1, 4);
        i += 4;
        escaped = false;
        continue;
      }
      if (ch != '"' && ch != '\\' && ch != '/' && ch != 'b' && ch != 'f' &&
          ch != 'n' && ch != 'r' && ch != 't') {
        return false;
      }
      raw.push_back('\\');
      raw.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      close = i;
      value = decode_json_string_fragment(raw);
      return true;
    }
    if (static_cast<unsigned char>(ch) < 0x20U) return false;
    raw.push_back(ch);
  }
  return false;
}

inline std::size_t json_value_start_after_key_fallback(
    const std::string& text, const std::size_t key_pos) {
  std::size_t close = std::string::npos;
  std::string ignored;
  if (!parse_json_string_literal(text, key_pos, close, ignored)) {
    return std::string::npos;
  }
  const std::size_t colon = skip_json_space(text, close + 1);
  if (colon >= text.size() || text[colon] != ':') return std::string::npos;
  return skip_json_space(text, colon + 1);
}

inline constexpr std::size_t kFallbackJsonMaxDepth = 256;

inline bool parse_json_value_syntax(const std::string& text, std::size_t& pos,
                                    std::size_t depth);

inline bool parse_json_number_syntax(const std::string& text,
                                     std::size_t& pos) {
  const std::size_t start = pos;
  if (pos < text.size() && text[pos] == '-') ++pos;
  if (pos >= text.size()) return false;
  if (text[pos] == '0') {
    ++pos;
    if (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
      return false;
    }
  } else {
    if (text[pos] < '1' || text[pos] > '9') return false;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
  }
  if (pos < text.size() && text[pos] == '.') {
    ++pos;
    const std::size_t fraction_start = pos;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
    if (pos == fraction_start) return false;
  }
  if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
    ++pos;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
    const std::size_t exponent_start = pos;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
    if (pos == exponent_start) return false;
  }
  return pos > start;
}

inline bool parse_json_array_syntax(const std::string& text, std::size_t& pos,
                                    const std::size_t depth) {
  if (depth > kFallbackJsonMaxDepth || pos >= text.size() ||
      text[pos] != '[') {
    return false;
  }
  pos = skip_json_space(text, pos + 1);
  if (pos < text.size() && text[pos] == ']') {
    ++pos;
    return true;
  }
  while (pos < text.size()) {
    if (!parse_json_value_syntax(text, pos, depth + 1)) return false;
    pos = skip_json_space(text, pos);
    if (pos >= text.size()) return false;
    if (text[pos] == ']') {
      ++pos;
      return true;
    }
    if (text[pos] != ',') return false;
    pos = skip_json_space(text, pos + 1);
  }
  return false;
}

inline bool parse_json_object_syntax(const std::string& text, std::size_t& pos,
                                     const std::size_t depth) {
  if (depth > kFallbackJsonMaxDepth || pos >= text.size() ||
      text[pos] != '{') {
    return false;
  }
  pos = skip_json_space(text, pos + 1);
  if (pos < text.size() && text[pos] == '}') {
    ++pos;
    return true;
  }
  std::set<std::string> keys;
  while (pos < text.size()) {
    std::size_t close = std::string::npos;
    std::string key;
    if (!parse_json_string_literal(text, pos, close, key)) return false;
    if (!keys.insert(key).second) return false;
    pos = skip_json_space(text, close + 1);
    if (pos >= text.size() || text[pos] != ':') return false;
    pos = skip_json_space(text, pos + 1);
    if (!parse_json_value_syntax(text, pos, depth + 1)) return false;
    pos = skip_json_space(text, pos);
    if (pos >= text.size()) return false;
    if (text[pos] == '}') {
      ++pos;
      return true;
    }
    if (text[pos] != ',') return false;
    pos = skip_json_space(text, pos + 1);
  }
  return false;
}

inline bool parse_json_value_syntax(const std::string& text, std::size_t& pos,
                                    const std::size_t depth) {
  if (depth > kFallbackJsonMaxDepth) return false;
  pos = skip_json_space(text, pos);
  if (pos >= text.size()) return false;
  if (text[pos] == '"') {
    std::size_t close = std::string::npos;
    std::string ignored;
    if (!parse_json_string_literal(text, pos, close, ignored)) return false;
    pos = close + 1;
    return true;
  }
  if (text[pos] == '{') return parse_json_object_syntax(text, pos, depth);
  if (text[pos] == '[') return parse_json_array_syntax(text, pos, depth);
  if (text.compare(pos, 4, "true") == 0) {
    pos += 4;
    return true;
  }
  if (text.compare(pos, 5, "false") == 0) {
    pos += 5;
    return true;
  }
  if (text.compare(pos, 4, "null") == 0) {
    pos += 4;
    return true;
  }
  return parse_json_number_syntax(text, pos);
}

inline bool json_document_is_valid_object(const std::string& text) {
  std::size_t pos = skip_json_space(text, 0);
  if (pos >= text.size() || text[pos] != '{' ||
      !parse_json_object_syntax(text, pos, 0)) {
    return false;
  }
  return skip_json_space(text, pos) == text.size();
}

inline std::size_t find_json_value_end(const std::string& text,
                                       const std::size_t start) {
  if (start >= text.size()) return std::string::npos;
  if (text[start] == '"') {
    std::size_t close = std::string::npos;
    std::string ignored;
    return parse_json_string_literal(text, start, close, ignored) ? close + 1
                                                                  : std::string::npos;
  }
  if (text[start] == '{' || text[start] == '[') {
    std::vector<char> expected_closers;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = start; i < text.size(); ++i) {
      const char ch = text[i];
      if (in_string) {
        if (escaped) {
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == '"') {
          in_string = false;
        }
        continue;
      }
      if (ch == '"') {
        in_string = true;
        continue;
      }
      if (ch == '{') {
        expected_closers.push_back('}');
        continue;
      }
      if (ch == '[') {
        expected_closers.push_back(']');
        continue;
      }
      if (ch == '}' || ch == ']') {
        if (expected_closers.empty() || expected_closers.back() != ch) {
          return std::string::npos;
        }
        expected_closers.pop_back();
        if (expected_closers.empty()) return i + 1;
      }
    }
    return std::string::npos;
  }
  std::size_t end = start;
  while (end < text.size() && text[end] != ',' && text[end] != '}' &&
         text[end] != ']') {
    ++end;
  }
  while (end > start && is_json_space(text[end - 1])) --end;
  return end;
}

inline bool json_root_is_object(const std::string& text) {
  const std::size_t root = skip_json_space(text, 0);
  return root < text.size() && text[root] == '{';
}

inline bool find_top_level_json_value_fallback(const std::string& text,
                                               const std::string& key,
                                               std::size_t& value_start,
                                               std::size_t& value_end) {
  const std::size_t root = skip_json_space(text, 0);
  if (root >= text.size() || text[root] != '{') return false;

  int depth = 1;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t i = root + 1; i < text.size(); ++i) {
    const char ch = text[i];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }

    if (ch == '"') {
      if (depth != 1) {
        in_string = true;
        continue;
      }
      std::size_t close = std::string::npos;
      std::string member_key;
      if (!parse_json_string_literal(text, i, close, member_key)) return false;
      const std::size_t colon = skip_json_space(text, close + 1);
      if (colon < text.size() && text[colon] == ':' && member_key == key) {
        value_start = skip_json_space(text, colon + 1);
        value_end = find_json_value_end(text, value_start);
        return value_end != std::string::npos && value_end >= value_start;
      }
      i = close;
      continue;
    }
    if (ch == '{' || ch == '[') {
      ++depth;
      continue;
    }
    if (ch == '}' || ch == ']') {
      --depth;
      if (depth == 0) return false;
    }
  }
  return false;
}

inline bool contains_top_level_key_fallback(const std::string& text,
                                            const std::string& key) {
  std::size_t value_start = std::string::npos;
  std::size_t value_end = std::string::npos;
  return find_top_level_json_value_fallback(text, key, value_start, value_end);
}

inline bool contains_key(const std::string& text, const std::string& key) {
  return find_json_key_fallback(text, key) != std::string::npos;
}

inline bool contains_semantic_key_fallback(const std::string& text,
                                           const std::string& key) {
  return json_root_is_object(text) ? contains_top_level_key_fallback(text, key)
                                   : contains_key(text, key);
}

inline bool extract_top_level_string_key_fallback(const std::string& text,
                                                  const std::string& key,
                                                  std::string& value) {
  value.clear();
  std::size_t value_start = std::string::npos;
  std::size_t value_end = std::string::npos;
  if (!find_top_level_json_value_fallback(text, key, value_start, value_end) ||
      value_start >= text.size() || text[value_start] != '"') {
    return false;
  }
  std::size_t close = std::string::npos;
  return parse_json_string_literal(text, value_start, close, value) &&
         close + 1 == value_end;
}

inline bool extract_string_key_fallback(const std::string& text,
                                        const std::string& key,
                                        std::string& value) {
  value.clear();
  if (extract_top_level_string_key_fallback(text, key, value)) return true;

  const std::size_t key_pos = find_json_key_fallback(text, key);
  if (key_pos == std::string::npos) return false;
  const std::size_t open =
      json_value_start_after_key_fallback(text, key_pos);
  if (open >= text.size() || text[open] != '"') return false;
  std::size_t close = std::string::npos;
  if (!parse_json_string_literal(text, open, close, value)) return false;
  return is_json_value_boundary(text, skip_json_space(text, close + 1));
}

inline std::string extract_string_key_fallback(const std::string& text,
                                               const std::string& key) {
  std::string value;
  return extract_string_key_fallback(text, key, value) ? value : "";
}

inline bool extract_semantic_string_key_fallback(const std::string& text,
                                                 const std::string& key,
                                                 std::string& value) {
  value.clear();
  return json_root_is_object(text)
             ? extract_top_level_string_key_fallback(text, key, value)
             : extract_string_key_fallback(text, key, value);
}

inline std::string extract_semantic_string_key_fallback(
    const std::string& text, const std::string& key) {
  std::string value;
  return extract_semantic_string_key_fallback(text, key, value) ? value : "";
}

inline bool extract_bool_key_fallback(const std::string& text,
                                      const std::string& key,
                                      bool& value) {
  std::size_t value_start = std::string::npos;
  std::size_t value_end = std::string::npos;
  if (find_top_level_json_value_fallback(text, key, value_start, value_end)) {
    if (text.compare(value_start, 4, "true") == 0 &&
        value_start + 4 == value_end) {
      value = true;
      return true;
    }
    if (text.compare(value_start, 5, "false") == 0 &&
        value_start + 5 == value_end) {
      value = false;
      return true;
    }
    return false;
  }
  if (json_root_is_object(text)) return false;

  const std::size_t key_pos = find_json_key_fallback(text, key);
  if (key_pos == std::string::npos) return false;
  const std::size_t start =
      json_value_start_after_key_fallback(text, key_pos);
  if (start == std::string::npos) return false;
  if (text.compare(start, 4, "true") == 0 &&
      is_json_value_boundary(text, start + 4)) {
    value = true;
    return true;
  }
  if (text.compare(start, 5, "false") == 0 &&
      is_json_value_boundary(text, start + 5)) {
    value = false;
    return true;
  }
  return false;
}

inline bool json_bool_value_or_false(const std::string& text,
                                     const std::string& key) {
  bool value = false;
  return extract_bool_key_fallback(text, key, value) && value;
}

inline bool json_bool_value_equals(const std::string& text,
                                   const std::string& key,
                                   const bool expected) {
  bool value = false;
  return extract_bool_key_fallback(text, key, value) && value == expected;
}

inline std::string json_number_or_null(const std::string& text,
                                       const std::string& key) {
  std::size_t start = std::string::npos;
  std::size_t value_end = std::string::npos;
  if (!find_top_level_json_value_fallback(text, key, start, value_end)) {
    if (json_root_is_object(text)) return "null";
    const std::size_t key_pos = find_json_key_fallback(text, key);
    if (key_pos == std::string::npos) return "null";
    start = json_value_start_after_key_fallback(text, key_pos);
  }
  std::size_t end = start;
  if (end < text.size() && text[end] == '-') ++end;
  if (end >= text.size()) return "null";
  if (text[end] == '0') {
    ++end;
    if (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      return "null";
    }
  } else {
    const std::size_t digit_start = end;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      ++end;
    }
    if (end == digit_start) return "null";
  }
  if (end < text.size() && text[end] == '.') {
    ++end;
    const std::size_t fraction_start = end;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      ++end;
    }
    if (end == fraction_start) return "null";
  }
  if (end < text.size() && (text[end] == 'e' || text[end] == 'E')) {
    ++end;
    if (end < text.size() && (text[end] == '+' || text[end] == '-')) ++end;
    const std::size_t exponent_start = end;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      ++end;
    }
    if (end == exponent_start) return "null";
  }
  if (value_end != std::string::npos && end != value_end) return "null";
  if (value_end == std::string::npos && !is_json_value_boundary(text, end)) {
    return "null";
  }
  return text.substr(start, end - start);
}

inline bool try_parse_unsigned_value(const std::string& value,
                                     std::size_t& result) {
  result = 0;
  if (value.empty()) return false;
  for (const char ch : value) {
    if (ch < '0' || ch > '9') return false;
    const auto digit = static_cast<std::size_t>(ch - '0');
    if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10U) {
      result = 0;
      return false;
    }
    result = result * 10U + digit;
  }
  return true;
}

inline std::size_t unsigned_value_or_zero(const std::string& value) {
  std::size_t result = 0;
  if (!try_parse_unsigned_value(value, result)) return 0;
  return result;
}

inline bool extract_unsigned_key_fallback(const std::string& text,
                                          const std::string& key,
                                          std::size_t& result) {
  return try_parse_unsigned_value(json_number_or_null(text, key), result);
}

inline std::size_t json_number_value_or_zero(const std::string& text,
                                             const std::string& key) {
  return unsigned_value_or_zero(json_number_or_null(text, key));
}

inline std::string json_string_or_null(const std::string& text,
                                       const std::string& key) {
  std::string value;
  return extract_semantic_string_key_fallback(text, key, value)
             ? inferlab_plan::json_string(value)
             : "null";
}

inline bool json_string_value_equals(const std::string& text,
                                     const std::string& key,
                                     const std::string& expected) {
  std::string value;
  return extract_semantic_string_key_fallback(text, key, value) &&
         value == expected;
}

inline bool json_mode_matches(const std::string& actual,
                              const std::string& expected) {
  if (actual == expected) return true;
  if (expected == "dry_run") {
    return actual == "dry_run_plan" || actual == "dry_run_ready";
  }
  if (expected == "replay") {
    return actual == "replay_supported" || actual == "replay_ready";
  }
  return false;
}

inline bool json_mode_value_equals(const std::string& text,
                                   const std::string& expected_mode) {
  std::string mode;
  if (extract_top_level_string_key_fallback(text, "mode", mode)) {
    return json_mode_matches(mode, expected_mode);
  }
  if (extract_top_level_string_key_fallback(text, "execution_kind", mode)) {
    return json_mode_matches(mode, expected_mode);
  }
  if (json_root_is_object(text)) return false;
  if (extract_semantic_string_key_fallback(text, "mode", mode)) {
    return json_mode_matches(mode, expected_mode);
  }
  if (extract_semantic_string_key_fallback(text, "execution_kind", mode)) {
    return json_mode_matches(mode, expected_mode);
  }
  return false;
}

inline std::string json_object_section_or_empty(const std::string& text,
                                                const std::string& key) {
  std::size_t value_start = std::string::npos;
  std::size_t value_end = std::string::npos;
  if (find_top_level_json_value_fallback(text, key, value_start, value_end) &&
      value_start < text.size() && text[value_start] == '{') {
    return text.substr(value_start, value_end - value_start);
  }
  if (json_root_is_object(text)) return "";

  const std::size_t key_pos = find_json_key_fallback(text, key);
  if (key_pos == std::string::npos) return "";
  const std::size_t open =
      json_value_start_after_key_fallback(text, key_pos);
  if (open >= text.size() || text[open] != '{') return "";
  const std::size_t end = find_json_value_end(text, open);
  return end == std::string::npos ? "" : text.substr(open, end - open);
}

inline std::string json_object_section_or_root_text(
    const TextReadResult& read_result, const std::string& key) {
  if (!read_result.readable) return "";
  const std::string section =
      json_object_section_or_empty(read_result.text, key);
  return section.empty() ? read_result.text : section;
}

struct StrictJsonObjectSectionOrRootResult {
  std::string text;
  bool usable = false;
};

inline StrictJsonObjectSectionOrRootResult
strict_json_object_section_or_flattened_root(const std::string& document,
                                             const std::string& key) {
  StrictJsonObjectSectionOrRootResult result;
  if (document.empty() || !json_document_is_valid_object(document)) {
    return result;
  }
  if (contains_top_level_key_fallback(document, key)) {
    result.text = json_object_section_or_empty(document, key);
    if (result.text.empty()) return result;
    result.usable = true;
    return result;
  }
  result.text = document;
  result.usable = true;
  return result;
}

inline std::string json_array_section_or_empty(const std::string& text,
                                               const std::string& key) {
  std::size_t value_start = std::string::npos;
  std::size_t value_end = std::string::npos;
  if (find_top_level_json_value_fallback(text, key, value_start, value_end) &&
      value_start < text.size() && text[value_start] == '[') {
    return text.substr(value_start, value_end - value_start);
  }
  if (json_root_is_object(text)) return "";

  const std::size_t key_pos = find_json_key_fallback(text, key);
  if (key_pos == std::string::npos) return "";
  const std::size_t open =
      json_value_start_after_key_fallback(text, key_pos);
  if (open >= text.size() || text[open] != '[') return "";
  const std::size_t end = find_json_value_end(text, open);
  return end == std::string::npos ? "" : text.substr(open, end - open);
}

inline std::vector<std::string> top_level_json_values(
    const std::string& section) {
  std::vector<std::string> values;
  std::size_t pos = skip_json_space(section, 0);
  if (pos >= section.size() || section[pos] != '[') return values;
  pos = skip_json_space(section, pos + 1);
  if (pos < section.size() && section[pos] == ']') {
    ++pos;
    return skip_json_space(section, pos) == section.size()
               ? values
               : std::vector<std::string>{};
  }
  while (pos < section.size()) {
    const std::size_t value_start = pos;
    if (!parse_json_value_syntax(section, pos, 1)) return {};
    values.push_back(section.substr(value_start, pos - value_start));
    pos = skip_json_space(section, pos);
    if (pos >= section.size()) return {};
    if (section[pos] == ']') {
      ++pos;
      return skip_json_space(section, pos) == section.size()
                 ? values
                 : std::vector<std::string>{};
    }
    if (section[pos] != ',') return {};
    pos = skip_json_space(section, pos + 1);
  }
  return {};
}

inline std::vector<std::string> top_level_json_objects(
    const std::string& section) {
  std::vector<std::string> objects;
  for (const auto& value : top_level_json_values(section)) {
    const std::size_t start = skip_json_space(value, 0);
    if (start < value.size() && value[start] == '{') objects.push_back(value);
  }
  return objects;
}

inline std::size_t count_array_items_fallback(const std::string& text,
                                              const std::string& key) {
  return top_level_json_values(json_array_section_or_empty(text, key)).size();
}

inline std::size_t count_key_occurrences(const std::string& text,
                                         const std::string& key) {
  if (text.empty()) return 0;
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = find_json_key_fallback(text, key, pos)) !=
         std::string::npos) {
    ++count;
    ++pos;
  }
  return count;
}

inline bool parse_string_array_items_fallback(
    const std::string& section, std::vector<std::string>& values) {
  values.clear();
  std::size_t pos = skip_json_space(section, 0);
  if (pos >= section.size() || section[pos] != '[') return false;
  pos = skip_json_space(section, pos + 1);
  if (pos < section.size() && section[pos] == ']') {
    ++pos;
    return skip_json_space(section, pos) == section.size();
  }
  while (pos < section.size()) {
    if (section[pos] != '"') return false;
    std::size_t close = std::string::npos;
    std::string value;
    if (!parse_json_string_literal(section, pos, close, value)) return false;
    values.push_back(std::move(value));
    pos = skip_json_space(section, close + 1);
    if (pos >= section.size()) return false;
    if (section[pos] == ']') {
      ++pos;
      return skip_json_space(section, pos) == section.size();
    }
    if (section[pos] != ',') return false;
    pos = skip_json_space(section, pos + 1);
  }
  return false;
}

inline std::size_t count_string_array_items_after(const std::string& text,
                                                  const std::string& key) {
  std::vector<std::string> values;
  return parse_string_array_items_fallback(
             json_array_section_or_empty(text, key), values)
             ? values.size()
             : 0U;
}

inline std::string first_string_array_item_or_empty(const std::string& text,
                                                    const std::string& key) {
  const std::string section = json_array_section_or_empty(text, key);
  std::vector<std::string> values;
  return parse_string_array_items_fallback(section, values) && !values.empty()
             ? values.front()
             : "";
}

inline Metadata read_metadata(const fs::path& path,
                              const TextReadResult& read_result) {
  Metadata meta;
  meta.source = path.filename().string();
  meta.present = read_result.present;
  if (!meta.present) {
    if (!read_result.error.empty()) {
      meta.warnings.push_back("artifact file presence probe failed: " +
                              read_result.error);
    } else {
      meta.warnings.push_back("artifact file missing");
    }
    return meta;
  }

  const std::string& text = read_result.text;
  if (!read_result.readable) {
    meta.warnings.push_back("artifact file unreadable: " +
                            (read_result.error.empty() ? read_result.status
                                                       : read_result.error));
    return meta;
  }
  if (read_result.empty) {
    meta.warnings.push_back("artifact file is empty");
    return meta;
  }
  if (!json_document_is_valid_object(text)) {
    meta.warnings.push_back(
        "artifact JSON failed fallback syntax and uniqueness validation");
    return meta;
  }

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
  try {
    const auto payload = nlohmann::json::parse(text);
    if (!payload.is_object()) {
      meta.warnings.push_back("artifact JSON root is not an object");
      return meta;
    }
    meta.used_structured_parser = true;
    meta.parse_ok = true;
    meta.schema_present = payload.contains("schema") || payload.contains("schema_version");
    meta.summary_present = payload.contains("summary");
    if (payload.contains("artifact_type") && payload["artifact_type"].is_string()) {
      meta.artifact_type = payload["artifact_type"].get<std::string>();
    } else if (payload.contains("schema") && payload["schema"].is_object() &&
               payload["schema"].contains("name") &&
               payload["schema"]["name"].is_string()) {
      meta.artifact_type = payload["schema"]["name"].get<std::string>();
    }
    if (payload.contains("mode") && payload["mode"].is_string()) {
      meta.mode = payload["mode"].get<std::string>();
    } else if (payload.contains("execution_kind") &&
               payload["execution_kind"].is_string()) {
      meta.mode = payload["execution_kind"].get<std::string>();
    } else {
      meta.mode = "unknown";
    }
    if (payload.contains("claim_scope") && payload["claim_scope"].is_string()) {
      meta.claim_scope_present = true;
      meta.claim_scope = payload["claim_scope"].get<std::string>();
    } else if (payload.contains("claim_scope_detail")) {
      meta.claim_scope_present = true;
      meta.claim_scope = "claim_scope_detail";
    }
    if (payload.contains("executed") && payload["executed"].is_boolean()) {
      meta.executed_present = true;
      meta.executed = payload["executed"].get<bool>();
    }
    if (payload.contains("replayed") && payload["replayed"].is_boolean()) {
      meta.replayed_present = true;
      meta.replayed = payload["replayed"].get<bool>();
    }
    if (payload.contains("target_gpu_execute_verified") &&
        payload["target_gpu_execute_verified"].is_boolean()) {
      meta.target_gpu_execute_verified_present = true;
      meta.target_gpu_execute_verified =
          payload["target_gpu_execute_verified"].get<bool>();
    }
    if (payload.contains("summary") && payload["summary"].is_object()) {
      const auto& summary = payload["summary"];
      if (summary.contains("release_decision") &&
          summary["release_decision"].is_string()) {
        meta.release_decision = summary["release_decision"].get<std::string>();
      } else if (summary.contains("gate_decision") &&
                 summary["gate_decision"].is_string()) {
        meta.release_decision = summary["gate_decision"].get<std::string>();
      } else if (summary.contains("warehouse_decision") &&
                 summary["warehouse_decision"].is_string()) {
        meta.release_decision = summary["warehouse_decision"].get<std::string>();
      }
      if (summary.contains("blocker_count") && summary["blocker_count"].is_number_unsigned()) {
        meta.blocker_count = summary["blocker_count"].get<std::size_t>();
      }
    }
    if (payload.contains("blockers") && payload["blockers"].is_array()) {
      meta.blocker_count = std::max(meta.blocker_count, payload["blockers"].size());
    }
    return meta;
  } catch (const std::exception& exc) {
    meta.warnings.push_back(
        "structured JSON parse failed: " +
        inferlab_plan::redact_error_message(exc.what()));
    return meta;
  }
#endif

  meta.parse_ok = true;
  meta.used_structured_parser = false;
  meta.schema_present = contains_semantic_key_fallback(text, "schema") ||
                        contains_semantic_key_fallback(text, "schema_version");
  meta.summary_present = contains_semantic_key_fallback(text, "summary");
  meta.artifact_type =
      extract_semantic_string_key_fallback(text, "artifact_type");
  if (meta.artifact_type.empty()) {
    const std::string schema_section = json_object_section_or_empty(text, "schema");
    meta.artifact_type =
        extract_string_key_fallback(schema_section, "name");
  }
  if (meta.artifact_type.empty() && !json_root_is_object(text)) {
    meta.artifact_type = extract_string_key_fallback(text, "name");
  }
  if (meta.artifact_type.empty()) meta.artifact_type = "unknown";
  meta.mode = extract_semantic_string_key_fallback(text, "mode");
  if (meta.mode.empty()) {
    meta.mode = extract_semantic_string_key_fallback(text, "execution_kind");
  }
  if (meta.mode.empty()) meta.mode = "unknown";
  const bool claim_scope_string_present =
      extract_semantic_string_key_fallback(text, "claim_scope",
                                           meta.claim_scope);
  meta.claim_scope_present =
      claim_scope_string_present ||
      contains_semantic_key_fallback(text, "claim_scope_detail");
  meta.executed_present = extract_bool_key_fallback(text, "executed", meta.executed);
  meta.replayed_present = extract_bool_key_fallback(text, "replayed", meta.replayed);
  meta.target_gpu_execute_verified_present = extract_bool_key_fallback(
      text, "target_gpu_execute_verified",
      meta.target_gpu_execute_verified);
  const std::string summary_section = json_object_section_or_empty(text, "summary");
  if (!summary_section.empty()) {
    meta.release_decision =
        extract_string_key_fallback(summary_section, "release_decision");
    if (meta.release_decision.empty()) {
      meta.release_decision =
          extract_string_key_fallback(summary_section, "gate_decision");
    }
    if (meta.release_decision.empty()) {
      meta.release_decision =
          extract_string_key_fallback(summary_section, "warehouse_decision");
    }
  }
  if (meta.release_decision.empty()) {
    meta.release_decision =
        extract_semantic_string_key_fallback(text, "release_decision");
  }
  if (meta.release_decision.empty()) {
    meta.release_decision =
        extract_semantic_string_key_fallback(text, "gate_decision");
  }
  if (meta.release_decision.empty()) {
    meta.release_decision =
        extract_semantic_string_key_fallback(text, "warehouse_decision");
  }
  meta.blocker_count = count_array_items_fallback(text, "blockers");
  meta.warnings.push_back("used fallback JSON metadata scan");
  return meta;
}

inline Metadata read_metadata(const fs::path& path) {
  return read_metadata(path, read_text_with_status(path));
}

inline std::string metadata_json(const Metadata& meta, const int indent) {
  const std::string pad(indent, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << pad << "  \"source\": " << inferlab_plan::json_string(meta.source) << ",\n";
  out << pad << "  \"present\": " << (meta.present ? "true" : "false") << ",\n";
  out << pad << "  \"parse_ok\": " << (meta.parse_ok ? "true" : "false") << ",\n";
  out << pad << "  \"parser\": "
      << inferlab_plan::json_string(meta.used_structured_parser ? "nlohmann_json" : "fallback_scan") << ",\n";
  out << pad << "  \"artifact_type\": " << inferlab_plan::json_string(meta.artifact_type) << ",\n";
  out << pad << "  \"mode\": " << inferlab_plan::json_string(meta.mode) << ",\n";
  out << pad << "  \"executed_present\": "
      << (meta.executed_present ? "true" : "false") << ",\n";
  out << pad << "  \"replayed_present\": "
      << (meta.replayed_present ? "true" : "false") << ",\n";
  out << pad << "  \"target_gpu_execute_verified_present\": "
      << (meta.target_gpu_execute_verified_present ? "true" : "false")
      << ",\n";
  out << pad << "  \"executed\": " << (meta.executed ? "true" : "false") << ",\n";
  out << pad << "  \"replayed\": " << (meta.replayed ? "true" : "false") << ",\n";
  out << pad << "  \"target_gpu_execute_verified\": "
      << (meta.target_gpu_execute_verified ? "true" : "false") << ",\n";
  out << pad << "  \"schema_present\": " << (meta.schema_present ? "true" : "false") << ",\n";
  out << pad << "  \"summary_present\": " << (meta.summary_present ? "true" : "false") << ",\n";
  out << pad << "  \"claim_scope_present\": " << (meta.claim_scope_present ? "true" : "false") << ",\n";
  out << pad << "  \"claim_scope\": " << inferlab_plan::json_string(meta.claim_scope) << ",\n";
  out << pad << "  \"release_decision\": " << inferlab_plan::json_string(meta.release_decision) << ",\n";
  out << pad << "  \"blocker_count\": " << meta.blocker_count << ",\n";
  out << pad << "  \"warnings\": "
      << inferlab_plan::json_string_array(meta.warnings, indent + 2) << "\n";
  out << pad << "}";
  return out.str();
}

inline Metadata metadata_from_fallback_object(const std::string& object,
                                             const std::string& source,
                                             const bool present) {
  Metadata metadata;
  metadata.source = extract_string_key_fallback(object, "source");
  if (metadata.source.empty()) metadata.source = source;
  metadata.present = present;
  bool bool_value = false;
  if (extract_bool_key_fallback(object, "present", bool_value)) {
    metadata.present = bool_value;
  }
  metadata.parse_ok = false;
  if (extract_bool_key_fallback(object, "parse_ok", bool_value)) {
    metadata.parse_ok = bool_value;
  }
  const std::string parser = extract_string_key_fallback(object, "parser");
  metadata.used_structured_parser = parser == "nlohmann_json";
  metadata.artifact_type = extract_string_key_fallback(object, "artifact_type");
  if (metadata.artifact_type.empty()) metadata.artifact_type = "unknown";
  metadata.mode = extract_string_key_fallback(object, "mode");
  if (metadata.mode.empty()) metadata.mode = "unknown";
  bool explicit_present = false;
  if (extract_bool_key_fallback(object, "executed_present", explicit_present)) {
    metadata.executed_present = explicit_present;
    if (explicit_present) {
      extract_bool_key_fallback(object, "executed", metadata.executed);
    }
  } else {
    metadata.executed_present =
        extract_bool_key_fallback(object, "executed", metadata.executed);
  }
  if (extract_bool_key_fallback(object, "replayed_present", explicit_present)) {
    metadata.replayed_present = explicit_present;
    if (explicit_present) {
      extract_bool_key_fallback(object, "replayed", metadata.replayed);
    }
  } else {
    metadata.replayed_present =
        extract_bool_key_fallback(object, "replayed", metadata.replayed);
  }
  if (extract_bool_key_fallback(
          object, "target_gpu_execute_verified_present", explicit_present)) {
    metadata.target_gpu_execute_verified_present = explicit_present;
    if (explicit_present) {
      extract_bool_key_fallback(object, "target_gpu_execute_verified",
                                metadata.target_gpu_execute_verified);
    }
  } else {
    metadata.target_gpu_execute_verified_present = extract_bool_key_fallback(
        object, "target_gpu_execute_verified",
        metadata.target_gpu_execute_verified);
  }
  metadata.schema_present =
      extract_bool_key_fallback(object, "schema_present", bool_value)
          ? bool_value
          : false;
  metadata.summary_present =
      extract_bool_key_fallback(object, "summary_present", bool_value)
          ? bool_value
          : false;
  metadata.claim_scope_present =
      extract_bool_key_fallback(object, "claim_scope_present", bool_value)
          ? bool_value
          : false;
  metadata.claim_scope = extract_string_key_fallback(object, "claim_scope");
  metadata.release_decision =
      extract_string_key_fallback(object, "release_decision");
  metadata.blocker_count =
      unsigned_value_or_zero(json_number_or_null(object, "blocker_count"));
  metadata.warnings.push_back("metadata parsed from fallback warehouse scan");
  return metadata;
}

inline WarehouseIndex read_warehouse_index(const fs::path& path) {
  WarehouseIndex index;
  index.path = path.string();
  const TextReadResult read_result = read_text_with_status(path);
  index.present = read_result.present;
  if (!index.present) {
    if (!read_result.error.empty()) {
      index.warnings.push_back("evidence warehouse presence probe failed: " +
                               read_result.error);
    } else {
      index.warnings.push_back("evidence warehouse artifact missing; falling back to direct repo/results scan");
    }
    return index;
  }

  const std::string& text = read_result.text;
  if (!read_result.readable) {
    index.warnings.push_back("evidence warehouse artifact unreadable: " +
                             (read_result.error.empty() ? read_result.status
                                                        : read_result.error));
    return index;
  }
  if (read_result.empty) {
    index.warnings.push_back("evidence warehouse artifact is empty");
    return index;
  }
  if (!json_document_is_valid_object(text)) {
    index.warnings.push_back(
        "warehouse JSON failed fallback syntax and uniqueness validation");
    return index;
  }
  std::string warehouse_artifact_type;
  std::string warehouse_mode;
  std::string warehouse_claim_scope;
  bool warehouse_executed = true;
  bool warehouse_replayed = true;
  if (!extract_top_level_string_key_fallback(
          text, "artifact_type", warehouse_artifact_type) ||
      warehouse_artifact_type != "native_evidence_warehouse_index" ||
      !extract_top_level_string_key_fallback(text, "mode", warehouse_mode) ||
      warehouse_mode != "dry_run" ||
      !extract_bool_key_fallback(text, "executed", warehouse_executed) ||
      warehouse_executed ||
      !extract_bool_key_fallback(text, "replayed", warehouse_replayed) ||
      warehouse_replayed ||
      !extract_top_level_string_key_fallback(
          text, "claim_scope", warehouse_claim_scope) ||
      warehouse_claim_scope.empty() ||
      json_object_section_or_empty(text, "summary").empty() ||
      json_array_section_or_empty(text, "artifacts").empty() ||
      json_array_section_or_empty(text, "scanned_raw_artifacts").empty() ||
      json_array_section_or_empty(text, "lineage_edges").empty() ||
      count_array_items_fallback(text, "artifacts") == 0U ||
      count_array_items_fallback(text, "lineage_edges") == 0U) {
    index.warnings.push_back(
        "warehouse JSON failed identity, mode, flag, claim, summary, artifacts, or lineage contract");
    return index;
  }

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
  try {
    const auto payload = nlohmann::json::parse(text);
    if (!payload.is_object() || !payload.contains("artifacts") ||
        !payload["artifacts"].is_array() || payload["artifacts"].empty()) {
      index.warnings.push_back("warehouse JSON lacks artifacts array");
      return index;
    }
    const auto metadata_from_json =
        [](const nlohmann::json& meta, const std::string& source,
           const bool present) {
          Metadata metadata;
          metadata.source = meta.value("source", source);
          metadata.present = meta.value("present", present);
          metadata.parse_ok = meta.value("parse_ok", false);
          metadata.used_structured_parser =
              meta.value("parser", "") == "nlohmann_json";
          metadata.artifact_type = meta.value("artifact_type", "unknown");
          metadata.mode = meta.value("mode", "unknown");
          const bool has_executed_present =
              meta.contains("executed_present") &&
              meta["executed_present"].is_boolean();
          metadata.executed_present =
              has_executed_present
                  ? meta["executed_present"].get<bool>()
                  : (meta.contains("executed") && meta["executed"].is_boolean());
          if (metadata.executed_present && meta.contains("executed") &&
              meta["executed"].is_boolean()) {
            metadata.executed = meta["executed"].get<bool>();
          }
          const bool has_replayed_present =
              meta.contains("replayed_present") &&
              meta["replayed_present"].is_boolean();
          metadata.replayed_present =
              has_replayed_present
                  ? meta["replayed_present"].get<bool>()
                  : (meta.contains("replayed") && meta["replayed"].is_boolean());
          if (metadata.replayed_present && meta.contains("replayed") &&
              meta["replayed"].is_boolean()) {
            metadata.replayed = meta["replayed"].get<bool>();
          }
          const bool has_target_gpu_execute_verified_present =
              meta.contains("target_gpu_execute_verified_present") &&
              meta["target_gpu_execute_verified_present"].is_boolean();
          metadata.target_gpu_execute_verified_present =
              has_target_gpu_execute_verified_present
                  ? meta["target_gpu_execute_verified_present"].get<bool>()
                  : (meta.contains("target_gpu_execute_verified") &&
                     meta["target_gpu_execute_verified"].is_boolean());
          if (metadata.target_gpu_execute_verified_present &&
              meta.contains("target_gpu_execute_verified") &&
              meta["target_gpu_execute_verified"].is_boolean()) {
            metadata.target_gpu_execute_verified =
                meta["target_gpu_execute_verified"].get<bool>();
          }
          metadata.schema_present = meta.value("schema_present", false);
          metadata.summary_present = meta.value("summary_present", false);
          metadata.claim_scope_present =
              meta.value("claim_scope_present", false);
          metadata.claim_scope = meta.value("claim_scope", "");
          metadata.release_decision = meta.value("release_decision", "");
          metadata.blocker_count = meta.value("blocker_count", 0U);
          return metadata;
        };
    std::set<std::string> managed_artifacts;
    for (const auto& item : payload["artifacts"]) {
      if (!item.is_object() || !item.contains("artifact") ||
          !item["artifact"].is_string() ||
          !item.contains("metadata") || !item["metadata"].is_object()) {
        index.records.clear();
        index.warnings.push_back(
            "warehouse artifacts array contains an invalid record");
        return index;
      }
      WarehouseArtifactRecord record;
      record.artifact = item["artifact"].get<std::string>();
      if (record.artifact.empty() ||
          !managed_artifacts.insert(record.artifact).second) {
        index.records.clear();
        index.warnings.push_back(
            "warehouse artifacts array contains an empty or duplicate artifact identity");
        return index;
      }
      record.present = item.value("present", false);
      record.matrix_present = item.value("matrix_present", false);
      record.summary_present = item.value("summary_present", false);
      record.prometheus_present = item.value("prometheus_present", false);
      record.report_present = item.value("report_present", false);
      record.final_report_present = item.value("final_report_present", false);
      record.contract_present = item.value("contract_present", false);
      record.freshness_present = item.value("freshness_present", false);
      record.metadata =
          metadata_from_json(item["metadata"], record.artifact, record.present);
      index.records.push_back(record);
    }
    if (!payload.contains("scanned_raw_artifacts") ||
        !payload["scanned_raw_artifacts"].is_array()) {
      index.records.clear();
      index.warnings.push_back(
          "warehouse JSON lacks a scanned_raw_artifacts array");
      return index;
    }
    {
      std::set<std::string> scanned_artifacts;
      for (const auto& meta : payload["scanned_raw_artifacts"]) {
        if (!meta.is_object() || !meta.contains("source") ||
            !meta["source"].is_string()) {
          index.records.clear();
          index.warnings.push_back(
              "warehouse scanned_raw_artifacts contains an invalid record");
          return index;
        }
        const std::string source = meta["source"].get<std::string>();
        if (source.empty() || !scanned_artifacts.insert(source).second) {
          index.records.clear();
          index.warnings.push_back(
              "warehouse scanned_raw_artifacts contains an empty or duplicate source identity");
          return index;
        }
        bool already_indexed = false;
        for (const auto& existing : index.records) {
          if (existing.artifact == source) {
            already_indexed = true;
            break;
          }
        }
        if (already_indexed) continue;
        WarehouseArtifactRecord record;
        record.artifact = source;
        record.present = meta.value("present", false);
        record.metadata = metadata_from_json(meta, source, record.present);
        index.records.push_back(record);
      }
    }
    index.parsed = true;
    index.used_structured_parser = true;
    return index;
  } catch (const std::exception& exc) {
    index.warnings.push_back(
        "warehouse JSON parse failed: " +
        inferlab_plan::redact_error_message(exc.what()));
    return index;
  }
#endif

  const std::string artifacts_section =
      json_array_section_or_empty(text, "artifacts");
  if (artifacts_section.empty()) {
    index.warnings.push_back("warehouse JSON lacks artifacts array");
    return index;
  }
  const auto artifact_values = top_level_json_values(artifacts_section);
  const auto artifact_objects = top_level_json_objects(artifacts_section);
  if (artifact_values.empty() ||
      artifact_values.size() != artifact_objects.size()) {
    index.warnings.push_back(
        "warehouse fallback artifacts array must contain only objects");
    return index;
  }
  std::set<std::string> managed_artifacts;
  for (const auto& item : artifact_objects) {
    const std::string artifact = extract_string_key_fallback(item, "artifact");
    const std::string metadata_object =
        json_object_section_or_empty(item, "metadata");
    if (artifact.empty() || metadata_object.empty() ||
        !managed_artifacts.insert(artifact).second) {
      index.records.clear();
      index.warnings.push_back(
          "warehouse fallback artifacts scan found an invalid or duplicate record");
      return index;
    }
    WarehouseArtifactRecord record;
    record.artifact = artifact;
    extract_bool_key_fallback(item, "present", record.present);
    extract_bool_key_fallback(item, "matrix_present", record.matrix_present);
    extract_bool_key_fallback(item, "summary_present", record.summary_present);
    extract_bool_key_fallback(item, "prometheus_present",
                              record.prometheus_present);
    extract_bool_key_fallback(item, "report_present", record.report_present);
    extract_bool_key_fallback(item, "final_report_present",
                              record.final_report_present);
    extract_bool_key_fallback(item, "contract_present",
                              record.contract_present);
    extract_bool_key_fallback(item, "freshness_present",
                              record.freshness_present);
    record.metadata =
        metadata_from_fallback_object(metadata_object, record.artifact,
                                      record.present);
    index.records.push_back(record);
  }

  const std::string scanned_section =
      json_array_section_or_empty(text, "scanned_raw_artifacts");
  const auto scanned_values = top_level_json_values(scanned_section);
  const auto scanned_objects = top_level_json_objects(scanned_section);
  if (scanned_values.size() != scanned_objects.size()) {
    index.records.clear();
    index.warnings.push_back(
        "warehouse fallback raw scan array must contain only objects");
    return index;
  }
  std::set<std::string> scanned_artifacts;
  for (const auto& meta : scanned_objects) {
    const std::string source = extract_string_key_fallback(meta, "source");
    if (source.empty() || !scanned_artifacts.insert(source).second) {
      index.records.clear();
      index.warnings.push_back(
          "warehouse fallback raw scan found an empty or duplicate source identity");
      return index;
    }
    bool already_indexed = false;
    for (const auto& existing : index.records) {
      if (existing.artifact == source) {
        already_indexed = true;
        break;
      }
    }
    if (already_indexed) continue;
    WarehouseArtifactRecord record;
    record.artifact = source;
    extract_bool_key_fallback(meta, "present", record.present);
    record.metadata =
        metadata_from_fallback_object(meta, source, record.present);
    index.records.push_back(record);
  }

  index.parsed = true;
  index.used_structured_parser = false;
  index.warnings.push_back("warehouse parsed with fallback JSON scan");
  return index;
}

inline const WarehouseArtifactRecord* find_warehouse_record(
    const WarehouseIndex& index, const std::string& artifact) {
  for (const auto& record : index.records) {
    if (record.artifact == artifact) return &record;
  }
  return nullptr;
}

inline std::string warehouse_index_json(const WarehouseIndex& index, const int indent) {
  const std::string pad(indent, ' ');
  const std::string path_label = inferlab_plan::redacted_path_label(index.path);
  std::ostringstream out;
  out << pad << "{\n";
  out << pad << "  \"path\": " << inferlab_plan::json_string(path_label) << ",\n";
  out << pad << "  \"path_redacted\": "
      << (inferlab_plan::path_was_redacted(index.path, path_label) ? "true" : "false")
      << ",\n";
  out << pad << "  \"present\": " << (index.present ? "true" : "false") << ",\n";
  out << pad << "  \"parsed\": " << (index.parsed ? "true" : "false") << ",\n";
  out << pad << "  \"parser\": "
      << inferlab_plan::json_string(index.used_structured_parser ? "nlohmann_json" : "fallback_or_unavailable") << ",\n";
  out << pad << "  \"record_count\": " << index.records.size() << ",\n";
  out << pad << "  \"warnings\": "
      << inferlab_plan::json_string_array(index.warnings, indent + 2) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_artifact
