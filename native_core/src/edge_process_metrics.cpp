#include "edge_process_metrics.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
#if defined(__linux__)
#include <sys/stat.h>
#include <sys/sysmacros.h>
#endif

namespace inferlab {
namespace {

constexpr std::size_t kMaximumProcTextBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumThermalZones = 256U;
constexpr std::size_t kMaximumThermalValueBytes = 64U;
constexpr std::size_t kMaximumPowercapInventoryEntries = 256U;
constexpr std::size_t kMaximumPowercapRootZones = 64U;
constexpr std::size_t kMaximumPowercapValueBytes = 64U;
constexpr const char* kPowercapSnapshotProvenance =
    "linux_sysfs_powercap_root_zone_energy_uj";
constexpr const char* kPowercapIntervalProvenance =
    "linux_sysfs_powercap_root_zone_energy_uj_lifecycle_delta";

bool consume_proc_text_line(const std::size_t line_bytes,
                            const bool delimiter_consumed,
                            std::size_t* consumed) noexcept {
  if (consumed == nullptr || *consumed > kMaximumProcTextBytes) return false;
  std::size_t remaining = kMaximumProcTextBytes - *consumed;
  if (line_bytes > remaining) return false;
  remaining -= line_bytes;
  if (delimiter_consumed && remaining == 0U) return false;
  *consumed += line_bytes + (delimiter_consumed ? 1U : 0U);
  return true;
}

std::optional<std::uint64_t> checked_kib_to_bytes(
    const std::uint64_t kibibytes) noexcept {
  if (kibibytes > std::numeric_limits<std::uint64_t>::max() / 1024U) {
    return std::nullopt;
  }
  return kibibytes * 1024U;
}

std::optional<std::uint64_t> parse_smaps_kib(
    const std::string& line, const char* expected_key) noexcept {
  try {
    std::istringstream input(line);
    std::string key;
    std::uint64_t value = 0U;
    std::string unit;
    if (!(input >> key >> value >> unit) ||
        key != std::string(expected_key) + ":" || unit != "kB") {
      return std::nullopt;
    }
    std::string trailing;
    if (input >> trailing) return std::nullopt;
    return checked_kib_to_bytes(value);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::uint64_t> parse_hex_u64(
    const std::string& value) noexcept {
  std::uint64_t parsed = 0U;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto result = std::from_chars(begin, end, parsed, 16);
  if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
  return parsed;
}

std::optional<std::uint64_t> parse_decimal_u64(
    const std::string& value) noexcept {
  std::uint64_t parsed = 0U;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
  return parsed;
}

std::optional<std::uint64_t> read_bounded_decimal_file(
    const std::filesystem::path& path) noexcept {
  try {
    std::ifstream input(path);
    std::string value;
    if (!input || !std::getline(input, value) || value.empty() ||
        value.size() > kMaximumPowercapValueBytes ||
        input.peek() != std::char_traits<char>::eof()) {
      return std::nullopt;
    }
    return parse_decimal_u64(value);
  } catch (...) {
    return std::nullopt;
  }
}

enum class PowercapZoneProbe {
  kNotZone,
  kObserved,
  kProbeError,
  kPowerBoundMissing,
  kMalformed,
};

PowercapZoneProbe probe_powercap_zone(
    const std::filesystem::path& path,
    EdgeEnergyZoneSnapshot* zone) noexcept {
  if (zone == nullptr) return PowercapZoneProbe::kProbeError;
  std::error_code error;
  const auto energy_path = path / "energy_uj";
  const bool has_energy = std::filesystem::exists(energy_path, error);
  if (error) return PowercapZoneProbe::kProbeError;
  if (!has_energy) return PowercapZoneProbe::kNotZone;
  const auto maximum_path = path / "max_energy_range_uj";
  if (!std::filesystem::exists(maximum_path, error) || error) {
    return error ? PowercapZoneProbe::kProbeError
                 : PowercapZoneProbe::kMalformed;
  }
  const auto maximum_power_path = path / "max_power_range_uw";
  if (!std::filesystem::exists(maximum_power_path, error) || error) {
    return error ? PowercapZoneProbe::kProbeError
                 : PowercapZoneProbe::kPowerBoundMissing;
  }
  const auto energy = read_bounded_decimal_file(energy_path);
  const auto maximum = read_bounded_decimal_file(maximum_path);
  const auto maximum_power = read_bounded_decimal_file(maximum_power_path);
  if (!energy.has_value() || !maximum.has_value() || *maximum == 0U ||
      !maximum_power.has_value() || *maximum_power == 0U ||
      *energy > *maximum) {
    return PowercapZoneProbe::kMalformed;
  }
  const auto canonical = std::filesystem::canonical(path, error);
  if (error || canonical.empty()) return PowercapZoneProbe::kProbeError;
  zone->identity = canonical.string();
  zone->energy_uj = *energy;
  zone->maximum_energy_range_uj = *maximum;
  zone->maximum_power_range_uw = *maximum_power;
  return PowercapZoneProbe::kObserved;
}

bool collect_direct_powercap_zones(
    const std::filesystem::path& container, const bool inspect_control_types,
    std::size_t* inspected,
    std::vector<EdgeEnergyZoneSnapshot>* candidates,
    std::string* failure_status) noexcept {
  if (inspected == nullptr || candidates == nullptr ||
      failure_status == nullptr) {
    return false;
  }
  try {
    std::error_code error;
    std::filesystem::directory_iterator iterator(container, error);
    const std::filesystem::directory_iterator end;
    if (error) {
      *failure_status = "unavailable_powercap_sysfs_probe_error";
      return false;
    }
    for (; iterator != end; iterator.increment(error)) {
      if (error) {
        *failure_status = "unavailable_powercap_sysfs_probe_error";
        return false;
      }
      if (*inspected >= kMaximumPowercapInventoryEntries) {
        *failure_status = "unavailable_powercap_inventory_limit_reached";
        return false;
      }
      std::error_code type_error;
      if (!iterator->is_directory(type_error)) {
        if (type_error) {
          *failure_status = "unavailable_powercap_sysfs_probe_error";
          return false;
        }
        continue;
      }
      ++*inspected;
      EdgeEnergyZoneSnapshot zone;
      const auto probe = probe_powercap_zone(iterator->path(), &zone);
      if (probe == PowercapZoneProbe::kObserved) {
        candidates->push_back(std::move(zone));
        if (candidates->size() > kMaximumPowercapInventoryEntries) {
          *failure_status = "unavailable_powercap_inventory_limit_reached";
          return false;
        }
      } else if (probe == PowercapZoneProbe::kProbeError) {
        *failure_status = "unavailable_powercap_sysfs_probe_error";
        return false;
      } else if (probe == PowercapZoneProbe::kPowerBoundMissing) {
        *failure_status = "unavailable_powercap_power_bound_missing";
        return false;
      } else if (probe == PowercapZoneProbe::kMalformed) {
        *failure_status = "unavailable_powercap_counter_malformed";
        return false;
      } else if (inspect_control_types &&
                 !collect_direct_powercap_zones(
                     iterator->path(), false, inspected, candidates,
                     failure_status)) {
        return false;
      }
    }
    if (error) {
      *failure_status = "unavailable_powercap_sysfs_probe_error";
      return false;
    }
    return true;
  } catch (...) {
    *failure_status = "unavailable_powercap_sysfs_exception";
    return false;
  }
}

bool path_is_strict_ancestor(const std::filesystem::path& ancestor,
                             const std::filesystem::path& descendant) {
  auto ancestor_part = ancestor.begin();
  auto descendant_part = descendant.begin();
  for (; ancestor_part != ancestor.end() &&
         descendant_part != descendant.end();
       ++ancestor_part, ++descendant_part) {
    if (*ancestor_part != *descendant_part) return false;
  }
  return ancestor_part == ancestor.end() &&
         descendant_part != descendant.end();
}

std::optional<bool> has_powercap_energy_ancestor(
    const std::filesystem::path& canonical_zone) noexcept {
  try {
    std::filesystem::path parent = canonical_zone.parent_path();
    for (std::size_t depth = 0U; depth < 64U && !parent.empty(); ++depth) {
      std::error_code error;
      if (std::filesystem::exists(parent / "energy_uj", error)) return true;
      if (error) return std::nullopt;
      const auto next = parent.parent_path();
      if (next == parent) return false;
      parent = next;
    }
    if (parent.empty()) return false;
    return std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

bool same_energy_zone_identity(const EdgeEnergyZoneSnapshot& lhs,
                               const EdgeEnergyZoneSnapshot& rhs) noexcept {
  return lhs.identity == rhs.identity &&
         lhs.maximum_energy_range_uj == rhs.maximum_energy_range_uj &&
         lhs.maximum_power_range_uw == rhs.maximum_power_range_uw;
}

bool energy_snapshot_structurally_valid(
    const EdgeEnergySnapshot& snapshot) noexcept {
  if (snapshot.status != "observed_linux_powercap_root_zones" ||
      snapshot.provenance != kPowercapSnapshotProvenance ||
      snapshot.root_zones.empty() ||
      snapshot.root_zones.size() > kMaximumPowercapRootZones ||
      snapshot.capture_completed_at < snapshot.capture_started_at) {
    return false;
  }
  std::string previous_identity;
  for (const auto& zone : snapshot.root_zones) {
    if (zone.identity.empty() || zone.maximum_energy_range_uj == 0U ||
        zone.maximum_power_range_uw == 0U ||
        zone.energy_uj > zone.maximum_energy_range_uj ||
        (!previous_identity.empty() && zone.identity <= previous_identity)) {
      return false;
    }
    previous_identity = zone.identity;
  }
  return true;
}

std::optional<std::uint64_t> checked_maximum_interval_energy_uj(
    const std::uint64_t maximum_power_uw,
    const std::uint64_t duration_ns) noexcept {
  constexpr std::uint64_t kNanosecondsPerSecond = 1000000000U;
  const std::uint64_t whole_seconds = duration_ns / kNanosecondsPerSecond;
  const std::uint64_t remaining_ns = duration_ns % kNanosecondsPerSecond;
  if (whole_seconds != 0U &&
      maximum_power_uw >
          std::numeric_limits<std::uint64_t>::max() / whole_seconds) {
    return std::nullopt;
  }
  std::uint64_t total = maximum_power_uw * whole_seconds;
  if (remaining_ns != 0U) {
    if (maximum_power_uw >
        std::numeric_limits<std::uint64_t>::max() / remaining_ns) {
      return std::nullopt;
    }
    const std::uint64_t fractional_product =
        maximum_power_uw * remaining_ns;
    std::uint64_t fractional_uj =
        fractional_product / kNanosecondsPerSecond;
    if (fractional_product % kNanosecondsPerSecond != 0U) {
      ++fractional_uj;
    }
    if (fractional_uj >
        std::numeric_limits<std::uint64_t>::max() - total) {
      return std::nullopt;
    }
    total += fractional_uj;
  }
  return total;
}

std::optional<double> parse_millidegrees_celsius(
    const std::string& value) noexcept {
  if (value.empty() || value.size() > kMaximumThermalValueBytes) {
    return std::nullopt;
  }
  std::int64_t parsed = 0;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc{} || result.ptr != end ||
      parsed < -273150 || parsed > 1000000) {
    return std::nullopt;
  }
  const double celsius = static_cast<double>(parsed) / 1000.0;
  return std::isfinite(celsius) ? std::optional<double>(celsius)
                               : std::nullopt;
}

struct ProcDeviceIdentity {
  std::uint64_t major = 0U;
  std::uint64_t minor = 0U;
};

std::optional<ProcDeviceIdentity> parse_proc_device(
    const std::string& value) noexcept {
  const auto separator = value.find(':');
  if (separator == std::string::npos ||
      value.find(':', separator + 1U) != std::string::npos) {
    return std::nullopt;
  }
  const auto device_major = parse_hex_u64(value.substr(0U, separator));
  const auto device_minor = parse_hex_u64(value.substr(separator + 1U));
  if (!device_major.has_value() || !device_minor.has_value()) {
    return std::nullopt;
  }
  return ProcDeviceIdentity{*device_major, *device_minor};
}

std::optional<std::string> proc_maps_encoded_path(
    const std::string& path) noexcept {
  try {
    std::string encoded;
    encoded.reserve(path.size());
    for (const char value : path) {
      if (value == '\n') {
        encoded += "\\012";
      } else {
        encoded.push_back(value);
      }
    }
    return encoded;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::uint64_t> read_smaps_rollup_pss() noexcept {
  try {
    std::ifstream input("/proc/self/smaps_rollup");
    if (!input) return std::nullopt;
    std::size_t consumed = 0U;
    std::optional<std::uint64_t> pss_bytes;
    std::string line;
    while (std::getline(input, line)) {
      if (!consume_proc_text_line(line.size(), !input.eof(), &consumed)) {
        return std::nullopt;
      }
      if (line.rfind("Pss:", 0U) != 0U) continue;
      if (pss_bytes.has_value()) return std::nullopt;
      pss_bytes = parse_smaps_kib(line, "Pss");
      if (!pss_bytes.has_value()) return std::nullopt;
    }
    if (!input.eof() || !pss_bytes.has_value() || *pss_bytes == 0U) {
      return std::nullopt;
    }
    return pss_bytes;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::uint64_t> read_smaps_total_pss() noexcept {
  try {
    std::ifstream input("/proc/self/smaps");
    if (!input) return std::nullopt;
    std::size_t consumed = 0U;
    std::size_t pss_line_count = 0U;
    std::uint64_t total = 0U;
    std::string line;
    while (std::getline(input, line)) {
      if (!consume_proc_text_line(line.size(), !input.eof(), &consumed)) {
        return std::nullopt;
      }
      if (line.rfind("Pss:", 0U) != 0U) continue;
      const auto mapping_pss = parse_smaps_kib(line, "Pss");
      if (!mapping_pss.has_value() ||
          *mapping_pss > std::numeric_limits<std::uint64_t>::max() - total) {
        return std::nullopt;
      }
      total += *mapping_pss;
      ++pss_line_count;
    }
    if (!input.eof() || pss_line_count == 0U || total == 0U) {
      return std::nullopt;
    }
    return total;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace

std::optional<std::uint64_t> edge_process_peak_rss_bytes() noexcept {
#if defined(__linux__) || defined(__APPLE__)
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) {
    return std::nullopt;
  }
  const auto raw = static_cast<std::uint64_t>(usage.ru_maxrss);
#if defined(__APPLE__)
  return raw;
#else
  if (raw > std::numeric_limits<std::uint64_t>::max() / 1024U) {
    return std::nullopt;
  }
  return raw * 1024U;
#endif
#else
  return std::nullopt;
#endif
}

EdgeProcessMemorySnapshot edge_process_memory_snapshot() noexcept {
  EdgeProcessMemorySnapshot snapshot;
  snapshot.process_lifetime_peak_rss_bytes = edge_process_peak_rss_bytes();
#if defined(__linux__)
  snapshot.current_pss_bytes = read_smaps_rollup_pss();
  if (!snapshot.current_pss_bytes.has_value()) {
    snapshot.current_pss_bytes = read_smaps_total_pss();
  }
#endif
  return snapshot;
}

EdgeThermalSnapshot edge_thermal_snapshot() noexcept {
  EdgeThermalSnapshot snapshot;
#if defined(__linux__)
  snapshot.status = "unavailable_no_readable_thermal_zone";
  snapshot.provenance = "linux_sysfs_thermal_zone_temp_point_in_time";
  try {
    const std::filesystem::path root("/sys/class/thermal");
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
      snapshot.status = error ? "unavailable_thermal_sysfs_probe_error"
                              : "unavailable_thermal_sysfs_missing";
      return snapshot;
    }
    std::size_t inspected = 0U;
    std::filesystem::directory_iterator iterator(root, error);
    const std::filesystem::directory_iterator end;
    if (error) {
      snapshot.status = "unavailable_thermal_sysfs_probe_error";
      return snapshot;
    }
    for (; iterator != end && inspected < kMaximumThermalZones;
         iterator.increment(error)) {
      if (error) {
        snapshot.maximum_temperature_c.reset();
        snapshot.readable_sensor_count = 0U;
        snapshot.status = "unavailable_thermal_sysfs_probe_error";
        return snapshot;
      }
      const std::string name = iterator->path().filename().string();
      if (name.rfind("thermal_zone", 0U) != 0U) continue;
      ++inspected;
      std::ifstream input(iterator->path() / "temp");
      std::string value;
      if (!input || !std::getline(input, value) || value.empty() ||
          value.size() > kMaximumThermalValueBytes) {
        continue;
      }
      if (input.peek() != std::char_traits<char>::eof()) continue;
      const auto temperature = parse_millidegrees_celsius(value);
      if (!temperature.has_value()) continue;
      ++snapshot.readable_sensor_count;
      if (!snapshot.maximum_temperature_c.has_value() ||
          *temperature > *snapshot.maximum_temperature_c) {
        snapshot.maximum_temperature_c = temperature;
      }
    }
    if (error) {
      snapshot.maximum_temperature_c.reset();
      snapshot.readable_sensor_count = 0U;
      snapshot.status = "unavailable_thermal_sysfs_probe_error";
    } else if (inspected == kMaximumThermalZones && iterator != end) {
      snapshot.maximum_temperature_c.reset();
      snapshot.readable_sensor_count = 0U;
      snapshot.status = "unavailable_thermal_zone_limit_reached";
    } else if (snapshot.maximum_temperature_c.has_value()) {
      snapshot.status = "observed_linux_thermal_zone_max";
    }
  } catch (...) {
    snapshot.maximum_temperature_c.reset();
    snapshot.readable_sensor_count = 0U;
    snapshot.status = "unavailable_thermal_sysfs_exception";
  }
#else
  snapshot.status = "unavailable_non_linux";
  snapshot.provenance = "unavailable_non_linux";
#endif
  return snapshot;
}

EdgeEnergySnapshot edge_energy_snapshot() noexcept {
  EdgeEnergySnapshot snapshot;
  snapshot.capture_started_at = std::chrono::steady_clock::now();
#if defined(__linux__)
  snapshot.status = "unavailable_no_readable_powercap_zone";
  snapshot.provenance = kPowercapSnapshotProvenance;
  try {
    const std::filesystem::path roots[] = {
        "/sys/class/powercap", "/sys/class/power_cap"};
    std::filesystem::path root;
    for (const auto& candidate : roots) {
      std::error_code error;
      const bool directory = std::filesystem::is_directory(candidate, error);
      if (error) {
        snapshot.status = "unavailable_powercap_sysfs_probe_error";
        return snapshot;
      }
      if (directory) {
        root = candidate;
        break;
      }
    }
    if (root.empty()) {
      snapshot.status = "unavailable_powercap_sysfs_missing";
      return snapshot;
    }

    std::size_t inspected = 0U;
    std::vector<EdgeEnergyZoneSnapshot> candidates;
    if (!collect_direct_powercap_zones(root, false, &inspected, &candidates,
                                       &snapshot.status)) {
      return snapshot;
    }
    if (candidates.empty()) {
      inspected = 0U;
      if (!collect_direct_powercap_zones(root, true, &inspected, &candidates,
                                         &snapshot.status)) {
        return snapshot;
      }
    }

    std::set<std::string> identities;
    for (const auto& candidate : candidates) {
      if (!identities.insert(candidate.identity).second) {
        snapshot.status = "unavailable_powercap_zone_identity_duplicate";
        return snapshot;
      }
    }
    for (const auto& candidate : candidates) {
      const std::filesystem::path candidate_path(candidate.identity);
      const auto physical_parent_zone =
          has_powercap_energy_ancestor(candidate_path);
      if (!physical_parent_zone.has_value()) {
        snapshot.root_zones.clear();
        snapshot.status = "unavailable_powercap_sysfs_probe_error";
        return snapshot;
      }
      const bool candidate_parent_zone = std::any_of(
          candidates.begin(), candidates.end(),
          [&candidate, &candidate_path](const auto& other) {
            return other.identity != candidate.identity &&
                   path_is_strict_ancestor(
                       std::filesystem::path(other.identity), candidate_path);
          });
      if (!*physical_parent_zone && !candidate_parent_zone) {
        snapshot.root_zones.push_back(candidate);
      }
    }
    if (snapshot.root_zones.empty()) return snapshot;
    if (snapshot.root_zones.size() > kMaximumPowercapRootZones) {
      snapshot.root_zones.clear();
      snapshot.status = "unavailable_powercap_root_zone_limit_reached";
      return snapshot;
    }
    std::sort(snapshot.root_zones.begin(), snapshot.root_zones.end(),
              [](const auto& lhs, const auto& rhs) {
                return lhs.identity < rhs.identity;
              });
    snapshot.capture_completed_at = std::chrono::steady_clock::now();
    snapshot.status = "observed_linux_powercap_root_zones";
  } catch (...) {
    snapshot.root_zones.clear();
    snapshot.status = "unavailable_powercap_sysfs_exception";
  }
#else
  snapshot.status = "unavailable_non_linux";
  snapshot.provenance = "unavailable_non_linux";
#endif
  return snapshot;
}

EdgeEnergyIntervalObservation edge_energy_interval_observation(
    const EdgeEnergySnapshot& before,
    const EdgeEnergySnapshot& after) noexcept {
  EdgeEnergyIntervalObservation observation;
  observation.before_status = before.status;
  observation.after_status = after.status;
  observation.provenance = kPowercapIntervalProvenance;
  try {
    if (!energy_snapshot_structurally_valid(before) ||
        !energy_snapshot_structurally_valid(after)) {
      observation.status = "unavailable_powercap_snapshot";
      if (before.provenance == "unavailable_non_linux" &&
          after.provenance == "unavailable_non_linux") {
        observation.provenance = "unavailable_non_linux";
      }
      return observation;
    }
    if (before.root_zones.empty() ||
        before.root_zones.size() != after.root_zones.size()) {
      observation.status = "unavailable_powercap_zone_set_changed";
      return observation;
    }
    std::uint64_t total = 0U;
    if (after.capture_completed_at < before.capture_started_at) {
      observation.status = "unavailable_powercap_duration_invalid";
      return observation;
    }
    const auto raw_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        after.capture_completed_at - before.capture_started_at).count();
    if (raw_duration < 0) {
      observation.status = "unavailable_powercap_duration_invalid";
      return observation;
    }
    const auto duration_ns = static_cast<std::uint64_t>(raw_duration);
    for (std::size_t index = 0U; index < before.root_zones.size(); ++index) {
      const auto& begin = before.root_zones[index];
      const auto& end = after.root_zones[index];
      if (begin.identity != end.identity) {
        observation.status = "unavailable_powercap_zone_set_changed";
        return observation;
      }
      if (!same_energy_zone_identity(begin, end)) {
        observation.status = "unavailable_powercap_counter_range_changed";
        return observation;
      }
      if (end.energy_uj < begin.energy_uj) {
        observation.status =
            "unavailable_powercap_counter_wrapped_or_reset";
        return observation;
      }
      const auto maximum_interval_energy =
          checked_maximum_interval_energy_uj(begin.maximum_power_range_uw,
                                             duration_ns);
      if (!maximum_interval_energy.has_value()) {
        observation.status =
            "unavailable_powercap_wrap_bound_overflow";
        return observation;
      }
      const std::uint64_t remaining_counter_range =
          begin.maximum_energy_range_uj - begin.energy_uj;
      if (end.energy_uj >
          std::numeric_limits<std::uint64_t>::max() -
              remaining_counter_range) {
        observation.status =
            "unavailable_powercap_wrap_bound_overflow";
        return observation;
      }
      const std::uint64_t one_wrap_energy =
          remaining_counter_range + end.energy_uj;
      if (*maximum_interval_energy >= one_wrap_energy) {
        observation.status = "unavailable_powercap_wrap_ambiguous";
        return observation;
      }
      const std::uint64_t delta = end.energy_uj - begin.energy_uj;
      if (delta > *maximum_interval_energy) {
        observation.status =
            "unavailable_powercap_counter_rate_exceeded";
        return observation;
      }
      if (delta > std::numeric_limits<std::uint64_t>::max() - total) {
        observation.status = "unavailable_powercap_delta_overflow";
        return observation;
      }
      total += delta;
    }
    observation.consumed_energy_uj = total;
    observation.root_zone_count = before.root_zones.size();
    observation.status = "observed_linux_powercap_lifecycle_delta";
  } catch (...) {
    observation.consumed_energy_uj.reset();
    observation.root_zone_count = 0U;
    observation.status = "unavailable_powercap_interval_exception";
  }
  return observation;
}

std::optional<std::uint64_t> edge_mapped_file_bytes(
    const std::filesystem::path& file_path) noexcept {
#if defined(__linux__)
  try {
    std::error_code error;
    const auto canonical_path =
        std::filesystem::weakly_canonical(file_path, error);
    if (error || canonical_path.empty()) return std::nullopt;
    struct stat file_status {};
    if (::stat(canonical_path.c_str(), &file_status) != 0) {
      return std::nullopt;
    }
    const auto expected_path =
        proc_maps_encoded_path(canonical_path.string());
    if (!expected_path.has_value()) return std::nullopt;
    const ProcDeviceIdentity expected_device{
        static_cast<std::uint64_t>(major(file_status.st_dev)),
        static_cast<std::uint64_t>(minor(file_status.st_dev))};
    const auto expected_inode =
        static_cast<std::uint64_t>(file_status.st_ino);
    std::ifstream input("/proc/self/maps");
    if (!input) return std::nullopt;
    std::uint64_t total = 0U;
    std::size_t consumed = 0U;
    std::string line;
    while (std::getline(input, line)) {
      if (!consume_proc_text_line(line.size(), !input.eof(), &consumed)) {
        return std::nullopt;
      }
      std::istringstream fields(line);
      std::string range;
      std::string permissions;
      std::string offset;
      std::string device;
      std::string inode;
      if (!(fields >> range >> permissions >> offset >> device >> inode)) {
        return std::nullopt;
      }
      std::string mapped_path;
      std::getline(fields, mapped_path);
      const auto first = mapped_path.find_first_not_of(" \t");
      if (first == std::string::npos) continue;
      mapped_path.erase(0U, first);
      if (mapped_path != *expected_path) continue;
      const auto mapped_device = parse_proc_device(device);
      const auto mapped_inode = parse_decimal_u64(inode);
      if (!mapped_device.has_value() || !mapped_inode.has_value()) {
        return std::nullopt;
      }
      if (mapped_device->major != expected_device.major ||
          mapped_device->minor != expected_device.minor ||
          *mapped_inode != expected_inode) {
        continue;
      }
      const auto separator = range.find('-');
      if (separator == std::string::npos ||
          range.find('-', separator + 1U) != std::string::npos) {
        return std::nullopt;
      }
      const auto begin = parse_hex_u64(range.substr(0U, separator));
      const auto end = parse_hex_u64(range.substr(separator + 1U));
      if (!begin.has_value() || !end.has_value() || *end < *begin ||
          *end - *begin > std::numeric_limits<std::uint64_t>::max() - total) {
        return std::nullopt;
      }
      total += *end - *begin;
    }
    return total;
  } catch (...) {
    return std::nullopt;
  }
#else
  (void)file_path;
  return std::nullopt;
#endif
}

void EdgeProcessMetricTracker::reset() noexcept {
  process_lifetime_peak_rss_bytes_.reset();
  sampled_peak_pss_bytes_.reset();
  mapped_file_bytes_.reset();
}

void EdgeProcessMetricTracker::observe() noexcept {
  const auto snapshot = edge_process_memory_snapshot();
  if (snapshot.process_lifetime_peak_rss_bytes.has_value() &&
      (!process_lifetime_peak_rss_bytes_.has_value() ||
       *snapshot.process_lifetime_peak_rss_bytes >
           *process_lifetime_peak_rss_bytes_)) {
    process_lifetime_peak_rss_bytes_ =
        snapshot.process_lifetime_peak_rss_bytes;
  }
  if (snapshot.current_pss_bytes.has_value() &&
      (!sampled_peak_pss_bytes_.has_value() ||
       *snapshot.current_pss_bytes > *sampled_peak_pss_bytes_)) {
    sampled_peak_pss_bytes_ = snapshot.current_pss_bytes;
  }
}

void EdgeProcessMetricTracker::bind_mapped_file(
    const std::filesystem::path& file_path) noexcept {
  mapped_file_bytes_ = edge_mapped_file_bytes(file_path);
}

const std::optional<std::uint64_t>&
EdgeProcessMetricTracker::process_lifetime_peak_rss_bytes() const noexcept {
  return process_lifetime_peak_rss_bytes_;
}

const std::optional<std::uint64_t>&
EdgeProcessMetricTracker::sampled_peak_pss_bytes() const noexcept {
  return sampled_peak_pss_bytes_;
}

const std::optional<std::uint64_t>&
EdgeProcessMetricTracker::mapped_file_bytes() const noexcept {
  return mapped_file_bytes_;
}

void project_edge_process_metrics(const EdgeProcessMetricTracker& tracker,
                                  EdgeRuntimeMetrics* metrics) noexcept {
  if (metrics == nullptr) return;
  metrics->peak_rss_bytes = tracker.process_lifetime_peak_rss_bytes();
  metrics->peak_pss_bytes = tracker.sampled_peak_pss_bytes();
  metrics->mapped_model_bytes = tracker.mapped_file_bytes();
}

EdgeProcessMetricFinalizer::EdgeProcessMetricFinalizer(
    EdgeProcessMetricTracker* tracker, std::filesystem::path mapped_file)
    : tracker_(tracker), mapped_file_(std::move(mapped_file)) {}

EdgeGenerationResult EdgeProcessMetricFinalizer::finish(
    EdgeGenerationResult result) noexcept {
  if (tracker_ != nullptr) {
    tracker_->observe();
    if (!mapped_file_.empty()) tracker_->bind_mapped_file(mapped_file_);
    project_edge_process_metrics(*tracker_, &result.metrics);
  }
  return result;
}

}  // namespace inferlab
