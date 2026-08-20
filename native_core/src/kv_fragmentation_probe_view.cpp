#include "kv_fragmentation_probe_view.hpp"

#include "paged_kv_probe_json.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::kv_fragmentation_probe {
namespace {

std::string snapshot_json(const std::vector<Snapshot>& snapshots) {
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < snapshots.size(); ++i) {
    const auto& snapshot = snapshots[i];
    out << "    {\n";
    out << "      \"phase\": "
        << inferlab_plan::json_string(snapshot.phase) << ",\n";
    out << "      \"stats\": "
        << probe::allocation_stats_json(snapshot.stats, 6) << "\n";
    out << "    }" << (i + 1U == snapshots.size() ? "\n" : ",\n");
  }
  out << "  ]";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& after_holes = plan.snapshots[2].stats;
  const auto& after_reuse = plan.snapshots[3].stats;
  const auto& final_stats = plan.snapshots.back().stats;
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_kv_fragmentation_probe",
      "Executed deterministic scalar CPU Paged KV allocation/release/reuse "
      "transitions over a synthetic fixture only; not allocator concurrency, "
      "GPU fragmentation, latency, throughput, or benchmark evidence.");
  out << "  \"config\": "
      << probe::paged_kv_cache_config_json(plan.config, 2) << ",\n";
  out << "  \"fixture_limits\": {\n";
  out << "    \"max_paged_kv_probe_blocks\": "
      << probe::kMaxPagedKVProbeBlocks << "\n";
  out << "  },\n";
  out << "  \"fragmentation\": {\n";
  out << "    \"after_holes_free_blocks\": "
      << after_holes.free_blocks << ",\n";
  out << "    \"after_holes_used_blocks\": "
      << after_holes.used_blocks << ",\n";
  out << "    \"after_reuse_free_blocks\": "
      << after_reuse.free_blocks << ",\n";
  out << "    \"after_reuse_used_blocks\": "
      << after_reuse.used_blocks << ",\n";
  out << "    \"failed_allocations\": " << plan.failed_allocations
      << ",\n";
  out << "    \"final_free_blocks\": " << final_stats.free_blocks
      << ",\n";
  out << "    \"final_used_blocks\": " << final_stats.used_blocks
      << ",\n";
  out << "    \"peak_reserved_bytes\": " << plan.peak_reserved_bytes
      << ",\n";
  out << "    \"pressure_attempt_succeeded\": "
      << inferlab_plan::json_bool(plan.pressure_attempt_succeeded)
      << ",\n";
  out << "    \"reuse_request_block_table\": "
      << probe::block_table_json(plan.reuse_blocks) << ",\n";
  out << "    \"reused_released_block_count\": "
      << plan.reused_released_blocks << "\n";
  out << "  },\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"released_block_sample\": "
      << probe::block_table_json(plan.released_blocks) << ",\n";
  out << "  \"snapshots\": " << snapshot_json(plan.snapshots) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kv_fragmentation_probe
