#pragma once

#include "plan_json_utils.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace inferlab_evidence_query_cli {

inline constexpr char kCanonicalTarget[] = "inferlab_evidence_query";
inline constexpr char kSelectorRepairCardCompatibilityTarget[] =
    "inferlab_evidence_query_selector_repair_card_export";
inline constexpr char kSelectorRepairCardOperation[] = "explain_blocker";
inline constexpr char kSelectorRepairCardSelector[] =
    "report_wiring_missing";
inline constexpr char kQueryResultArtifact[] =
    "results/raw/native_evidence_query_result.json";
inline constexpr char kSelectorRepairCardArtifact[] =
    "results/raw/native_evidence_query_selector_repair_card_export.json";
inline constexpr char kDefaultWarehouseArtifact[] =
    "results/raw/native_evidence_warehouse_index.json";
inline constexpr char kDefaultResultsDirectory[] = "results/raw";

static_assert(sizeof(kCanonicalTarget) > 1 &&
                  sizeof(kSelectorRepairCardCompatibilityTarget) > 1 &&
                  sizeof(kSelectorRepairCardOperation) > 1 &&
                  sizeof(kSelectorRepairCardSelector) > 1,
              "Evidence Query CLI contract tokens must not be empty");

inline const inferlab_plan::CliOptionSpec& option_spec() {
  static const inferlab_plan::CliOptionSpec spec{
      {"--repo-root", "--warehouse", "--results", "--fixture-dir",
       "--query", "--operation", "--selector", "--target", "--artifact",
       "--blocker", "--before", "--after", "--format", "--output",
       "--selector-repair-card-output"},
      {},
      {"--blocker"},
      true,
  };
  return spec;
}

inline bool is_selector_repair_card_compatibility_invocation(
    const char* program) {
  const std::string_view invocation = program == nullptr ? "" : program;
  const std::size_t separator = invocation.find_last_of("/\\");
  const std::string_view executable =
      separator == std::string_view::npos ? invocation
                                          : invocation.substr(separator + 1);
  return executable == kSelectorRepairCardCompatibilityTarget;
}

inline void apply_selector_repair_card_compatibility_contract(
    const bool compatibility_invocation,
    std::string& operation,
    std::string& selector) {
  if (!compatibility_invocation) return;
  if (!operation.empty() && operation != kSelectorRepairCardOperation) {
    throw std::invalid_argument(
        "selector repair-card compatibility target only supports operation " +
        std::string(kSelectorRepairCardOperation));
  }
  if (!selector.empty() && selector != kSelectorRepairCardSelector) {
    throw std::invalid_argument(
        "selector repair-card compatibility target only supports selector " +
        std::string(kSelectorRepairCardSelector));
  }
  operation = kSelectorRepairCardOperation;
  selector = kSelectorRepairCardSelector;
}

inline const char* primary_output_default(
    const bool /*compatibility_invocation*/) {
  return "";
}

inline const char* selector_repair_card_output_default(
    const bool compatibility_invocation) {
  return compatibility_invocation ? kSelectorRepairCardArtifact : "";
}

inline std::string selector_repair_card_export_command() {
  return "./build/" + std::string(kCanonicalTarget) +
         " --repo-root . --warehouse " + kDefaultWarehouseArtifact +
         " --operation " +
         kSelectorRepairCardOperation + " --selector " +
         kSelectorRepairCardSelector + " --selector-repair-card-output " +
         kSelectorRepairCardArtifact +
         " --format json";
}

}  // namespace inferlab_evidence_query_cli
