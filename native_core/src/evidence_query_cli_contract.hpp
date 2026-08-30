#pragma once

#include "plan_json_utils.hpp"

#include <string>

namespace inferlab_evidence_query_cli {

inline constexpr char kCanonicalTarget[] = "inferlab_evidence_query";
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
