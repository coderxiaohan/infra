#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace inferlab_plan {

inline std::string json_string(const std::string& value);

inline std::string json_bool(const bool value) {
  return value ? "true" : "false";
}

inline std::string json_indent(const int indent) {
  if (indent < 0 || indent > 256) {
    throw std::invalid_argument("JSON indent must be between 0 and 256");
  }
  return std::string(static_cast<std::size_t>(indent), ' ');
}

inline bool contains(const std::string& value, const std::string& needle) {
  return value.find(needle) != std::string::npos;
}

inline bool has_non_whitespace(const std::string& value) {
  return std::any_of(value.begin(), value.end(), [](const unsigned char ch) {
    return std::isspace(ch) == 0;
  });
}

inline bool append_unique_string(std::vector<std::string>& values,
                                 const std::string& value) {
  if (std::find(values.begin(), values.end(), value) != values.end()) {
    return false;
  }
  values.push_back(value);
  return true;
}

inline bool append_unique_nonempty_string(
    std::vector<std::string>& values,
    const std::string& value) {
  return !value.empty() && append_unique_string(values, value);
}

inline std::string hex_u64(const std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

inline std::uint64_t fnv1a64(const std::string& value) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : value) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::string fnv1a64_fingerprint(const std::string& value) {
  return "fnv1a64:" + hex_u64(fnv1a64(value));
}

inline bool is_non_sensitive_path_placeholder(const std::string& value) {
  return value.empty() || value == "missing" || value == "." || value == "..";
}

inline std::string redacted_path_label(const std::string& value) {
  if (is_non_sensitive_path_placeholder(value)) return value;
  const std::filesystem::path path(value);
  std::string suffix = path.extension().string();
  if (suffix.size() > 16) suffix.clear();
  return "path_" + hex_u64(fnv1a64(value)) + suffix;
}

inline bool path_was_redacted(const std::string& value,
                              const std::string& label) {
  return !value.empty() && value != label;
}

inline std::string redacted_optional_path_json(const std::string& value) {
  return value.empty() ? "null" : json_string(redacted_path_label(value));
}

inline std::string optional_path_redacted_json_bool(
    const std::string& value) {
  if (value.empty()) return "false";
  const std::string label = redacted_path_label(value);
  return json_bool(path_was_redacted(value, label));
}

inline bool is_path_token_char(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isalnum(byte) != 0 || ch == '_' || ch == '-' || ch == '.' ||
         ch == '/' || ch == '\\' || ch == ':' || ch == '~';
}

inline bool looks_like_path_token(const std::string& token) {
  if (is_non_sensitive_path_placeholder(token)) return false;
  if (token.find("://") != std::string::npos) return false;
  if (token.rfind("/", 0) == 0 || token.rfind("~/", 0) == 0 ||
      token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0) {
    return true;
  }
  if (token.size() >= 3 && std::isalpha(static_cast<unsigned char>(token[0])) &&
      token[1] == ':' && (token[2] == '/' || token[2] == '\\')) {
    return true;
  }
  return token.find('/') != std::string::npos ||
         token.find('\\') != std::string::npos;
}

inline std::string redact_path_like_tokens(const std::string& message) {
  std::string redacted;
  redacted.reserve(message.size());
  std::size_t pos = 0;
  while (pos < message.size()) {
    if (!is_path_token_char(message[pos])) {
      redacted.push_back(message[pos]);
      ++pos;
      continue;
    }
    const std::size_t begin = pos;
    while (pos < message.size() && is_path_token_char(message[pos])) {
      ++pos;
    }
    const std::string token = message.substr(begin, pos - begin);
    redacted += looks_like_path_token(token) ? redacted_path_label(token) : token;
  }
  return redacted;
}

inline void replace_all(std::string& text,
                        const std::string& needle,
                        const std::string& replacement) {
  if (needle.empty() || needle == replacement) return;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    text.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
}

inline std::string redact_error_message(
    const std::string& message,
    const std::vector<std::string>& known_paths = {}) {
  std::string redacted = redact_path_like_tokens(message);
  for (const auto& path : known_paths) {
    if (is_non_sensitive_path_placeholder(path)) continue;
    replace_all(redacted, path, redacted_path_label(path));
  }
  return redacted;
}

inline void report_cli_error(
    const std::exception& error,
    const std::string& prefix = "error: ",
    const std::vector<std::string>& known_paths = {},
    std::ostream& out = std::cerr) {
  out << prefix << redact_error_message(error.what(), known_paths) << "\n";
}

inline std::string redacted_path_json_fields(const std::string& field_name,
                                             const std::string& value,
                                             const int indent,
                                             const bool trailing_comma = true) {
  const std::string pad = json_indent(indent);
  const std::string label = redacted_path_label(value);
  std::ostringstream out;
  out << pad << json_string(field_name) << ": " << json_string(label) << ",\n";
  out << pad << json_string(field_name + "_redacted") << ": "
      << (path_was_redacted(value, label) ? "true" : "false")
      << (trailing_comma ? ",\n" : "\n");
  return out.str();
}

inline double clamp01(const double value) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("clamp01 value must be finite");
  }
  return std::max(0.0, std::min(1.0, value));
}

inline std::tm utc_time_from_time_t(const std::time_t value) {
  std::tm result{};
#if defined(_WIN32)
  if (gmtime_s(&result, &value) != 0) {
    throw std::runtime_error("failed to convert UTC time");
  }
#else
  if (gmtime_r(&value, &result) == nullptr) {
    throw std::runtime_error("failed to convert UTC time");
  }
#endif
  return result;
}

inline bool ascii_digits_in_range(const std::string& value,
                                  const std::size_t offset,
                                  const std::size_t count,
                                  const int min_value,
                                  const int max_value) {
  if (offset > value.size() || count > value.size() - offset) return false;
  int parsed = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const char ch = value[offset + i];
    if (ch < '0' || ch > '9') return false;
    const int digit = ch - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  return parsed >= min_value && parsed <= max_value;
}

inline bool is_valid_utc_date(const std::string& value) {
  if (value.size() != 10 || value[4] != '-' || value[7] != '-' ||
      !ascii_digits_in_range(value, 0, 4, 1, 9999) ||
      !ascii_digits_in_range(value, 5, 2, 1, 12) ||
      !ascii_digits_in_range(value, 8, 2, 1, 31)) {
    return false;
  }
  const int year = std::stoi(value.substr(0, 4));
  const int month = std::stoi(value.substr(5, 2));
  const int day = std::stoi(value.substr(8, 2));
  static constexpr int kDaysByMonth[] = {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
  int max_day = kDaysByMonth[month - 1];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (month == 2 && leap) max_day = 29;
  return day <= max_day;
}

inline bool is_valid_utc_timestamp(const std::string& value) {
  return value.size() == 20 && value[10] == 'T' && value[13] == ':' &&
         value[16] == ':' && value[19] == 'Z' &&
         is_valid_utc_date(value.substr(0, 10)) &&
         ascii_digits_in_range(value, 11, 2, 0, 23) &&
         ascii_digits_in_range(value, 14, 2, 0, 59) &&
         ascii_digits_in_range(value, 17, 2, 0, 60);
}

inline std::string utc_time_string(const char* format) {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  const std::tm utc = utc_time_from_time_t(now_time);
  char buffer[32] = {};
  if (std::strftime(buffer, sizeof(buffer), format, &utc) == 0) {
    throw std::runtime_error("failed to format UTC time");
  }
  return buffer;
}

inline std::string current_utc_timestamp() {
  if (const char* override_value = std::getenv("INFERLAB_GENERATED_AT");
      override_value != nullptr && override_value[0] != '\0') {
    const std::string override_timestamp(override_value);
    if (!is_valid_utc_timestamp(override_timestamp)) {
      throw std::invalid_argument(
          "INFERLAB_GENERATED_AT must use YYYY-MM-DDTHH:MM:SSZ UTC format");
    }
    return override_timestamp;
  }
  return utc_time_string("%Y-%m-%dT%H:%M:%SZ");
}

inline std::string current_utc_date() {
  if (const char* override_value = std::getenv("INFERLAB_ACCESSED_AT");
      override_value != nullptr && override_value[0] != '\0') {
    const std::string override_date(override_value);
    if (!is_valid_utc_date(override_date)) {
      throw std::invalid_argument(
          "INFERLAB_ACCESSED_AT must use YYYY-MM-DD format");
    }
    return override_date;
  }
  if (const char* override_value = std::getenv("INFERLAB_GENERATED_AT");
      override_value != nullptr && override_value[0] != '\0') {
    const std::string override_timestamp(override_value);
    if (!is_valid_utc_timestamp(override_timestamp)) {
      throw std::invalid_argument(
          "INFERLAB_GENERATED_AT must use YYYY-MM-DDTHH:MM:SSZ UTC format");
    }
    return override_timestamp.substr(0, 10);
  }
  return utc_time_string("%Y-%m-%d");
}

inline int parse_bounded_int_arg(const std::string& name,
                                 const std::string& value,
                                 const int min_value,
                                 const int max_value) {
  if (min_value > max_value) {
    throw std::invalid_argument(name + " has invalid integer bounds [" +
                                std::to_string(min_value) + ", " +
                                std::to_string(max_value) + "]");
  }
  try {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    if (parsed < min_value || parsed > max_value) {
      throw std::invalid_argument("value out of allowed range");
    }
    return parsed;
  } catch (const std::exception& error) {
    throw std::invalid_argument(name + " must be an integer in [" +
                                std::to_string(min_value) + ", " +
                                std::to_string(max_value) + "]: " +
                                error.what());
  }
}

inline int parse_positive_int_arg(
    const std::string& name,
    const std::string& value,
    const int max_value = std::numeric_limits<int>::max()) {
  return parse_bounded_int_arg(name, value, 1, max_value);
}

inline constexpr int kStandardPositiveIntArgMax = 1048576;

inline int parse_standard_positive_int_arg(const std::string& name,
                                           const std::string& value) {
  return parse_positive_int_arg(name, value, kStandardPositiveIntArgMax);
}

inline int parse_non_negative_int_arg(
    const std::string& name,
    const std::string& value,
    const int max_value = std::numeric_limits<int>::max()) {
  return parse_bounded_int_arg(name, value, 0, max_value);
}

inline std::uint64_t parse_positive_u64_arg(
    const std::string& name,
    const std::string& value,
    const std::uint64_t max_value =
        std::numeric_limits<std::uint64_t>::max()) {
  if (max_value == 0U) {
    throw std::invalid_argument(name + " has an invalid uint64 upper bound");
  }
  try {
    if (value.empty()) {
      throw std::invalid_argument("empty value");
    }
    for (const char ch : value) {
      if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
        throw std::invalid_argument("expected unsigned decimal digits only");
      }
    }
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != value.size() || parsed == 0U || parsed > max_value) {
      throw std::invalid_argument("value out of allowed positive range");
    }
    return parsed;
  } catch (const std::exception& error) {
    throw std::invalid_argument(name + " must be a positive uint64 value: " +
                                redact_error_message(error.what()));
  }
}

inline double parse_bounded_double_arg(const std::string& name,
                                       const std::string& value,
                                       const double min_value,
                                       const double max_value) {
  if (!std::isfinite(min_value) || !std::isfinite(max_value) ||
      min_value > max_value) {
    std::ostringstream range;
    range << name << " has invalid finite-number bounds [" << min_value
          << ", " << max_value << "]";
    throw std::invalid_argument(range.str());
  }
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(parsed)) {
      throw std::invalid_argument("trailing characters or non-finite value");
    }
    if (parsed < min_value || parsed > max_value) {
      throw std::invalid_argument("value out of allowed range");
    }
    return parsed;
  } catch (const std::exception& error) {
    std::ostringstream range;
    range << name << " must be a finite number in [" << min_value << ", "
          << max_value << "]: " << redact_error_message(error.what());
    throw std::invalid_argument(range.str());
  }
}

inline double parse_positive_double_arg(
    const std::string& name,
    const std::string& value,
    const double max_value = std::numeric_limits<double>::max()) {
  const double parsed = parse_bounded_double_arg(name, value, 0.0, max_value);
  if (parsed <= 0.0) {
    throw std::invalid_argument(name + " must be a positive finite number");
  }
  return parsed;
}

inline double parse_non_negative_double_arg(
    const std::string& name,
    const std::string& value,
    const double max_value = std::numeric_limits<double>::max()) {
  return parse_bounded_double_arg(name, value, 0.0, max_value);
}

inline double parse_finite_double_or_default(const std::string& value,
                                             const double fallback) {
  if (!std::isfinite(fallback)) {
    throw std::invalid_argument("finite-double fallback must be finite");
  }
  if (value.empty()) return fallback;
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(parsed)) {
      return fallback;
    }
    return parsed;
  } catch (...) {
    return fallback;
  }
}

inline std::size_t checked_element_count(
    const std::string& name,
    const int count,
    const std::size_t max_elements = 268435456ULL) {
  if (count < 0) {
    throw std::invalid_argument(name + " must not be negative");
  }
  const auto elements = static_cast<std::size_t>(count);
  if (elements > max_elements) {
    throw std::invalid_argument(name + " exceeds maximum element count");
  }
  return elements;
}

inline std::size_t checked_element_count(
    const std::string& name,
    const int rows,
    const int cols,
    const std::size_t max_elements = 268435456ULL) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(name + " dimensions must be positive");
  }
  const auto lhs = static_cast<std::size_t>(rows);
  const auto rhs = static_cast<std::size_t>(cols);
  if (lhs > max_elements / rhs) {
    throw std::invalid_argument(name + " dimensions exceed maximum element count");
  }
  return lhs * rhs;
}

inline std::string json_string(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const char ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '\\') {
      out << "\\\\";
    } else if (ch == '"') {
      out << "\\\"";
    } else if (ch == '\b') {
      out << "\\b";
    } else if (ch == '\f') {
      out << "\\f";
    } else if (ch == '\n') {
      out << "\\n";
    } else if (ch == '\r') {
      out << "\\r";
    } else if (ch == '\t') {
      out << "\\t";
    } else if (byte < 0x20) {
      out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
          << static_cast<int>(byte) << std::dec << std::setfill(' ');
    } else {
      out << ch;
    }
  }
  out << '"';
  return out.str();
}

inline std::string fixed6(const double value) {
  if (!std::isfinite(value)) {
    return "null";
  }
  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << value;
  return out.str();
}

enum class JsonStringArrayLayout {
  kPrettyIfNonEmpty,
  kPrettyAlways,
  kCompact,
};

inline std::string render_json_string_array(
    const std::vector<std::string>& values,
    const int indent,
    const JsonStringArrayLayout layout) {
  if (layout == JsonStringArrayLayout::kCompact) {
    std::ostringstream compact;
    compact << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
      compact << (i == 0 ? "" : ", ") << json_string(values[i]);
    }
    compact << "]";
    return compact.str();
  }

  const std::string pad = json_indent(indent);
  const std::string nested = json_indent(indent + 2);
  std::ostringstream out;
  out << "[";
  const bool multiline = !values.empty() ||
                         layout == JsonStringArrayLayout::kPrettyAlways;
  if (multiline) out << "\n";
  for (std::size_t i = 0; i < values.size(); ++i) {
    const bool is_last = i == values.size() - 1U;
    out << nested << json_string(values[i])
        << (is_last ? "\n" : ",\n");
  }
  if (multiline) out << pad;
  out << "]";
  return out.str();
}

inline std::string json_string_array(const std::vector<std::string>& values,
                                     const int indent) {
  return render_json_string_array(
      values, indent, JsonStringArrayLayout::kPrettyIfNonEmpty);
}

inline std::string json_string_array_multiline(
    const std::vector<std::string>& values,
    const int indent) {
  return render_json_string_array(values, indent,
                                  JsonStringArrayLayout::kPrettyAlways);
}

inline std::string json_string_array_compact(
    const std::vector<std::string>& values) {
  return render_json_string_array(values, 0,
                                  JsonStringArrayLayout::kCompact);
}

inline void write_or_print(const std::string& path, const std::string& content) {
  if (path.empty()) {
    std::cout << content;
    if (!std::cout) {
      throw std::runtime_error("failed to write artifact to stdout");
    }
    return;
  }

  const std::filesystem::path output_path(path);
  const std::filesystem::path parent = output_path.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      throw std::runtime_error("failed to create output directory for " +
                               redacted_path_label(path) + ": " +
                               redact_error_message(error.message(), {path}));
    }
  }

  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open output path: " +
                             redacted_path_label(path));
  }
  file << content;
  if (!file) {
    throw std::runtime_error("failed to write output path: " +
                             redacted_path_label(path));
  }
  file.flush();
  if (!file) {
    throw std::runtime_error("failed to flush output path: " +
                             redacted_path_label(path));
  }
  file.close();
  if (!file) {
    throw std::runtime_error("failed to close output path: " +
                             redacted_path_label(path));
  }
}

inline std::filesystem::path resolve_repo_relative_path(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& path) {
  if (path.empty() || path.is_absolute()) return path;
  return repo_root / path;
}

inline std::filesystem::path resolve_optional_repo_relative_path(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& path) {
  return path.empty() ? path : resolve_repo_relative_path(repo_root, path);
}

inline std::filesystem::path path_identity(
    const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::path identity =
      std::filesystem::weakly_canonical(path, error);
  if (!error) return identity.lexically_normal();
  error.clear();
  identity = std::filesystem::absolute(path, error);
  return (error ? path : identity).lexically_normal();
}

inline bool paths_alias(const std::filesystem::path& lhs,
                        const std::filesystem::path& rhs) {
  if (lhs.empty() || rhs.empty()) return false;
  std::error_code error;
  const bool equivalent = std::filesystem::equivalent(lhs, rhs, error);
  return (!error && equivalent) || path_identity(lhs) == path_identity(rhs);
}

inline std::string path_string_or_default(
    const std::filesystem::path& path,
    const std::string& fallback) {
  return path.empty() ? fallback : path.generic_string();
}

inline bool is_cli_end_of_options(const std::string& arg) {
  return arg == "--";
}

inline bool is_cli_option_token(const std::string& arg) {
  return arg.rfind("--", 0) == 0 && !is_cli_end_of_options(arg);
}

inline bool contains_cli_option(const std::vector<std::string>& options,
                                const std::string& arg) {
  return std::find(options.begin(), options.end(), arg) != options.end();
}

struct CliOptionSpec {
  std::vector<std::string> value_options;
  std::vector<std::string> flag_options;
  std::vector<std::string> option_like_value_options;
  bool allow_positionals = false;
};

inline void validate_cli_option_spec(const CliOptionSpec& spec) {
  std::vector<std::string> seen;
  const auto add_options = [&](const std::vector<std::string>& options,
                               const char* category) {
    for (const auto& option : options) {
      if (!is_cli_option_token(option) || option == "--help") {
        throw std::invalid_argument(std::string(category) +
                                    " contains invalid CLI option: " + option);
      }
      if (contains_cli_option(seen, option)) {
        throw std::invalid_argument("duplicate CLI option in spec: " + option);
      }
      seen.push_back(option);
    }
  };
  add_options(spec.value_options, "value_options");
  add_options(spec.flag_options, "flag_options");
  std::vector<std::string> option_like_seen;
  for (const auto& option : spec.option_like_value_options) {
    if (!contains_cli_option(spec.value_options, option)) {
      throw std::invalid_argument(
          "option-like value option is not a value option: " + option);
    }
    if (contains_cli_option(option_like_seen, option)) {
      throw std::invalid_argument(
          "duplicate option-like value option in spec: " + option);
    }
    option_like_seen.push_back(option);
  }
}

inline bool cli_help_requested(const int argc, char** argv,
                               const CliOptionSpec& spec) {
  validate_cli_option_spec(spec);
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (is_cli_end_of_options(arg)) break;
    if (arg == "--help" || arg == "-h") return true;
    if (!contains_cli_option(spec.value_options, arg) || i + 1 >= argc) {
      continue;
    }
    const bool allow_option_like_value =
        contains_cli_option(spec.option_like_value_options, arg);
    if (allow_option_like_value || !is_cli_option_token(argv[i + 1])) ++i;
  }
  return false;
}

inline const CliOptionSpec& repo_root_output_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--output"},
      {},
  };
  return spec;
}

inline const CliOptionSpec& repo_root_output_format_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--output", "--format"},
      {},
  };
  return spec;
}

inline const CliOptionSpec& repo_root_fixture_output_format_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--fixture-dir", "--output", "--format"},
      {},
  };
  return spec;
}

inline const CliOptionSpec&
repo_root_results_warehouse_output_format_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--results", "--warehouse", "--output", "--format"},
      {},
  };
  return spec;
}

inline const CliOptionSpec& repo_root_results_output_format_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--results", "--output", "--format"},
      {},
  };
  return spec;
}

inline const CliOptionSpec& repo_root_artifact_output_cli_spec() {
  static const CliOptionSpec spec{
      {"--repo-root", "--artifact", "--output"},
      {},
  };
  return spec;
}

inline const char* cli_program_name(const int argc, char** argv,
                                    const char* fallback) {
  if (argc > 0 && argv != nullptr && argv[0] != nullptr &&
      argv[0][0] != '\0') {
    return argv[0];
  }
  return fallback != nullptr && fallback[0] != '\0' ? fallback : "inferlab";
}

inline void print_cli_usage_impl(
    const char* program,
    const std::vector<std::string>& value_options,
    const std::vector<std::string>& flag_options,
    const bool allow_positionals,
    std::ostream& out) {
  out << "Usage: " << program;
  for (const auto& option : value_options) {
    out << " [" << option << " VALUE]";
  }
  for (const auto& option : flag_options) {
    out << " [" << option << "]";
  }
  if (allow_positionals) out << " [args...]";
  out << " [--help]\n";
}

inline void print_cli_usage(const char* program,
                            const CliOptionSpec& spec,
                            std::ostream& out = std::cerr) {
  validate_cli_option_spec(spec);
  print_cli_usage_impl(program, spec.value_options, spec.flag_options,
                       spec.allow_positionals, out);
}

inline bool handle_cli_help(const int argc,
                            char** argv,
                            const char* program,
                            const CliOptionSpec& spec) {
  if (!cli_help_requested(argc, argv, spec)) return false;
  print_cli_usage(program, spec);
  return true;
}

inline std::string arg_value(const int argc, char** argv,
                             const std::string& name,
                             const std::string& fallback) {
  for (int i = 1; i < argc; ++i) {
    if (is_cli_end_of_options(argv[i])) break;
    if (argv[i] != name) continue;
    if (i + 1 >= argc) {
      throw std::invalid_argument("missing value for option: " + name);
    }
    const std::string value = argv[i + 1];
    if (is_cli_option_token(value)) {
      throw std::invalid_argument("missing value for option: " + name);
    }
    return value;
  }
  return fallback;
}

inline std::string arg_value_with_spec(const int argc, char** argv,
                                       const std::string& name,
                                       const std::string& fallback,
                                       const CliOptionSpec& spec) {
  validate_cli_option_spec(spec);
  if (!contains_cli_option(spec.value_options, name)) {
    throw std::invalid_argument("requested option is not a value option: " +
                                name);
  }
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (is_cli_end_of_options(arg)) break;
    if (!is_cli_option_token(arg)) continue;
    if (contains_cli_option(spec.value_options, arg)) {
      const bool allow_option_like_value =
          contains_cli_option(spec.option_like_value_options, arg);
      if (i + 1 >= argc ||
          (!allow_option_like_value && is_cli_option_token(argv[i + 1]))) {
        throw std::invalid_argument("missing value for option: " + arg);
      }
      const std::string value = argv[i + 1];
      if (arg == name) return value;
      ++i;
      continue;
    }
    if (contains_cli_option(spec.flag_options, arg) || arg == "--help") {
      continue;
    }
    throw std::invalid_argument("unknown option: " + arg);
  }
  return fallback;
}

inline std::vector<std::string> arg_values(const int argc,
                                           char** argv,
                                           const std::string& name) {
  std::vector<std::string> values;
  for (int i = 1; i < argc; ++i) {
    if (is_cli_end_of_options(argv[i])) break;
    if (argv[i] != name) continue;
    if (i + 1 >= argc) {
      throw std::invalid_argument("missing value for option: " + name);
    }
    const std::string value = argv[i + 1];
    if (is_cli_option_token(value)) {
      throw std::invalid_argument("missing value for option: " + name);
    }
    values.push_back(value);
    ++i;
  }
  return values;
}

inline bool has_flag(const int argc, char** argv, const std::string& name) {
  for (int i = 1; i < argc; ++i) {
    if (is_cli_end_of_options(argv[i])) break;
    if (argv[i] == name) return true;
  }
  return false;
}

inline std::vector<std::string> positional_args_impl(
    const int argc,
    char** argv,
    const std::vector<std::string>& value_options,
    const std::vector<std::string>& flag_options,
    const std::vector<std::string>& option_like_value_options) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (is_cli_end_of_options(arg)) {
      for (++i; i < argc; ++i) args.push_back(argv[i]);
      break;
    }
    if (is_cli_option_token(arg)) {
      if (contains_cli_option(value_options, arg)) {
        const bool allow_option_like_value =
            contains_cli_option(option_like_value_options, arg);
        if (i + 1 >= argc ||
            (!allow_option_like_value && is_cli_option_token(argv[i + 1]))) {
          throw std::invalid_argument("missing value for option: " + arg);
        }
        ++i;
        continue;
      }
      if (contains_cli_option(flag_options, arg)) continue;
      throw std::invalid_argument("unknown option: " + arg);
    }
    args.push_back(arg);
  }
  return args;
}

inline std::vector<std::string> positional_args(const int argc,
                                                char** argv,
                                                const CliOptionSpec& spec) {
  validate_cli_option_spec(spec);
  return positional_args_impl(argc, argv, spec.value_options,
                              spec.flag_options,
                              spec.option_like_value_options);
}

inline void validate_cli_args_impl(
    const int argc,
    char** argv,
    const std::vector<std::string>& value_options,
    const std::vector<std::string>& flag_options,
    const std::vector<std::string>& option_like_value_options,
    const bool allow_positionals = false) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") continue;
    if (is_cli_end_of_options(arg)) {
      if (!allow_positionals && i + 1 < argc) {
        throw std::invalid_argument("unexpected positional argument after --: " +
                                    std::string(argv[i + 1]));
      }
      break;
    }
    if (!is_cli_option_token(arg)) {
      if (!allow_positionals) {
        throw std::invalid_argument("unexpected positional argument: " + arg);
      }
      continue;
    }
    if (contains_cli_option(value_options, arg)) {
      const bool allow_option_like_value =
          contains_cli_option(option_like_value_options, arg);
      if (i + 1 >= argc ||
          (!allow_option_like_value && is_cli_option_token(argv[i + 1]))) {
        throw std::invalid_argument("missing value for option: " + arg);
      }
      ++i;
      continue;
    }
    if (contains_cli_option(flag_options, arg)) continue;
    throw std::invalid_argument("unknown option: " + arg);
  }
}

inline void validate_cli_args(const int argc,
                              char** argv,
                              const CliOptionSpec& spec) {
  validate_cli_option_spec(spec);
  validate_cli_args_impl(
      argc, argv, spec.value_options, spec.flag_options,
      spec.option_like_value_options, spec.allow_positionals);
}

inline std::string source_url_for_product(const std::string& product) {
  if (product.find("vLLM") != std::string::npos) return "https://docs.vllm.ai/";
  if (product.find("SGLang") != std::string::npos) return "https://github.com/sgl-project/sglang";
  if (product.find("TensorRT") != std::string::npos) return "https://nvidia.github.io/TensorRT-LLM/";
  if (product.find("Triton") != std::string::npos) return "https://docs.nvidia.com/deeplearning/triton-inference-server/user-guide/docs/";
  if (product.find("FlashInfer") != std::string::npos) return "https://docs.flashinfer.ai/";
  if (product.find("LMCache") != std::string::npos) return "https://docs.lmcache.ai/";
  if (product.find("Mooncake") != std::string::npos) return "https://kvcache-ai.github.io/Mooncake/";
  if (product.find("Dynamo") != std::string::npos) return "https://docs.dynamo.nvidia.com/";
  if (product.find("KServe") != std::string::npos) return "https://kserve.github.io/website/";
  if (product.find("llm-d") != std::string::npos) return "https://github.com/llm-d/llm-d";
  if (product.find("Ray Data") != std::string::npos) {
    return "https://docs.ray.io/en/latest/data/data.html";
  }
  if (product.find("Ray") != std::string::npos) return "https://docs.ray.io/en/latest/serve/";
  if (product.find("TGI") != std::string::npos) return "https://github.com/huggingface/text-generation-inference";
  if (product.find("KTransformers") != std::string::npos) return "https://github.com/kvcache-ai/ktransformers";
  if (product.find("AIBrix") != std::string::npos) return "https://github.com/vllm-project/aibrix";
  if (product.find("Punica") != std::string::npos) return "https://github.com/punica-ai/punica";
  if (product.find("S-LoRA") != std::string::npos) return "https://github.com/S-LoRA/S-LoRA";
  if (product.find("XGrammar") != std::string::npos) return "https://github.com/mlc-ai/xgrammar";
  if (product.find("llguidance") != std::string::npos) return "https://github.com/guidance-ai/llguidance";
  if (product.find("Outlines") != std::string::npos) return "https://github.com/dottxt-ai/outlines";
  if (product.find("MLC") != std::string::npos ||
      product.find("WebLLM") != std::string::npos) {
    return "https://github.com/mlc-ai/mlc-llm";
  }
  if (product.find("LMDeploy") != std::string::npos) return "https://github.com/InternLM/lmdeploy";
  if (product.find("llama.cpp") != std::string::npos) return "https://github.com/ggml-org/llama.cpp";
  if (product.find("Llumnix") != std::string::npos) return "https://github.com/AlibabaPAI/llumnix";
  if (product.find("Sarathi") != std::string::npos) return "https://www.usenix.org/conference/osdi24/presentation/agrawal";
  if (product.find("CUTLASS") != std::string::npos) return "https://github.com/NVIDIA/cutlass";
  if (product.find("Marlin") != std::string::npos) return "https://github.com/IST-DASLab/marlin";
  if (product.find("Android") != std::string::npos) return "https://developer.android.com/ndk";
  return "";
}

inline std::string official_sources_json(const std::vector<std::string>& products,
                                         const int indent) {
  const std::string pad = json_indent(indent);
  const std::string nested = json_indent(indent + 2);
  std::ostringstream out;
  out << "[";
  if (!products.empty()) out << "\n";
  for (std::size_t i = 0; i < products.size(); ++i) {
    const std::string url = source_url_for_product(products[i]);
    const bool is_last = i == products.size() - 1U;
    out << nested << "{\n";
    out << nested << "  \"name\": " << json_string(products[i]) << ",\n";
    out << nested << "  \"url\": " << (url.empty() ? "null" : json_string(url)) << ",\n";
    out << nested << "  \"accessed_at\": "
        << json_string(current_utc_date()) << ",\n";
    out << nested << "  \"purpose\": \"mature product capability alignment only; no product execution in current environment\"\n";
    out << nested << "}" << (is_last ? "\n" : ",\n");
  }
  if (!products.empty()) out << pad;
  out << "]";
  return out.str();
}

inline std::string common_claim_scope_detail_json(const std::string& claim_scope,
                                                  const int indent) {
  const std::string pad = json_indent(indent);
  std::ostringstream out;
  out << "{\n";
  out << pad << "  \"can_claim\": [\n";
  out << pad << "    \"C++17 native mechanism slice/schema artifact is implemented\",\n";
  out << pad << "    \"artifact is registered for benchmark-matrix refresh on a target machine\"\n";
  out << pad << "  ],\n";
  out << pad << "  \"cannot_claim\": [\n";
  out << pad << "    " << json_string(claim_scope) << ",\n";
  out << pad << "    \"does not reimplement mature product core runtime, scheduler, kernel, storage engine, gateway, or deployment control plane\",\n";
  out << pad << "    \"does not provide measured throughput, latency, CUDA timing, or production SLO evidence on this server\"\n";
  out << pad << "  ],\n";
  out << pad << "  \"requires_execute_before_claim\": [\n";
  out << pad << "    \"CMake build and native binary execution on the target machine\",\n";
  out << pad << "    \"product replay or execute artifact when making product behavior claims\",\n";
  out << pad << "    \"summary, Prometheus, final report, claim, freshness, and release gates after artifacts are refreshed\"\n";
  out << pad << "  ]\n";
  out << pad << "}";
  return out.str();
}

inline std::string default_not_run_json(const int indent) {
  return json_string_array(
      {"tests", "builds", "benchmarks", "native binaries", "product scripts",
       "product replay", "CUDA correctness/timing", "Kubernetes/docker commands",
       "endpoint probes", "product installs", "model downloads",
       "final report rendering", "static validation"},
      indent);
}

inline void write_common_native_probe_contract(
    std::ostream& out,
    const std::string& artifact_type,
    const std::string& claim_scope) {
  if (artifact_type.empty() || claim_scope.empty()) {
    throw std::invalid_argument(
        "native probe artifact type and claim scope must not be empty");
  }
  out << "  \"artifact_type\": " << json_string(artifact_type) << ",\n";
  out << "  \"mode\": \"execute\",\n";
  out << "  \"executed\": true,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": " << json_string(claim_scope) << ",\n";
}

inline void write_common_dry_run_metadata_impl(
    std::ostream& out,
    const std::string& artifact_name,
    const std::string& claim_scope,
    const std::vector<std::string>& product_alignment,
    const std::string& target_name,
    const std::string& artifact_path,
    const std::string& command_override,
    const bool include_mode_and_not_run) {
  const std::string command =
      command_override.empty()
          ? "./build/" + target_name + " --output " + artifact_path
          : command_override;
  out << "  \"schema\": {\n";
  out << "    \"name\": " << json_string(artifact_name) << ",\n";
  out << "    \"version\": \"v1\"\n";
  out << "  },\n";
  if (include_mode_and_not_run) out << "  \"mode\": \"dry_run\",\n";
  out << "  \"generated_at\": " << json_string(current_utc_timestamp()) << ",\n";
  out << "  \"claim_scope_detail\": "
      << common_claim_scope_detail_json(claim_scope, 2) << ",\n";
  out << "  \"environment\": {\n";
  out << "    \"current_host_has_gpu\": false,\n";
  out << "    \"requires_target_gpu_validation\": true,\n";
  out << "    \"requires_product_environment_for_execute\": true\n";
  out << "  },\n";
  out << "  \"official_sources\": "
      << official_sources_json(product_alignment, 2) << ",\n";
  out << "  \"refresh_commands\": " << json_string_array({command}, 2) << ",\n";
  out << "  \"migration_commands\": " << json_string_array({command}, 2) << ",\n";
  if (include_mode_and_not_run) {
    out << "  \"not_run\": " << default_not_run_json(2) << ",\n";
  }
}

inline std::string catalog_evidence_contract_json(
    const std::string& evidence_confidence,
    const std::size_t blocker_count,
    const std::string& claim_boundary,
    const std::string& refresh_command,
    const int indent) {
  if (!has_non_whitespace(evidence_confidence)) {
    throw std::invalid_argument(
        "catalog evidence confidence must not be empty");
  }
  const std::string pad = json_indent(indent);
  const std::string nested = json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"evidence_confidence\": "
      << json_string(evidence_confidence) << ",\n";
  out << nested << "\"blocker_count\": " << blocker_count << ",\n";
  out << nested << "\"catalog_registry_coverage\": 1.000000,\n";
  out << nested << "\"claim_boundary_present\": "
      << json_bool(has_non_whitespace(claim_boundary)) << ",\n";
  out << nested << "\"refresh_command_present\": "
      << json_bool(has_non_whitespace(refresh_command)) << "\n";
  out << pad << "}";
  return out.str();
}

inline void write_common_dry_run_context_metadata(
    std::ostream& out,
    const std::string& artifact_name,
    const std::string& claim_scope,
    const std::vector<std::string>& product_alignment,
    const std::string& target_name,
    const std::string& artifact_path,
    const std::string& command_override = "") {
  write_common_dry_run_metadata_impl(
      out, artifact_name, claim_scope, product_alignment, target_name,
      artifact_path, command_override, false);
}

inline void write_common_dry_run_metadata(
    std::ostream& out,
    const std::string& artifact_name,
    const std::string& claim_scope,
    const std::vector<std::string>& product_alignment,
    const std::string& target_name,
    const std::string& artifact_path,
    const std::string& command_override = "") {
  write_common_dry_run_metadata_impl(
      out, artifact_name, claim_scope, product_alignment, target_name,
      artifact_path, command_override, true);
}

}  // namespace inferlab_plan
