#include "paged_kv_locality_probe_view.hpp"

#include "paged_kv_probe_json.hpp"
#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::paged_kv_locality_probe {
namespace {

std::string metrics_json(const BlockTableLocalityMetrics& metrics,
                         const std::vector<BlockId>& block_table,
                         const int indent) {
  const auto pad = probe::probe_json_indent(indent);
  const auto nested = probe::probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"average_physical_jump\": " << std::fixed
      << std::setprecision(6) << metrics.average_physical_jump << ",\n";
  out << nested << "\"backward_transitions\": "
      << metrics.backward_transitions << ",\n";
  out << nested << "\"block_element_amplification\": " << std::fixed
      << std::setprecision(6) << metrics.block_element_amplification
      << ",\n";
  out << nested << "\"block_table\": "
      << probe::block_table_json(block_table) << ",\n";
  out << nested << "\"contiguous_forward_transitions\": "
      << metrics.contiguous_forward_transitions << ",\n";
  out << nested << "\"logical_blocks\": " << metrics.logical_blocks
      << ",\n";
  out << nested << "\"logical_element_reads\": "
      << metrics.logical_element_reads << ",\n";
  out << nested << "\"max_physical_jump\": "
      << metrics.max_physical_jump << ",\n";
  out << nested << "\"non_contiguous_transitions\": "
      << metrics.non_contiguous_transitions << ",\n";
  out << nested << "\"physical_span_blocks\": "
      << metrics.physical_span_blocks << ",\n";
  out << nested << "\"physical_span_elements\": "
      << metrics.physical_span_elements << ",\n";
  out << nested << "\"span_element_amplification\": " << std::fixed
      << std::setprecision(6) << metrics.span_element_amplification
      << ",\n";
  out << nested << "\"token_count\": " << metrics.token_count << ",\n";
  out << nested << "\"touched_block_elements\": "
      << metrics.touched_block_elements << ",\n";
  out << nested << "\"transitions\": " << metrics.transitions << ",\n";
  out << nested << "\"unique_physical_blocks\": "
      << metrics.unique_physical_blocks << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_paged_kv_locality_probe",
      "Executed deterministic scalar CPU Paged KV block-table locality "
      "analysis over synthetic block ids only; not measured memory traffic, "
      "GPU cache locality, latency, throughput, or benchmark evidence.");
  out << "  \"config\": {\n";
  out << "    \"block_size_tokens\": "
      << plan.request.shape.block_size_tokens << ",\n";
  out << "    \"head_dim\": " << plan.request.shape.head_dim << ",\n";
  out << "    \"num_heads\": " << plan.request.shape.num_heads << ",\n";
  out << "    \"token_count\": " << plan.request.token_count << "\n";
  out << "  },\n";
  out << "  \"fixture_limits\": {\n";
  out << "    \"max_paged_kv_probe_blocks\": "
      << probe::kMaxPagedKVProbeBlocks << "\n";
  out << "  },\n";
  out << "  \"locality\": {\n";
  out << "    \"contiguous\": "
      << metrics_json(
             plan.contiguous_metrics, plan.contiguous.block_table, 4)
      << ",\n";
  out << "    \"fragmented\": "
      << metrics_json(
             plan.fragmented_metrics, plan.fragmented.block_table, 4)
      << ",\n";
  out << "    \"fragmented_vs_contiguous_span_amplification_delta\": "
      << std::fixed << std::setprecision(6)
      << plan.fragmented_vs_contiguous_span_amplification_delta << "\n";
  out << "  },\n";
  out << "  \"reference_simulation_only\": true\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::paged_kv_locality_probe
