#include "speculative_decode_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::speculative_decode_plan {
namespace {

std::string scenario_json(const SpeculativeDecodeScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "speculative decode scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"acceptance_rate\": " << std::fixed
      << std::setprecision(6) << scenario.acceptance_rate << ",\n";
  out << nested << "\"draft_tokens_per_second\": " << std::fixed
      << std::setprecision(6) << scenario.draft_tokens_per_second
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"output_tokens\": " << scenario.output_tokens
      << ",\n";
  out << nested << "\"round_overhead_ms\": " << std::fixed
      << std::setprecision(6) << scenario.round_overhead_ms << ",\n";
  out << nested << "\"target_tokens_per_second\": " << std::fixed
      << std::setprecision(6) << scenario.target_tokens_per_second
      << ",\n";
  out << nested << "\"target_verify_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.target_verify_ms_per_token
      << ",\n";
  out << nested << "\"verify_window\": " << scenario.verify_window
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const SpeculativeDecodeResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "speculative decode record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"accepted_tokens\": " << result.accepted_tokens
      << ",\n";
  out << nested << "\"break_even_acceptance_rate\": " << std::fixed
      << std::setprecision(6) << result.break_even_acceptance_rate
      << ",\n";
  out << nested << "\"draft_tokens\": " << result.draft_tokens
      << ",\n";
  out << nested << "\"rejected_tokens\": " << result.rejected_tokens
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"speculative_ms\": " << std::fixed
      << std::setprecision(6) << result.speculative_ms << ",\n";
  out << nested << "\"speedup\": " << std::fixed
      << std::setprecision(6) << result.speedup << ",\n";
  out << nested << "\"target_only_ms\": " << std::fixed
      << std::setprecision(6) << result.target_only_ms << ",\n";
  out << nested << "\"target_verify_rounds\": "
      << result.target_verify_rounds << ",\n";
  out << nested << "\"tokens_per_second\": " << std::fixed
      << std::setprecision(6) << result.tokens_per_second << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& best_speedup =
      plan.records[plan.summary.best_speedup_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ native planning/estimation artifact only; does not run benchmarks, product backends, CUDA kernels, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_speedup\": " << std::fixed
      << std::setprecision(6) << best_speedup.speedup << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    if (index != 0U) {
      out << ",\n";
    }
    out << record_json(plan.records[index], 4);
  }
  out << "\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::speculative_decode_plan
