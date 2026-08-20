#include "claim_scope_contract_matrix.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace inferlab_claim_scope {
namespace {

constexpr std::size_t kMaxMatrixArtifacts = 16384U;
constexpr std::size_t kMaxMatrixIssues = 16384U;
constexpr std::size_t kMaxMatrixLines = 1048576U;

std::string trim(std::string value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
      });
  const auto last =
      std::find_if_not(value.rbegin(), value.rend(),
                       [](const unsigned char ch) {
                         return std::isspace(ch) != 0;
                       })
          .base();
  if (first >= last) return {};
  return std::string(first, last);
}

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}

void append_issue(ClaimScopeMatrixInventory& inventory,
                  const std::string& issue) {
  if (inventory.issues.size() >= kMaxMatrixIssues) {
    throw std::invalid_argument(
        "benchmark matrix exceeds the claim-scope issue limit");
  }
  inventory.issues.push_back(issue);
}

bool normalize_yaml_scalar(std::string& value) {
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool escaped = false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (in_double_quote && escaped) {
      escaped = false;
      continue;
    }
    if (in_double_quote && ch == '\\') {
      escaped = true;
      continue;
    }
    if (!in_double_quote && ch == '\'') {
      in_single_quote = !in_single_quote;
      continue;
    }
    if (!in_single_quote && ch == '"') {
      in_double_quote = !in_double_quote;
      continue;
    }
    if (!in_single_quote && !in_double_quote && ch == '#') {
      value.resize(index);
      break;
    }
  }
  if (in_single_quote || in_double_quote || escaped) return false;

  value = trim(std::move(value));
  if (value.size() >= 2U &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1U, value.size() - 2U);
  } else if (!value.empty() &&
             (value.front() == '"' || value.front() == '\'' ||
              value.back() == '"' || value.back() == '\'')) {
    return false;
  }
  value = trim(std::move(value));
  return !value.empty();
}

bool is_safe_flat_json_artifact(const std::string& artifact) {
  constexpr char kJsonSuffix[] = ".json";
  constexpr std::size_t kJsonSuffixSize = sizeof(kJsonSuffix) - 1U;
  if (artifact.size() <= kJsonSuffixSize || artifact == "." ||
      artifact == ".." || artifact.find('/') != std::string::npos ||
      artifact.find('\\') != std::string::npos ||
      artifact.compare(artifact.size() - kJsonSuffixSize, kJsonSuffixSize,
                       kJsonSuffix) != 0) {
    return false;
  }
  return std::none_of(
      artifact.begin(), artifact.end(), [](const unsigned char ch) {
        return std::iscntrl(ch) != 0 || std::isspace(ch) != 0;
      });
}

bool is_safe_matrix_identity(const std::string& value) {
  return !value.empty() && value.front() != '/' &&
         value.find("..") == std::string::npos &&
         value.find('\\') == std::string::npos &&
         std::none_of(value.begin(), value.end(),
                      [](const unsigned char ch) {
                        return std::iscntrl(ch) != 0 ||
                               std::isspace(ch) != 0;
                      });
}

bool parse_scalar_field(const std::string& stripped,
                        const std::string& prefix,
                        std::string& value) {
  if (!starts_with(stripped, prefix)) return false;
  value = trim(stripped.substr(prefix.size()));
  return normalize_yaml_scalar(value);
}

}  // namespace

ClaimScopeMatrixInventory parse_claim_scope_matrix(const std::string& text) {
  ClaimScopeMatrixInventory inventory;
  std::set<std::string> artifacts;
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0U;
  std::string entry_name;
  std::string producer_target;
  std::string producer_source;
  int producer_priority = 0;
  while (std::getline(input, line)) {
    line_number = inferlab::internal::checked_add_size(
        line_number, 1U, "claim scope matrix line count");
    if (line_number > kMaxMatrixLines) {
      throw std::invalid_argument(
          "benchmark matrix exceeds the claim-scope line limit");
    }

    const std::string stripped = trim(line);
    std::string scalar;
    if (starts_with(stripped, "- name:")) {
      entry_name.clear();
      producer_target.clear();
      producer_source.clear();
      producer_priority = 0;
      if (!parse_scalar_field(stripped, "- name:", scalar) ||
          !is_safe_matrix_identity(scalar)) {
        append_issue(inventory, "invalid_entry_name_at_line_" +
                                    std::to_string(line_number));
        continue;
      }
      entry_name = std::move(scalar);
      continue;
    }
    const auto bind_producer = [&](const std::string& field,
                                   std::string value,
                                   const int priority) {
      if (priority <= producer_priority) return true;
      if (!is_safe_matrix_identity(value)) return false;
      if (field == "binary" && starts_with(value, "build/")) {
        value.erase(0U, sizeof("build/") - 1U);
      } else if (field == "module") {
        value = "python_module:" + value;
      }
      if (!is_safe_matrix_identity(value)) return false;
      producer_target = std::move(value);
      producer_source = field;
      producer_priority = priority;
      return true;
    };
    if (starts_with(stripped, "binary:")) {
      if (!parse_scalar_field(stripped, "binary:", scalar)) {
        producer_target.clear();
        producer_source.clear();
        producer_priority = 0;
        append_issue(inventory, "invalid_binary_value_at_line_" +
                                    std::to_string(line_number));
        continue;
      }
      if (!bind_producer("binary", std::move(scalar), 4)) {
        append_issue(inventory, "unsafe_binary_value_at_line_" +
                                    std::to_string(line_number));
      }
      continue;
    }
    if (starts_with(stripped, "script:")) {
      if (!parse_scalar_field(stripped, "script:", scalar)) {
        producer_target.clear();
        producer_source.clear();
        producer_priority = 0;
        append_issue(inventory, "invalid_script_value_at_line_" +
                                    std::to_string(line_number));
        continue;
      }
      if (!bind_producer("script", std::move(scalar), 3)) {
        append_issue(inventory, "unsafe_script_value_at_line_" +
                                    std::to_string(line_number));
      }
      continue;
    }
    if (starts_with(stripped, "module:")) {
      if (!parse_scalar_field(stripped, "module:", scalar)) {
        producer_target.clear();
        producer_source.clear();
        producer_priority = 0;
        append_issue(inventory, "invalid_module_value_at_line_" +
                                    std::to_string(line_number));
        continue;
      }
      if (!bind_producer("module", std::move(scalar), 2)) {
        append_issue(inventory, "unsafe_module_value_at_line_" +
                                    std::to_string(line_number));
      }
      continue;
    }
    if (starts_with(stripped, "coverage_source:")) {
      if (!parse_scalar_field(stripped, "coverage_source:", scalar)) {
        producer_target.clear();
        producer_source.clear();
        producer_priority = 0;
        append_issue(inventory,
                     "invalid_coverage_source_value_at_line_" +
                         std::to_string(line_number));
        continue;
      }
      if (!bind_producer("coverage_source", std::move(scalar), 1)) {
        append_issue(inventory,
                     "unsafe_coverage_source_value_at_line_" +
                         std::to_string(line_number));
      }
      continue;
    }
    if (!starts_with(stripped, "output:")) continue;
    std::string value = trim(stripped.substr(sizeof("output:") - 1U));
    if (!normalize_yaml_scalar(value)) {
      append_issue(inventory, "invalid_output_value_at_line_" +
                                  std::to_string(line_number));
      continue;
    }

    constexpr char kResultsPrefix[] = "results/raw/";
    constexpr std::size_t kResultsPrefixSize = sizeof(kResultsPrefix) - 1U;
    if (!starts_with(value, kResultsPrefix)) {
      inventory.ignored_output_count = inferlab::internal::checked_add_size(
          inventory.ignored_output_count, 1U,
          "claim scope ignored matrix output count");
      continue;
    }

    const std::string artifact = value.substr(kResultsPrefixSize);
    if (!is_safe_flat_json_artifact(artifact)) {
      append_issue(inventory, "unsafe_results_raw_output_at_line_" +
                                  std::to_string(line_number));
      continue;
    }
    if (!artifacts.insert(artifact).second) {
      inventory.duplicate_output_count =
          inferlab::internal::checked_add_size(
              inventory.duplicate_output_count, 1U,
              "claim scope duplicate matrix output count");
      append_issue(inventory, "duplicate_results_raw_output_at_line_" +
                                  std::to_string(line_number));
      continue;
    }
    if (artifacts.size() > kMaxMatrixArtifacts) {
      throw std::invalid_argument(
          "benchmark matrix exceeds the claim-scope artifact limit");
    }
    ClaimScopeMatrixProducer producer;
    producer.artifact = artifact;
    producer.entry_name = entry_name;
    producer.producer_target = producer_target;
    producer.producer_source = producer_source;
    if (producer.producer_target.empty()) {
      if (!is_safe_matrix_identity(producer.entry_name)) {
        append_issue(inventory,
                     "missing_producer_identity_at_line_" +
                         std::to_string(line_number));
        continue;
      }
      producer.producer_target =
          "benchmark_matrix_entry:" + producer.entry_name;
      producer.producer_source = "entry_name_fallback";
    }
    inventory.producers.push_back(std::move(producer));
  }
  inventory.artifacts.assign(artifacts.begin(), artifacts.end());
  std::sort(inventory.producers.begin(), inventory.producers.end(),
            [](const ClaimScopeMatrixProducer& lhs,
               const ClaimScopeMatrixProducer& rhs) {
              return lhs.artifact < rhs.artifact;
            });
  return inventory;
}

}  // namespace inferlab_claim_scope
