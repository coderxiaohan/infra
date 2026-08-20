#pragma once

#include "inferlab/edge_runtime.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inferlab {

struct EdgeProcessMemorySnapshot {
  std::optional<std::uint64_t> current_pss_bytes;
  std::optional<std::uint64_t> process_lifetime_peak_rss_bytes;
};

struct EdgeThermalSnapshot {
  std::optional<double> maximum_temperature_c;
  std::size_t readable_sensor_count = 0U;
  std::string status;
  std::string provenance;
};

// Internal identity and capture timestamps are retained only to reconcile two
// process-local sysfs snapshots. They must not be rendered into an artifact.
struct EdgeEnergyZoneSnapshot {
  std::string identity;
  std::uint64_t energy_uj = 0U;
  std::uint64_t maximum_energy_range_uj = 0U;
  std::uint64_t maximum_power_range_uw = 0U;
};

struct EdgeEnergySnapshot {
  std::vector<EdgeEnergyZoneSnapshot> root_zones;
  std::chrono::steady_clock::time_point capture_started_at;
  std::chrono::steady_clock::time_point capture_completed_at;
  std::string status;
  std::string provenance;
};

struct EdgeEnergyIntervalObservation {
  std::optional<std::uint64_t> consumed_energy_uj;
  std::size_t root_zone_count = 0U;
  std::string before_status;
  std::string after_status;
  std::string status;
  std::string provenance;
};

// Process-lifetime peak RSS, not a per-request delta or PSS measurement.
std::optional<std::uint64_t> edge_process_peak_rss_bytes() noexcept;
EdgeProcessMemorySnapshot edge_process_memory_snapshot() noexcept;

// A bounded point-in-time observation of Linux thermal-zone temperatures.
// It does not infer throttling, power, energy, or device-wide thermal fitness.
EdgeThermalSnapshot edge_thermal_snapshot() noexcept;

// Bounded Linux powercap root-zone energy and power-range counters. Root zones
// are used so a parent and its child zones are never summed together.
EdgeEnergySnapshot edge_energy_snapshot() noexcept;

// Checked whole-system power-zone delta across one caller-defined interval.
// A steady-clock/power-range bound must rule out hidden counter wrap; reset,
// inventory drift and malformed snapshots fail closed.
EdgeEnergyIntervalObservation edge_energy_interval_observation(
    const EdgeEnergySnapshot& before,
    const EdgeEnergySnapshot& after) noexcept;

// Linux /proc virtual mappings that match one current physical file by the
// kernel-rendered path plus device and inode identity.
// A returned zero is an observed no-mapping state; null means unavailable.
std::optional<std::uint64_t> edge_mapped_file_bytes(
    const std::filesystem::path& file_path) noexcept;

class EdgeProcessMetricTracker {
 public:
  void reset() noexcept;
  void observe() noexcept;
  // Replaces the point-in-time mapping observation, including with null when
  // the latest checkpoint is unavailable. RSS/PSS remain checkpoint maxima.
  void bind_mapped_file(const std::filesystem::path& file_path) noexcept;

  const std::optional<std::uint64_t>& process_lifetime_peak_rss_bytes()
      const noexcept;
  const std::optional<std::uint64_t>& sampled_peak_pss_bytes()
      const noexcept;
  const std::optional<std::uint64_t>& mapped_file_bytes() const noexcept;

 private:
  std::optional<std::uint64_t> process_lifetime_peak_rss_bytes_;
  std::optional<std::uint64_t> sampled_peak_pss_bytes_;
  std::optional<std::uint64_t> mapped_file_bytes_;
};

void project_edge_process_metrics(const EdgeProcessMetricTracker& tracker,
                                  EdgeRuntimeMetrics* metrics) noexcept;

// Samples and projects process observations before a generation result is
// moved or copied into the caller. It owns no runtime resources or host paths.
class EdgeProcessMetricFinalizer {
 public:
  EdgeProcessMetricFinalizer(EdgeProcessMetricTracker* tracker,
                             std::filesystem::path mapped_file);

  EdgeProcessMetricFinalizer(const EdgeProcessMetricFinalizer&) = delete;
  EdgeProcessMetricFinalizer& operator=(
      const EdgeProcessMetricFinalizer&) = delete;

  EdgeGenerationResult finish(EdgeGenerationResult result) noexcept;

 private:
  EdgeProcessMetricTracker* tracker_ = nullptr;
  std::filesystem::path mapped_file_;
};

}  // namespace inferlab
