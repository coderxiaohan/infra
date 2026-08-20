#include "prefix_cache_compare_view.hpp"

#include "paged_kv_probe_json.hpp"
#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::prefix_cache_compare {

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& profile = plan.traffic_profile;
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_prefix_cache_compare",
      "Executed deterministic scalar CPU Paged KV prefix-sharing accounting "
      "over a synthetic workload only; not product prefix-cache hit rate, "
      "latency, throughput, GPU traffic, or benchmark evidence.");
  out << "  \"config\": "
      << probe::paged_kv_cache_config_json(plan.cache_config, 2) << ",\n";
  out << "  \"fixture_limits\": {\n";
  out << "    \"max_paged_kv_probe_blocks\": "
      << probe::kMaxPagedKVProbeBlocks << "\n";
  out << "  },\n";
  out << "  \"no_prefix_cache\": "
      << probe::allocation_stats_json(plan.no_prefix_stats, 2) << ",\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"savings\": {\n";
  out << "    \"reserved_bytes_delta\": "
      << plan.reserved_bytes_delta << ",\n";
  out << "    \"reserved_bytes_ratio\": " << std::fixed
      << std::setprecision(6) << plan.reserved_bytes_ratio << "\n";
  out << "  },\n";
  out << "  \"shared_prefix\": "
      << probe::allocation_stats_json(plan.shared_prefix_stats, 2) << ",\n";
  out << "  \"traffic_profile\": {\n";
  out << "    \"block_size_tokens\": " << profile.block_size_tokens << ",\n";
  out << "    \"bytes_per_token\": " << profile.bytes_per_token << ",\n";
  out << "    \"decode_attention_kv_read_bytes_per_step\": "
      << profile.decode_attention_kv_read_bytes_per_step << ",\n";
  out << "    \"decode_attention_shared_prefix_read_bytes_per_step\": "
      << profile.decode_attention_shared_prefix_read_bytes_per_step << ",\n";
  out << "    \"no_prefix_physical_tokens\": "
      << profile.no_prefix_physical_tokens << ",\n";
  out << "    \"no_prefix_prefill_write_bytes\": "
      << profile.no_prefix_prefill_write_bytes << ",\n";
  out << "    \"no_prefix_request_block_table_entries\": "
      << profile.no_prefix_request_block_table_entries << ",\n";
  out << "    \"no_prefix_reserved_bytes\": "
      << profile.no_prefix_reserved_bytes << ",\n";
  out << "    \"num_requests\": " << profile.num_requests << ",\n";
  out << "    \"physical_tokens_saved\": "
      << profile.physical_tokens_saved << ",\n";
  out << "    \"prefix_blocks\": " << profile.prefix_blocks << ",\n";
  out << "    \"prefix_tokens\": " << profile.prefix_tokens << ",\n";
  out << "    \"prefix_write_bytes_saved\": "
      << profile.prefix_write_bytes_saved << ",\n";
  out << "    \"reserved_bytes_saved\": "
      << profile.reserved_bytes_saved << ",\n";
  out << "    \"shared_prefix_entry_block_table_entries\": "
      << profile.shared_prefix_entry_block_table_entries << ",\n";
  out << "    \"shared_prefix_physical_tokens\": "
      << profile.shared_prefix_physical_tokens << ",\n";
  out << "    \"shared_prefix_prefill_write_bytes\": "
      << profile.shared_prefix_prefill_write_bytes << ",\n";
  out << "    \"shared_prefix_reserved_bytes\": "
      << profile.shared_prefix_reserved_bytes << ",\n";
  out << "    \"shared_request_block_table_entries\": "
      << profile.shared_request_block_table_entries << ",\n";
  out << "    \"shared_reserved_ratio\": " << std::fixed
      << std::setprecision(6) << profile.shared_reserved_ratio << ",\n";
  out << "    \"suffix_blocks_per_request\": "
      << profile.suffix_blocks_per_request << ",\n";
  out << "    \"suffix_tokens_per_request\": "
      << profile.suffix_tokens_per_request << ",\n";
  out << "    \"total_tokens_per_request\": "
      << profile.total_tokens_per_request << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::prefix_cache_compare
