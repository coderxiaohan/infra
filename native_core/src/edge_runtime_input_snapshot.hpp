#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace inferlab::edge_runtime_matrix {

struct EdgeRuntimeInputSnapshot {
  bool observed = false;
  std::uint64_t file_size_bytes = 0U;
  std::string metadata_source;
  // Internal comparison material. This value must never be rendered.
  std::string opaque_identity;
};

enum class EdgeRuntimeInputSnapshotComparison {
  kStable,
  kChanged,
  kUnavailable,
};

EdgeRuntimeInputSnapshot capture_edge_runtime_input_snapshot(
    const std::filesystem::path& path);

EdgeRuntimeInputSnapshotComparison compare_edge_runtime_input_snapshots(
    const EdgeRuntimeInputSnapshot& baseline,
    const EdgeRuntimeInputSnapshot& current) noexcept;

bool is_known_edge_runtime_input_snapshot_source(
    const std::string& source) noexcept;
bool is_observed_edge_runtime_input_snapshot_source(
    const std::string& source) noexcept;

}  // namespace inferlab::edge_runtime_matrix
