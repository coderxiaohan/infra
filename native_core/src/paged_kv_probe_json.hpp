#pragma once

#include "paged_kv_probe_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::probe {

inline std::string probe_json_indent(const int count) {
  if (count < 0 || count > 256) {
    throw std::invalid_argument(
        "native probe JSON indent must be between 0 and 256");
  }
  return std::string(static_cast<std::size_t>(count), ' ');
}

inline std::string probe_json_indent_offset(const int count,
                                            const int offset) {
  return probe_json_indent(internal::checked_add_int(
      count, offset, "native probe JSON indent"));
}

inline std::string block_table_json(const std::vector<BlockId>& blocks) {
  if (blocks.size() > static_cast<std::size_t>(kMaxPagedKVProbeBlocks)) {
    throw std::invalid_argument("Paged KV probe block table exceeds cap");
  }
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    if (blocks[i] < 0) {
      throw std::invalid_argument(
          "Paged KV probe block table contains a negative block id");
    }
    out << blocks[i] << (i + 1 == blocks.size() ? "" : ", ");
  }
  out << "]";
  return out.str();
}

inline std::string allocation_stats_json(const AllocationStats& stats,
                                         const int indent) {
  validate_paged_kv_probe_stats(stats, "Paged KV JSON stats");
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"active_requests\": " << stats.active_requests << ",\n";
  out << nested << "\"free_blocks\": " << stats.free_blocks << ",\n";
  out << nested << "\"reserved_bytes\": " << stats.reserved_bytes << ",\n";
  out << nested << "\"total_blocks\": " << stats.total_blocks << ",\n";
  out << nested << "\"total_tokens\": " << stats.total_tokens << ",\n";
  out << nested << "\"used_blocks\": " << stats.used_blocks << ",\n";
  out << nested << "\"used_bytes\": " << stats.used_bytes << ",\n";
  out << nested << "\"utilization\": " << std::fixed
      << std::setprecision(6) << stats.utilization << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string paged_kv_cache_config_json(
    const PagedKVCacheConfig& config, const int indent) {
  require_paged_kv_probe_capacity(config.num_blocks,
                                  "Paged KV probe config blocks");
  if (config.block_size_tokens <= 0 || config.bytes_per_token <= 0) {
    throw std::invalid_argument(
        "Paged KV probe config dimensions must be positive");
  }
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"block_size_tokens\": " << config.block_size_tokens
      << ",\n";
  out << nested << "\"bytes_per_token\": " << config.bytes_per_token
      << ",\n";
  out << nested << "\"num_blocks\": " << config.num_blocks << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab::probe
