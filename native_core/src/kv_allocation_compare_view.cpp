#include "kv_allocation_compare_view.hpp"

#include "paged_kv_probe_json.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::kv_allocation_compare {
namespace {

std::string static_stats_json(const StaticAllocationStats& stats,
                              const int indent) {
  const auto pad = probe::probe_json_indent(indent);
  const auto nested = probe::probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"active_requests\": " << stats.active_requests << ",\n";
  out << nested << "\"max_requests\": " << stats.max_requests << ",\n";
  out << nested << "\"reserved_bytes\": " << stats.reserved_bytes << ",\n";
  out << nested << "\"total_tokens\": " << stats.total_tokens << ",\n";
  out << nested << "\"used_bytes\": " << stats.used_bytes << ",\n";
  out << nested << "\"utilization\": " << std::fixed
      << std::setprecision(6) << stats.utilization << "\n";
  out << pad << "}";
  return out.str();
}

std::string workload_json(const std::vector<WorkloadRow>& workload) {
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0U; index < workload.size(); ++index) {
    out << "    {\n";
    out << "      \"request_id\": " << workload[index].request_id << ",\n";
    out << "      \"tokens\": " << workload[index].tokens << "\n";
    out << "    }" << (index + 1U == workload.size() ? "\n" : ",\n");
  }
  out << "  ]";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_kv_allocation_compare",
      "Executed deterministic scalar CPU static-versus-paged KV allocation "
      "accounting over a synthetic workload only; not product serving, GPU "
      "memory, latency, throughput, fragmentation, or benchmark evidence.");
  out << "  \"config\": {\n";
  out << "    \"block_size_tokens\": "
      << plan.config.block_size_tokens << ",\n";
  out << "    \"bytes_per_token\": "
      << plan.config.bytes_per_token << ",\n";
  out << "    \"max_requests\": " << plan.config.max_requests << ",\n";
  out << "    \"max_seq_len\": "
      << plan.config.max_sequence_length << ",\n";
  out << "    \"paged_total_blocks\": "
      << plan.paged_total_blocks << "\n";
  out << "  },\n";
  out << "  \"fixture_limits\": {\n";
  out << "    \"max_paged_kv_probe_blocks\": "
      << probe::kMaxPagedKVProbeBlocks << "\n";
  out << "  },\n";
  out << "  \"paged\": "
      << probe::allocation_stats_json(plan.paged_stats, 2) << ",\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"savings\": {\n";
  out << "    \"reserved_bytes_delta\": "
      << plan.reserved_bytes_delta << ",\n";
  out << "    \"reserved_bytes_ratio\": " << std::fixed
      << std::setprecision(6) << plan.reserved_bytes_ratio << "\n";
  out << "  },\n";
  out << "  \"static\": "
      << static_stats_json(plan.static_stats, 2) << ",\n";
  out << "  \"workload\": " << workload_json(plan.workload) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kv_allocation_compare
